#include "stdafx.h"
#include <stdio.h>

#include "Illumination.h"
#include "GameComponent.h"
#include "GameTime.h"
#include "Camera.h"
#include "DirectionalLight.h"
#include "FullScreenRenderTarget.h"
#include "GameException.h"
#include "Model.h"
#include "Mesh.h"
#include "Game.h"
#include "MatrixHelper.h"
#include "MaterialHelper.h"
#include "Utility.h"
#include "VertexDeclarations.h"
#include "RasterizerStates.h"
#include "ShaderCompiler.h"
#include "VoxelizationGIMaterial.h"
#include "Scene.h"
#include "GBuffer.h"
#include "ShadowMapper.h"
#include "Foliage.h"
#include "IBLRadianceMap.h"
#include "RenderableAABB.h"

namespace Library {

	const float voxelCascadesSizes[NUM_VOXEL_GI_CASCADES] = { 256.0f, 256.0f };

	Illumination::Illumination(Game& game, Camera& camera, DirectionalLight& light, ShadowMapper& shadowMapper, const Scene* scene)
		: 
		GameComponent(game),
		mCamera(camera),
		mDirectionalLight(light),
		mShadowMapper(shadowMapper)
	{
		Initialize(scene);
	}

	Illumination::~Illumination()
	{
		ReleaseObject(mVCTMainCS);
		ReleaseObject(mUpsampleBlurCS);
		ReleaseObject(mVCTVoxelizationDebugVS);
		ReleaseObject(mVCTVoxelizationDebugGS);
		ReleaseObject(mVCTVoxelizationDebugPS);
		ReleaseObject(mDepthStencilStateRW);
		ReleaseObject(mLinearSamplerState);

		ReleaseObject(mIrradianceDiffuseTextureSRV);
		ReleaseObject(mIrradianceSpecularTextureSRV);
		ReleaseObject(mIntegrationMapTextureSRV);

		DeletePointerCollection(mVCTVoxelCascades3DRTs);
		DeletePointerCollection(mDebugVoxelZonesGizmos);
		DeleteObject(mVCTVoxelizationDebugRT);
		DeleteObject(mVCTMainRT);
		DeleteObject(mVCTUpsampleAndBlurRT);
		DeleteObject(mDepthBuffer);

		mVoxelizationDebugConstantBuffer.Release();
		mVoxelConeTracingConstantBuffer.Release();
		mUpsampleBlurConstantBuffer.Release();
	}

	void Illumination::Initialize(const Scene* scene)
	{
		//callbacks
		{
			for (auto& obj : scene->objects) {
				for (int voxelCascadeIndex = 0; voxelCascadeIndex < NUM_VOXEL_GI_CASCADES; voxelCascadeIndex++)
					obj.second->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::voxelizationGIMaterialName + "_" + std::to_string(voxelCascadeIndex),
						[&, voxelCascadeIndex](int meshIndex) { UpdateVoxelizationGIMaterialVariables(obj.second, meshIndex, voxelCascadeIndex); });
			}
		}
		
		//shaders
		{
			ID3DBlob* blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\GI\\VoxelConeTracingVoxelizationDebug.hlsl").c_str(), "VSMain", "vs_5_0", &blob)))
				throw GameException("Failed to load VSMain from shader: VoxelConeTracingVoxelization.hlsl!");
			if (FAILED(mGame->Direct3DDevice()->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mVCTVoxelizationDebugVS)))
				throw GameException("Failed to create vertex shader from VoxelConeTracingVoxelization.hlsl!");
			blob->Release();
			
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\GI\\VoxelConeTracingVoxelizationDebug.hlsl").c_str(), "GSMain", "gs_5_0", &blob)))
				throw GameException("Failed to load GSMain from shader: VoxelConeTracingVoxelization.hlsl!");
			if (FAILED(mGame->Direct3DDevice()->CreateGeometryShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mVCTVoxelizationDebugGS)))
				throw GameException("Failed to create geometry shader from VoxelConeTracingVoxelization.hlsl!");
			blob->Release();
			
			blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\GI\\VoxelConeTracingVoxelizationDebug.hlsl").c_str(), "PSMain", "ps_5_0", &blob)))
				throw GameException("Failed to load PSMain from shader: VoxelConeTracingVoxelization.hlsl!");
			if (FAILED(mGame->Direct3DDevice()->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mVCTVoxelizationDebugPS)))
				throw GameException("Failed to create pixel shader from VoxelConeTracingVoxelization.hlsl!");
			blob->Release();
			
			blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\GI\\VoxelConeTracingMain.hlsl").c_str(), "CSMain", "cs_5_0", &blob)))
				throw GameException("Failed to load CSMain from shader: VoxelConeTracingMain.hlsl!");
			if (FAILED(mGame->Direct3DDevice()->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mVCTMainCS)))
				throw GameException("Failed to create shader from VoxelConeTracingMain.hlsl!");
			blob->Release();

			blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\UpsampleBlur.hlsl").c_str(), "CSMain", "cs_5_0", &blob)))
				throw GameException("Failed to load CSMain from shader: UpsampleBlur.hlsl!");
			if (FAILED(mGame->Direct3DDevice()->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mUpsampleBlurCS)))
				throw GameException("Failed to create shader from UpsampleBlur.hlsl!");
			blob->Release();
		}

		//sampler states
		{
			D3D11_SAMPLER_DESC sam_desc;
			sam_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			sam_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
			sam_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
			sam_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
			sam_desc.MipLODBias = 0;
			sam_desc.MaxAnisotropy = 1;
			sam_desc.MinLOD = -1000.0f;
			sam_desc.MaxLOD = 1000.0f;
			if (FAILED(mGame->Direct3DDevice()->CreateSamplerState(&sam_desc, &mLinearSamplerState)))
				throw GameException("Failed to create sampler mLinearSamplerState!");

			CD3D11_DEPTH_STENCIL_DESC dsDesc((CD3D11_DEFAULT()));
			dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
			mGame->Direct3DDevice()->CreateDepthStencilState(&dsDesc, &mDepthStencilStateRW);
		}
		
		//cbuffers
		{
			mVoxelizationDebugConstantBuffer.Initialize(mGame->Direct3DDevice());
			mVoxelConeTracingConstantBuffer.Initialize(mGame->Direct3DDevice());
			mUpsampleBlurConstantBuffer.Initialize(mGame->Direct3DDevice());
		}

		//RTs and gizmos
		{
			for (int i = 0; i < NUM_VOXEL_GI_CASCADES; i++)
			{
				mVCTVoxelCascades3DRTs.push_back(new CustomRenderTarget(mGame->Direct3DDevice(), voxelCascadesSizes[i], voxelCascadesSizes[i], 1u, 
					DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS, 6, voxelCascadesSizes[i]));
				
				mVoxelCameraPositions[i] = XMFLOAT4(mCamera.Position().x, mCamera.Position().y, mCamera.Position().z, 1.0f);
				
				mDebugVoxelZonesGizmos.push_back(new RenderableAABB(*mGame, mCamera, XMFLOAT4(0.1f, 0.34f, 0.1f, 1.0f)));
				mDebugVoxelZonesGizmos[i]->Initialize();
				float maxBB = voxelCascadesSizes[i] / mWorldVoxelScales[i] * 0.5f;
				mDebugVoxelZonesGizmos[i]->InitializeGeometry({ XMFLOAT3(-maxBB, -maxBB, -maxBB), XMFLOAT3(maxBB, maxBB, maxBB) }, XMMatrixScaling(1, 1, 1));
				mDebugVoxelZonesGizmos[i]->SetPosition(XMFLOAT3(mVoxelCameraPositions[i].x, mVoxelCameraPositions[i].y, mVoxelCameraPositions[i].z));
			}
			mVCTMainRT = new CustomRenderTarget(mGame->Direct3DDevice(), 
				static_cast<UINT>(mGame->ScreenWidth()) * VCT_GI_MAIN_PASS_DOWNSCALE, static_cast<UINT>(mGame->ScreenHeight()) * VCT_GI_MAIN_PASS_DOWNSCALE, 1u, 
				DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, 1);
			mVCTUpsampleAndBlurRT = new CustomRenderTarget(mGame->Direct3DDevice(), static_cast<UINT>(mGame->ScreenWidth()), static_cast<UINT>(mGame->ScreenHeight()), 1u,
				DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, 1);
			mVCTVoxelizationDebugRT = new CustomRenderTarget(mGame->Direct3DDevice(), static_cast<UINT>(mGame->ScreenWidth()), static_cast<UINT>(mGame->ScreenHeight()), 1u,
				DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 1);
			mDepthBuffer = DepthTarget::Create(mGame->Direct3DDevice(), mGame->ScreenWidth(), mGame->ScreenHeight(), 1u, DXGI_FORMAT_D24_UNORM_S8_UINT);
		}
		
		//IBL - not used atm
		{
			if (FAILED(DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), 
				Utility::GetFilePath(L"content\\textures\\skyboxes\\Sky_5\\textureDiffuseHDR.dds").c_str(), nullptr, &mIrradianceDiffuseTextureSRV)))
				throw GameException("Failed to create Diffuse Irradiance Map.");

			mIBLRadianceMap.reset(new IBLRadianceMap(*mGame, Utility::GetFilePath(L"content\\textures\\skyboxes\\Sky_5\\textureEnvHDR.dds")));
			mIBLRadianceMap->Initialize();
			mIBLRadianceMap->Create(*mGame);

			mIrradianceSpecularTextureSRV = *mIBLRadianceMap->GetShaderResourceViewAddress();
			if (mIrradianceSpecularTextureSRV == nullptr)
				throw GameException("Failed to create Specular Irradiance Map.");
			mIBLRadianceMap.release();
			mIBLRadianceMap.reset(nullptr);

			// Load a pre-computed Integration Map
			if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), 
				Utility::GetFilePath(L"content\\textures\\PBR\\Skyboxes\\ibl_brdf_lut.png").c_str(), nullptr, &mIntegrationMapTextureSRV)))
				throw GameException("Failed to create Integration Texture.");

		}
	}

	void Illumination::Draw(const GameTime& gameTime, const Scene* scene, GBuffer* gbuffer)
	{
		static const float clearColorBlack[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		ID3D11DeviceContext* context = mGame->Direct3DDeviceContext();

		if (!mEnabled)
		{
			context->ClearUnorderedAccessViewFloat(mVCTMainRT->getUAV(), clearColorBlack);
			context->ClearUnorderedAccessViewFloat(mVCTUpsampleAndBlurRT->getUAV(), clearColorBlack);
			return;
		}

		D3D11_RECT rect = { 0.0f, 0.0f, mGame->ScreenWidth(), mGame->ScreenHeight() };
		D3D11_VIEWPORT viewport;
		UINT num_viewport = 1;
		context->RSGetViewports(&num_viewport, &viewport);

		ID3D11RasterizerState* oldRS;
		context->RSGetState(&oldRS);

		ID3D11RenderTargetView* nullRTVs[1] = { NULL };

		//voxelization
		{
			for (int cascade = 0; cascade < NUM_VOXEL_GI_CASCADES; cascade++)
			{
				D3D11_VIEWPORT vctViewport = { 0.0f, 0.0f, voxelCascadesSizes[cascade], voxelCascadesSizes[cascade] };
				D3D11_RECT vctRect = { 0.0f, 0.0f, voxelCascadesSizes[cascade], voxelCascadesSizes[cascade] };
				ID3D11UnorderedAccessView* UAV[1] = { mVCTVoxelCascades3DRTs[cascade]->getUAV() };

				context->RSSetState(RasterizerStates::NoCullingNoDepthEnabledScissorRect);
				context->RSSetViewports(1, &vctViewport);
				context->RSSetScissorRects(1, &vctRect);
				context->OMSetRenderTargets(1, nullRTVs, nullptr);
				context->ClearUnorderedAccessViewFloat(UAV[0], clearColorBlack);

				std::string materialName = MaterialHelper::voxelizationGIMaterialName + "_" + std::to_string(cascade);
				for (auto& obj : scene->objects) {
					auto material = static_cast<Rendering::VoxelizationGIMaterial*>(obj.second->GetMaterials()[materialName]);
					if (material)
					{
						material->VoxelCameraPos() << XMVECTOR{
							mVoxelCameraPositions[cascade].x,
							mVoxelCameraPositions[cascade].y,
							mVoxelCameraPositions[cascade].z, 1.0f };
						material->WorldVoxelScale() << mWorldVoxelScales[cascade];
						material->GetEffect()->GetEffect()->GetVariableByName("outputTexture")->AsUnorderedAccessView()->SetUnorderedAccessView(UAV[0]);
					}
					obj.second->Draw(materialName);
				}

				//voxelize extra objects
				{
					if (cascade == 0)
						mFoliageSystem->Draw(gameTime, &mShadowMapper, FoliageRenderingPass::VOXELIZATION);
				}

				//reset back
				context->RSSetViewports(1, &viewport);
				context->RSSetScissorRects(1, &rect);
			}
		}

		//voxelization debug 
		if (mDrawVoxelization)
		{
			int cascade = 0; //TODO fix for multiple cascades
			{
				float sizeTranslateShift = -voxelCascadesSizes[cascade] / mWorldVoxelScales[cascade] * 0.5f;
				mVoxelizationDebugConstantBuffer.Data.WorldVoxelCube =
					XMMatrixTranslation(
						sizeTranslateShift + mVoxelCameraPositions[cascade].x,
						sizeTranslateShift - mVoxelCameraPositions[cascade].y,
						sizeTranslateShift + mVoxelCameraPositions[cascade].z);
				mVoxelizationDebugConstantBuffer.Data.ViewProjection = XMMatrixTranspose(mCamera.ViewMatrix() * mCamera.ProjectionMatrix());
				mVoxelizationDebugConstantBuffer.ApplyChanges(context);

				ID3D11Buffer* CBs[1] = { mVoxelizationDebugConstantBuffer.Buffer() };
				ID3D11ShaderResourceView* SRVs[1] = { mVCTVoxelCascades3DRTs[cascade]->getSRV() };
				ID3D11RenderTargetView* RTVs[1] = { mVCTVoxelizationDebugRT->getRTV() };

				context->RSSetState(RasterizerStates::BackCulling);
				context->OMSetRenderTargets(1, RTVs, mDepthBuffer->getDSV());
				context->OMSetDepthStencilState(mDepthStencilStateRW, 0);
				context->ClearRenderTargetView(mVCTVoxelizationDebugRT->getRTV(), clearColorBlack);
				context->ClearDepthStencilView(mDepthBuffer->getDSV(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

				context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
				context->IASetInputLayout(nullptr);

				context->VSSetShader(mVCTVoxelizationDebugVS, NULL, NULL);
				context->VSSetConstantBuffers(0, 1, CBs);
				context->VSSetShaderResources(0, 1, SRVs);

				context->GSSetShader(mVCTVoxelizationDebugGS, NULL, NULL);
				context->GSSetConstantBuffers(0, 1, CBs);

				context->PSSetShader(mVCTVoxelizationDebugPS, NULL, NULL);
				context->PSSetConstantBuffers(0, 1, CBs);

				context->DrawInstanced(voxelCascadesSizes[cascade] * voxelCascadesSizes[cascade] * voxelCascadesSizes[cascade], 1, 0, 0);

				context->RSSetState(oldRS);
				context->OMSetRenderTargets(1, nullRTVs, NULL);
			}
		} 
		else
		{
			context->OMSetRenderTargets(1, nullRTVs, NULL);
			for (int i = 0; i < NUM_VOXEL_GI_CASCADES; i++)
			{
				context->GenerateMips(mVCTVoxelCascades3DRTs[i]->getSRV());
				mVoxelConeTracingConstantBuffer.Data.VoxelCameraPositions[i] = mVoxelCameraPositions[i];
				mVoxelConeTracingConstantBuffer.Data.WorldVoxelScales[i] = XMFLOAT4(mWorldVoxelScales[i], 0.0, 0.0, 0.0);
			}
			mVoxelConeTracingConstantBuffer.Data.CameraPos = XMFLOAT4(mCamera.Position().x, mCamera.Position().y, mCamera.Position().z, 1);
			mVoxelConeTracingConstantBuffer.Data.UpsampleRatio = XMFLOAT2(1.0f / VCT_GI_MAIN_PASS_DOWNSCALE, 1.0f / VCT_GI_MAIN_PASS_DOWNSCALE);
			mVoxelConeTracingConstantBuffer.Data.IndirectDiffuseStrength = mVCTIndirectDiffuseStrength;
			mVoxelConeTracingConstantBuffer.Data.IndirectSpecularStrength = mVCTIndirectSpecularStrength;
			mVoxelConeTracingConstantBuffer.Data.MaxConeTraceDistance = mVCTMaxConeTraceDistance;
			mVoxelConeTracingConstantBuffer.Data.AOFalloff = mVCTAoFalloff;
			mVoxelConeTracingConstantBuffer.Data.SamplingFactor = mVCTSamplingFactor;
			mVoxelConeTracingConstantBuffer.Data.VoxelSampleOffset = mVCTVoxelSampleOffset;
			mVoxelConeTracingConstantBuffer.Data.GIPower = mVCTGIPower;
			mVoxelConeTracingConstantBuffer.Data.pad0 = XMFLOAT3(0,0,0);
			mVoxelConeTracingConstantBuffer.ApplyChanges(context);

			ID3D11UnorderedAccessView* UAV[1] = { mVCTMainRT->getUAV() };
			ID3D11Buffer* CBs[1] = { mVoxelConeTracingConstantBuffer.Buffer() };
			ID3D11ShaderResourceView* SRVs[4 + NUM_VOXEL_GI_CASCADES];
			SRVs[0] = gbuffer->GetAlbedo()->getSRV();
			SRVs[1] = gbuffer->GetNormals()->getSRV();
			SRVs[2] = gbuffer->GetPositions()->getSRV();
			SRVs[3] = gbuffer->GetExtraBuffer()->getSRV();
			for (int i = 0; i < NUM_VOXEL_GI_CASCADES; i++)
				SRVs[4 + i] = mVCTVoxelCascades3DRTs[i]->getSRV();

			ID3D11SamplerState* SSs[] = { mLinearSamplerState };

			context->CSSetSamplers(0, 1, SSs);
			context->CSSetShaderResources(0, 4 + mVCTVoxelCascades3DRTs.size(), SRVs);
			context->CSSetConstantBuffers(0, 1, CBs);
			context->CSSetShader(mVCTMainCS, NULL, NULL);
			context->CSSetUnorderedAccessViews(0, 1, UAV, NULL);

			context->Dispatch(DivideByMultiple(static_cast<UINT>(mVCTMainRT->GetWidth()), 8u), DivideByMultiple(static_cast<UINT>(mVCTMainRT->GetHeight()), 8u), 1u);

			ID3D11ShaderResourceView* nullSRV[] = { NULL };
			context->CSSetShaderResources(0, 1, nullSRV);
			ID3D11UnorderedAccessView* nullUAV[] = { NULL };
			context->CSSetUnorderedAccessViews(0, 1, nullUAV, 0);
			ID3D11Buffer* nullCBs[] = { NULL };
			context->CSSetConstantBuffers(0, 1, nullCBs);
			ID3D11SamplerState* nullSSs[] = { NULL };
			context->CSSetSamplers(0, 1, nullSSs);
		}

		//upsample & blur
		{
			mUpsampleBlurConstantBuffer.Data.Upsample = true;
			mUpsampleBlurConstantBuffer.ApplyChanges(context);

			ID3D11UnorderedAccessView* UAV[1] = { mVCTUpsampleAndBlurRT->getUAV() };
			ID3D11Buffer* CBs[1] = { mUpsampleBlurConstantBuffer.Buffer() };
			ID3D11ShaderResourceView* SRVs[1] = { mVCTMainRT->getSRV() };
			ID3D11SamplerState* SSs[] = { mLinearSamplerState };

			context->CSSetSamplers(0, 1, SSs);
			context->CSSetShaderResources(0, 1, SRVs);
			context->CSSetConstantBuffers(0, 1, CBs);
			context->CSSetShader(mUpsampleBlurCS, NULL, NULL);
			context->CSSetUnorderedAccessViews(0, 1, UAV, NULL);

			context->Dispatch(DivideByMultiple(static_cast<UINT>(mVCTUpsampleAndBlurRT->GetWidth()), 8u), DivideByMultiple(static_cast<UINT>(mVCTUpsampleAndBlurRT->GetHeight()), 8u), 1u);

			ID3D11ShaderResourceView* nullSRV[] = { NULL };
			context->CSSetShaderResources(0, 1, nullSRV);
			ID3D11UnorderedAccessView* nullUAV[] = { NULL };
			context->CSSetUnorderedAccessViews(0, 1, nullUAV, 0);
			ID3D11Buffer* nullCBs[] = { NULL };
			context->CSSetConstantBuffers(0, 1, nullCBs);
			ID3D11SamplerState* nullSSs[] = { NULL };
			context->CSSetSamplers(0, 1, nullSSs);
		}
	}

	void Illumination::DrawDebugGizmos()
	{
		if (mDrawVoxelZonesGizmos) 
		{
			for (int i = 0; i < NUM_VOXEL_GI_CASCADES; i++)
			{
				mDebugVoxelZonesGizmos[i]->Draw();
			}
		}
	}

	void Illumination::Update(const GameTime& gameTime)
	{
		UpdateVoxelCameraPosition();
		UpdateImGui();
	}

	void Illumination::UpdateImGui()
	{
		if (!mShowDebug)
			return;

		ImGui::Begin("Global Illumination System");
		ImGui::Checkbox("Enabled", &mEnabled);
		ImGui::SliderFloat("VCT GI Intensity", &mVCTGIPower, 0.0f, 5.0f);
		ImGui::SliderFloat("VCT Diffuse Strength", &mVCTIndirectDiffuseStrength, 0.0f, 1.0f);
		ImGui::SliderFloat("VCT Specular Strength", &mVCTIndirectSpecularStrength, 0.0f, 1.0f);
		ImGui::SliderFloat("VCT Max Cone Trace Dist", &mVCTMaxConeTraceDistance, 0.0f, 2500.0f);
		ImGui::SliderFloat("VCT AO Falloff", &mVCTAoFalloff, 0.0f, 2.0f);
		ImGui::SliderFloat("VCT Sampling Factor", &mVCTSamplingFactor, 0.01f, 3.0f);
		ImGui::SliderFloat("VCT Sample Offset", &mVCTVoxelSampleOffset, -0.1f, 0.1f);
		for (int cascade = 0; cascade < NUM_VOXEL_GI_CASCADES; cascade++)
		{
			std::string name = "VCT Voxel Scale Cascade " + std::to_string(cascade);
			ImGui::SliderFloat(name.c_str(), &mWorldVoxelScales[cascade], 0.1f, 10.0f);
		}
		ImGui::Separator();
		ImGui::Checkbox("DEBUG - Ambient Occlusion", &mDrawAmbientOcclusionOnly);
		ImGui::Checkbox("DEBUG - Voxel Texture", &mDrawVoxelization);
		ImGui::Checkbox("DEBUG - Voxel Cascades Gizmos (Editor)", &mDrawVoxelZonesGizmos);

		ImGui::End();
	}

	void Illumination::UpdateVoxelizationGIMaterialVariables(Rendering::RenderingObject* obj, int meshIndex, int voxelCascadeIndex)
	{
		XMMATRIX vp = mCamera.ViewMatrix() * mCamera.ProjectionMatrix();

		const int shadowCascade = 1;
		std::string materialName = MaterialHelper::voxelizationGIMaterialName + "_" + std::to_string(voxelCascadeIndex);
		auto material = static_cast<Rendering::VoxelizationGIMaterial*>(obj->GetMaterials()[materialName]);
		if (material)
		{
			material->ViewProjection() << vp;
			material->ShadowMatrix() << mShadowMapper.GetViewMatrix(shadowCascade) * mShadowMapper.GetProjectionMatrix(shadowCascade) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix());
			material->ShadowTexelSize() << XMVECTOR{ 1.0f / mShadowMapper.GetResolution(), 1.0f, 1.0f , 1.0f };
			material->ShadowCascadeDistances() << XMVECTOR{ mCamera.GetCameraFarCascadeDistance(0), mCamera.GetCameraFarCascadeDistance(1), mCamera.GetCameraFarCascadeDistance(2), 1.0f };
			material->MeshWorld() << obj->GetTransformationMatrix();
			material->MeshAlbedo() << obj->GetTextureData(meshIndex).AlbedoMap;
			material->ShadowTexture() << mShadowMapper.GetShadowTexture(shadowCascade);
		}
	}

	void Illumination::SetFoliageSystem(FoliageSystem* foliageSystem)
	{
		mFoliageSystem = foliageSystem;
		if (mFoliageSystem)
		{
			//only first cascade due to performance
			mFoliageSystem->SetVoxelizationTextureOutput(mVCTVoxelCascades3DRTs[0]->getUAV()); 
			mFoliageSystem->SetVoxelizationParams(&mWorldVoxelScales[0], &mVoxelCameraPositions[0]);
		}
	}

	void Illumination::UpdateVoxelCameraPosition()
	{
		for (int i = 0; i < NUM_VOXEL_GI_CASCADES; i++)
		{
			float halfCascadeBox = 0.5f * (voxelCascadesSizes[i] / mWorldVoxelScales[i] * 0.5f);
			XMFLOAT3 voxelGridBoundsMax = XMFLOAT3{ mVoxelCameraPositions[i].x + halfCascadeBox, mVoxelCameraPositions[i].y + halfCascadeBox, mVoxelCameraPositions[i].z + halfCascadeBox };
			XMFLOAT3 voxelGridBoundsMin = XMFLOAT3{ mVoxelCameraPositions[i].x - halfCascadeBox, mVoxelCameraPositions[i].y - halfCascadeBox, mVoxelCameraPositions[i].z - halfCascadeBox };
			
			if (mCamera.Position().x < voxelGridBoundsMin.x || mCamera.Position().y < voxelGridBoundsMin.y || mCamera.Position().z < voxelGridBoundsMin.z ||
				mCamera.Position().x > voxelGridBoundsMax.x || mCamera.Position().y > voxelGridBoundsMax.y || mCamera.Position().z > voxelGridBoundsMax.z)
				mVoxelCameraPositions[i] = XMFLOAT4(mCamera.Position().x, mCamera.Position().y, mCamera.Position().z, 1.0f);

			mDebugVoxelZonesGizmos[i]->SetPosition(XMFLOAT3(mVoxelCameraPositions[i].x, mVoxelCameraPositions[i].y, mVoxelCameraPositions[i].z));
			mDebugVoxelZonesGizmos[i]->Update();
		}
	}
}