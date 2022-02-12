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
#include "StandardLightingMaterial.h"
#include "Scene.h"
#include "GBuffer.h"
#include "ShadowMapper.h"
#include "Foliage.h"
#include "IBLRadianceMap.h"
#include "RenderableAABB.h"
#include "ER_GPUBuffer.h"

namespace Library {

	const float voxelCascadesSizes[NUM_VOXEL_GI_CASCADES] = { 256.0f, 256.0f };

	Illumination::Illumination(Game& game, Camera& camera, const DirectionalLight& light, const ShadowMapper& shadowMapper, const Scene* scene)
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
		ReleaseObject(mDeferredLightingCS);

		ReleaseObject(mDepthStencilStateRW);
		ReleaseObject(mLinearSamplerState);
		ReleaseObject(mShadowSamplerState);

		DeletePointerCollection(mVCTVoxelCascades3DRTs);
		DeletePointerCollection(mDebugVoxelZonesGizmos);
		DeleteObject(mVCTVoxelizationDebugRT);
		DeleteObject(mVCTMainRT);
		DeleteObject(mVCTUpsampleAndBlurRT);
		DeleteObject(mDepthBuffer);

		mVoxelizationDebugConstantBuffer.Release();
		mVoxelConeTracingConstantBuffer.Release();
		mUpsampleBlurConstantBuffer.Release();
		mDeferredLightingConstantBuffer.Release();
		mLightProbesConstantBuffer.Release();
	}

	void Illumination::Initialize(const Scene* scene)
	{
		if (!scene)
			return;

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

			blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\DeferredLighting.hlsl").c_str(), "CSMain", "cs_5_0", &blob)))
				throw GameException("Failed to load CSMain from shader: DeferredLighting.hlsl!");
			if (FAILED(mGame->Direct3DDevice()->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mDeferredLightingCS)))
				throw GameException("Failed to create shader from DeferredLighting.hlsl!");
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

			sam_desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
			sam_desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
			sam_desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
			sam_desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
			float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			memcpy(sam_desc.BorderColor, reinterpret_cast<FLOAT*>(&white), sizeof(FLOAT) * 4);
			sam_desc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
			if (FAILED(mGame->Direct3DDevice()->CreateSamplerState(&sam_desc, &mShadowSamplerState)))
				throw GameException("Failed to create sampler mShadowSamplerState!");

			CD3D11_DEPTH_STENCIL_DESC dsDesc((CD3D11_DEFAULT()));
			dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
			mGame->Direct3DDevice()->CreateDepthStencilState(&dsDesc, &mDepthStencilStateRW);
		}
		
		//cbuffers
		{
			mVoxelizationDebugConstantBuffer.Initialize(mGame->Direct3DDevice());
			mVoxelConeTracingConstantBuffer.Initialize(mGame->Direct3DDevice());
			mUpsampleBlurConstantBuffer.Initialize(mGame->Direct3DDevice());
			mDeferredLightingConstantBuffer.Initialize(mGame->Direct3DDevice());
			mLightProbesConstantBuffer.Initialize(mGame->Direct3DDevice());
		}

		//RTs and gizmos
		{
			for (int i = 0; i < NUM_VOXEL_GI_CASCADES; i++)
			{
				mVCTVoxelCascades3DRTs.push_back(new ER_GPUTexture(mGame->Direct3DDevice(), voxelCascadesSizes[i], voxelCascadesSizes[i], 1u, 
					DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS, 6, voxelCascadesSizes[i]));
				
				mVoxelCameraPositions[i] = XMFLOAT4(mCamera.Position().x, mCamera.Position().y, mCamera.Position().z, 1.0f);
				
				mDebugVoxelZonesGizmos.push_back(new RenderableAABB(*mGame, mCamera, XMFLOAT4(0.1f, 0.34f, 0.1f, 1.0f)));
				mDebugVoxelZonesGizmos[i]->Initialize();
				float maxBB = voxelCascadesSizes[i] / mWorldVoxelScales[i] * 0.5f;
				mVoxelCascadesAABBs[i].first = XMFLOAT3(-maxBB, -maxBB, -maxBB);
				mVoxelCascadesAABBs[i].second = XMFLOAT3(maxBB, maxBB, maxBB);
				mDebugVoxelZonesGizmos[i]->InitializeGeometry({ mVoxelCascadesAABBs[i].first, mVoxelCascadesAABBs[i].second }, XMMatrixScaling(1, 1, 1));
				mDebugVoxelZonesGizmos[i]->SetPosition(XMFLOAT3(mVoxelCameraPositions[i].x, mVoxelCameraPositions[i].y, mVoxelCameraPositions[i].z));
			}
			mVCTMainRT = new ER_GPUTexture(mGame->Direct3DDevice(), 
				static_cast<UINT>(mGame->ScreenWidth()) * VCT_GI_MAIN_PASS_DOWNSCALE, static_cast<UINT>(mGame->ScreenHeight()) * VCT_GI_MAIN_PASS_DOWNSCALE, 1u, 
				DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, 1);
			mVCTUpsampleAndBlurRT = new ER_GPUTexture(mGame->Direct3DDevice(), static_cast<UINT>(mGame->ScreenWidth()), static_cast<UINT>(mGame->ScreenHeight()), 1u,
				DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, 1);
			mVCTVoxelizationDebugRT = new ER_GPUTexture(mGame->Direct3DDevice(), static_cast<UINT>(mGame->ScreenWidth()), static_cast<UINT>(mGame->ScreenHeight()), 1u,
				DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 1);
			mDepthBuffer = DepthTarget::Create(mGame->Direct3DDevice(), mGame->ScreenWidth(), mGame->ScreenHeight(), 1u, DXGI_FORMAT_D24_UNORM_S8_UINT);
		}
		
		for (auto& obj : scene->objects) {
			if (obj.second->IsForwardShading())
			{
				mForwardPassObjects.insert(obj);
				obj.second->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::forwardLightingMaterialName,
					[&](int meshIndex) { UpdateForwardLightingMaterial(obj.second, meshIndex); });
			}
		}
	}

	//deferred rendering approach
	void Illumination::DrawLocalIllumination(GBuffer* gbuffer, ER_GPUTexture* aRenderTarget, bool isEditorMode, bool clearInitTarget)
	{
		DrawDeferredLighting(gbuffer, aRenderTarget, clearInitTarget);
		DrawForwardLighting(gbuffer, aRenderTarget);

		if (isEditorMode) //todo move to a separate debug renderer
		{
			DrawDebugGizmos();
		}
	}

	//voxel GI based on "Interactive Indirect Illumination Using Voxel Cone Tracing" by C.Crassin et al.
	//https://research.nvidia.com/sites/default/files/pubs/2011-09_Interactive-Indirect-Illumination/GIVoxels-pg2011-authors.pdf
	void Illumination::DrawGlobalIllumination(GBuffer* gbuffer, const GameTime& gameTime)
	{
		static const float clearColorBlack[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		ID3D11DeviceContext* context = mGame->Direct3DDeviceContext();

		if (!mEnabled)
		{
			context->ClearUnorderedAccessViewFloat(mVCTMainRT->GetUAV(), clearColorBlack);
			context->ClearUnorderedAccessViewFloat(mVCTUpsampleAndBlurRT->GetUAV(), clearColorBlack);
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
				ID3D11UnorderedAccessView* UAV[1] = { mVCTVoxelCascades3DRTs[cascade]->GetUAV() };

				context->RSSetState(RasterizerStates::NoCullingNoDepthEnabledScissorRect);
				context->RSSetViewports(1, &vctViewport);
				context->RSSetScissorRects(1, &vctRect);
				context->OMSetRenderTargets(1, nullRTVs, nullptr);
				/*if (mVoxelCameraPositionsUpdated)
					*/context->ClearUnorderedAccessViewFloat(UAV[0], clearColorBlack);

				std::string materialName = MaterialHelper::voxelizationGIMaterialName + "_" + std::to_string(cascade);
				for (auto& obj : mVoxelizationObjects[cascade]) {
					auto material = static_cast<Rendering::VoxelizationGIMaterial*>(obj.second->GetMaterials()[materialName]);
					if (material)
					{
						material->VoxelCameraPos() << XMVECTOR{
							mVoxelCameraPositions[cascade].x,
							mVoxelCameraPositions[cascade].y,
							mVoxelCameraPositions[cascade].z, 1.0f };
						material->WorldVoxelScale() << mWorldVoxelScales[cascade];
						material->GetEffect()->GetEffect()->GetVariableByName("outputTexture")->AsUnorderedAccessView()->SetUnorderedAccessView(UAV[0]);
						obj.second->Draw(materialName);
					}
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
				ID3D11ShaderResourceView* SRVs[1] = { mVCTVoxelCascades3DRTs[cascade]->GetSRV() };
				ID3D11RenderTargetView* RTVs[1] = { mVCTVoxelizationDebugRT->GetRTV() };

				context->RSSetState(RasterizerStates::BackCulling);
				context->OMSetRenderTargets(1, RTVs, mDepthBuffer->getDSV());
				context->OMSetDepthStencilState(mDepthStencilStateRW, 0);
				context->ClearRenderTargetView(mVCTVoxelizationDebugRT->GetRTV(), clearColorBlack);
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
				context->GenerateMips(mVCTVoxelCascades3DRTs[i]->GetSRV());
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
			mVoxelConeTracingConstantBuffer.Data.pad0 = XMFLOAT3(0, 0, 0);
			mVoxelConeTracingConstantBuffer.ApplyChanges(context);

			ID3D11UnorderedAccessView* UAV[1] = { mVCTMainRT->GetUAV() };
			ID3D11Buffer* CBs[1] = { mVoxelConeTracingConstantBuffer.Buffer() };
			ID3D11ShaderResourceView* SRVs[4 + NUM_VOXEL_GI_CASCADES];
			SRVs[0] = gbuffer->GetAlbedo()->GetSRV();
			SRVs[1] = gbuffer->GetNormals()->GetSRV();
			SRVs[2] = gbuffer->GetPositions()->GetSRV();
			SRVs[3] = gbuffer->GetExtraBuffer()->GetSRV();
			for (int i = 0; i < NUM_VOXEL_GI_CASCADES; i++)
				SRVs[4 + i] = mVCTVoxelCascades3DRTs[i]->GetSRV();

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

			ID3D11UnorderedAccessView* UAV[1] = { mVCTUpsampleAndBlurRT->GetUAV() };
			ID3D11Buffer* CBs[1] = { mUpsampleBlurConstantBuffer.Buffer() };
			ID3D11ShaderResourceView* SRVs[1] = { mVCTMainRT->GetSRV() };
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
		//voxel GI
		if (mDrawVoxelZonesGizmos) 
		{
			for (int i = 0; i < NUM_VOXEL_GI_CASCADES; i++)
			{
				mDebugVoxelZonesGizmos[i]->Draw();
			}
		}

		//light probe system
		if (mProbesManager) {
			if (mDrawDiffuseProbes)
				mProbesManager->DrawDebugProbes(DIFFUSE_PROBE);
			if (mDrawSpecularProbes)
				mProbesManager->DrawDebugProbes(SPECULAR_PROBE);

			if (mDrawProbesVolumeGizmo)
				mProbesManager->DrawDebugProbesVolumeGizmo();
		}
	}

	void Illumination::Update(const GameTime& gameTime, const Scene* scene)
	{
		CPUCullObjectsAgainstVoxelCascades(scene);
		UpdateVoxelCameraPosition();
		UpdateImGui();
	}

	void Illumination::UpdateImGui()
	{
		if (!mShowDebug)
			return;

		ImGui::Begin("Illumination System");
		if (ImGui::CollapsingHeader("Global Illumination"))
		{
			if (ImGui::CollapsingHeader("Dynamic - Voxel Cone Tracing"))
			{
				ImGui::Checkbox("VCT GI Enabled", &mEnabled);
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
			}
			if (ImGui::CollapsingHeader("Static - Light Probes"))
			{
				//TODO add on/off for probe system
				//TODO add rebake etc.
				ImGui::Checkbox("DEBUG - Probe Volume", &mDrawProbesVolumeGizmo);
				ImGui::Checkbox("DEBUG - Diffuse Probes", &mDrawDiffuseProbes);
				ImGui::Checkbox("DEBUG - Specular Probes", &mDrawSpecularProbes);
			}
		}
		if (ImGui::CollapsingHeader("Local Illumination"))
		{
			ImGui::SliderFloat("Directional Light Intensity", &mDirectionalLightIntensity, 0.0f, 50.0f);
			ImGui::Separator();
		}
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

	void Illumination::UpdateForwardLightingMaterial(Rendering::RenderingObject* obj, int meshIndex)
	{
		XMMATRIX worldMatrix = XMLoadFloat4x4(&(obj->GetTransformationMatrix4X4()));
		XMMATRIX vp = mCamera.ViewMatrix() * mCamera.ProjectionMatrix();

		XMMATRIX shadowMatrices[3] =
		{
			mShadowMapper.GetViewMatrix(0) * mShadowMapper.GetProjectionMatrix(0) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix()) ,
			mShadowMapper.GetViewMatrix(1) * mShadowMapper.GetProjectionMatrix(1) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix()) ,
			mShadowMapper.GetViewMatrix(2) * mShadowMapper.GetProjectionMatrix(2) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix())
		};

		ID3D11ShaderResourceView* shadowMaps[3] =
		{
			mShadowMapper.GetShadowTexture(0),
			mShadowMapper.GetShadowTexture(1),
			mShadowMapper.GetShadowTexture(2)
		};

		auto material = static_cast<Rendering::StandardLightingMaterial*>(obj->GetMaterials()[MaterialHelper::forwardLightingMaterialName]);
		if (material)
		{
			if (obj->IsInstanced())
				material->SetCurrentTechnique(material->GetEffect()->TechniquesByName().at("standard_lighting_pbr_instancing"));
			else
				material->SetCurrentTechnique(material->GetEffect()->TechniquesByName().at("standard_lighting_pbr"));

			material->ViewProjection() << vp;
			material->World() << worldMatrix;
			material->ShadowMatrices().SetMatrixArray(shadowMatrices, 0, NUM_SHADOW_CASCADES);
			material->CameraPosition() << mCamera.PositionVector();
			material->SunDirection() << XMVectorNegate(mDirectionalLight.DirectionVector());
			material->SunColor() << XMVECTOR{ mDirectionalLight.GetDirectionalLightColor().x, mDirectionalLight.GetDirectionalLightColor().y, mDirectionalLight.GetDirectionalLightColor().z , mDirectionalLightIntensity };
			material->AmbientColor() << XMVECTOR{ mDirectionalLight.GetAmbientLightColor().x, mDirectionalLight.GetAmbientLightColor().y, mDirectionalLight.GetAmbientLightColor().z , 1.0f };
			material->ShadowTexelSize() << XMVECTOR{ 1.0f / mShadowMapper.GetResolution(), 1.0f, 1.0f , 1.0f };
			material->ShadowCascadeDistances() << XMVECTOR{ mCamera.GetCameraFarCascadeDistance(0), mCamera.GetCameraFarCascadeDistance(1), mCamera.GetCameraFarCascadeDistance(2), 1.0f };
			material->AlbedoTexture() << obj->GetTextureData(meshIndex).AlbedoMap;
			material->NormalTexture() << obj->GetTextureData(meshIndex).NormalMap;
			material->SpecularTexture() << obj->GetTextureData(meshIndex).SpecularMap;
			material->RoughnessTexture() << obj->GetTextureData(meshIndex).RoughnessMap;
			material->MetallicTexture() << obj->GetTextureData(meshIndex).MetallicMap;
			material->CascadedShadowTextures().SetResourceArray(shadowMaps, 0, NUM_SHADOW_CASCADES);
			material->IrradianceDiffuseTexture() << mProbesManager->GetDiffuseLightProbe(0)->GetCubemapSRV(); //TODO
			material->IrradianceSpecularTexture() << mProbesManager->GetSpecularLightProbe(0)->GetCubemapSRV();//TODO
			material->IntegrationTexture() << mProbesManager->GetIntegrationMap(); //TODO
		}
	}

	void Illumination::SetFoliageSystemForGI(FoliageSystem* foliageSystem)
	{
		mFoliageSystem = foliageSystem;
		if (mFoliageSystem)
		{
			//only first cascade due to performance
			mFoliageSystem->SetVoxelizationTextureOutput(mVCTVoxelCascades3DRTs[0]->GetUAV()); 
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
			{
				mVoxelCameraPositions[i] = XMFLOAT4(mCamera.Position().x, mCamera.Position().y, mCamera.Position().z, 1.0f);
				mVoxelCameraPositionsUpdated = true;
			}
			else
				mVoxelCameraPositionsUpdated = false;

			mDebugVoxelZonesGizmos[i]->SetPosition(XMFLOAT3(mVoxelCameraPositions[i].x, mVoxelCameraPositions[i].y, mVoxelCameraPositions[i].z));
			mDebugVoxelZonesGizmos[i]->Update();
		}
	}

	void Illumination::DrawDeferredLighting(GBuffer* gbuffer, ER_GPUTexture* aRenderTarget, bool clearTarget)
	{
		static const float clearColorBlack[] = { 0.0f, 0.0f, 0.0f, 0.0f };

		ID3D11DeviceContext* context = mGame->Direct3DDeviceContext();

		//compute pass
		if (aRenderTarget)
		{
			//might be cleared before (i.e., in PP)
			if (clearTarget) 
				context->ClearUnorderedAccessViewFloat(aRenderTarget->GetUAV(), clearColorBlack);

			for (size_t i = 0; i < NUM_SHADOW_CASCADES; i++)
				mDeferredLightingConstantBuffer.Data.ShadowMatrices[i] = mShadowMapper.GetViewMatrix(i) * mShadowMapper.GetProjectionMatrix(i) /** XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix())*/;
			mDeferredLightingConstantBuffer.Data.ShadowCascadeDistances = XMFLOAT4{ mCamera.GetCameraFarCascadeDistance(0), mCamera.GetCameraFarCascadeDistance(1), mCamera.GetCameraFarCascadeDistance(2), 1.0f };
			mDeferredLightingConstantBuffer.Data.ShadowTexelSize = XMFLOAT4{ 1.0f / mShadowMapper.GetResolution(), 1.0f, 1.0f , 1.0f };
			mDeferredLightingConstantBuffer.Data.SunDirection = XMFLOAT4{ -mDirectionalLight.Direction().x, -mDirectionalLight.Direction().y, -mDirectionalLight.Direction().z, 1.0f };
			mDeferredLightingConstantBuffer.Data.SunColor = XMFLOAT4{ mDirectionalLight.GetDirectionalLightColor().x, mDirectionalLight.GetDirectionalLightColor().y, mDirectionalLight.GetDirectionalLightColor().z, mDirectionalLightIntensity };
			mDeferredLightingConstantBuffer.Data.CameraPosition = XMFLOAT4{ mCamera.Position().x,mCamera.Position().y,mCamera.Position().z, 1.0f };
			mDeferredLightingConstantBuffer.ApplyChanges(context);

			for (size_t i = 0; i < NUM_PROBE_VOLUME_CASCADES; i++)
			{
				mLightProbesConstantBuffer.Data.LightProbesVolumeBounds[i] = XMFLOAT4{ mProbesManager->GetProbesVolumeCascade(i).x, mProbesManager->GetProbesVolumeCascade(i).y, mProbesManager->GetProbesVolumeCascade(i).z, 1.0f };
				mLightProbesConstantBuffer.Data.DiffuseProbesCellsCount[i] = mProbesManager->GetProbesCellsCount(DIFFUSE_PROBE, i);
				mLightProbesConstantBuffer.Data.DiffuseProbeIndexSkip[i] = XMFLOAT4(mProbesManager->GetProbesIndexSkip(DIFFUSE_PROBE, i), 0.0, 0.0, 0.0);
			}
			mLightProbesConstantBuffer.Data.SceneLightProbesBounds = XMFLOAT4{ mProbesManager->GetSceneProbesVolumeMin().x, mProbesManager->GetSceneProbesVolumeMin().y, mProbesManager->GetSceneProbesVolumeMin().z, 1.0f };
			mLightProbesConstantBuffer.Data.DistanceBetweenDiffuseProbes = DISTANCE_BETWEEN_DIFFUSE_PROBES;
			mLightProbesConstantBuffer.ApplyChanges(context);

			ID3D11Buffer* CBs[2] = { mDeferredLightingConstantBuffer.Buffer(), mLightProbesConstantBuffer.Buffer() };
			context->CSSetConstantBuffers(0, 2, CBs);

			ID3D11ShaderResourceView* SRs[16] = {
				gbuffer->GetAlbedo()->GetSRV(),
				gbuffer->GetNormals()->GetSRV(),
				gbuffer->GetPositions()->GetSRV(),
				gbuffer->GetExtraBuffer()->GetSRV(),
				mProbesManager->GetCulledDiffuseProbesTextureArray(0)->GetSRV(),
				mProbesManager->GetCulledDiffuseProbesTextureArray(1)->GetSRV(),
				mProbesManager->GetSpecularLightProbe(0)->GetCubemapSRV() /*TODO*/,
				mProbesManager->GetIntegrationMap()
			};
			for (int i = 0; i < NUM_SHADOW_CASCADES; i++)
				SRs[8 + i] = mShadowMapper.GetShadowTexture(i);

			SRs[11] = mProbesManager->GetDiffuseProbesCellsIndicesBuffer(0)->GetBufferSRV();
			SRs[12] = mProbesManager->GetDiffuseProbesCellsIndicesBuffer(1)->GetBufferSRV();
			SRs[13] = mProbesManager->GetDiffuseProbesTexArrayIndicesBuffer(0)->GetBufferSRV();
			SRs[14] = mProbesManager->GetDiffuseProbesTexArrayIndicesBuffer(1)->GetBufferSRV();
			SRs[15] = mProbesManager->GetDiffuseProbesPositionsBuffer()->GetBufferSRV();
			context->CSSetShaderResources(0, 16, SRs);

			ID3D11SamplerState* SS[2] = { mLinearSamplerState, mShadowSamplerState };
			context->CSSetSamplers(0, 2, SS);

			ID3D11UnorderedAccessView* UAV[1] = { aRenderTarget->GetUAV() };
			context->CSSetUnorderedAccessViews(0, 1, UAV, NULL);

			context->CSSetShader(mDeferredLightingCS, NULL, NULL);

			context->Dispatch(DivideByMultiple(static_cast<UINT>(aRenderTarget->GetWidth()), 8u), DivideByMultiple(static_cast<UINT>(aRenderTarget->GetHeight()), 8u), 1u);

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

	void Illumination::DrawForwardLighting(GBuffer* gbuffer, ER_GPUTexture* aRenderTarget)
	{
		ID3D11DeviceContext* context = mGame->Direct3DDeviceContext();
		context->OMSetRenderTargets(1, aRenderTarget->GetRTVs(), gbuffer->GetDepth()->getDSV());

		for (auto& obj : mForwardPassObjects)
		{
			obj.second->Draw(MaterialHelper::forwardLightingMaterialName);
		}
	}

	void Illumination::CPUCullObjectsAgainstVoxelCascades(const Scene* scene)
	{
		//TODO add instancing support
		//TODO fix repetition checks when the object AABB is bigger than the lower cascade (i.e. sponza)
		//TODO add optimization for culling objects by checking its volume size in second+ cascades
		//TODO add indirect drawing support (GPU cull)
		//TODO add multithreading per cascade
		for (int cascade = 0; cascade < NUM_VOXEL_GI_CASCADES; cascade++)
		{
			for (auto& objectInfo : scene->objects)
			{
				auto aabbObj = objectInfo.second->GetLocalAABB();
				XMFLOAT3 position;
				MatrixHelper::GetTranslation(objectInfo.second->GetTransformationMatrix(), position);
				aabbObj.first.x += position.x;
				aabbObj.first.y += position.y;
				aabbObj.first.z += position.z;
				aabbObj.second.x += position.x;
				aabbObj.second.y += position.y;
				aabbObj.second.z += position.z;

				//check if exists in previous cascade container
				if (cascade > 0)
				{
					auto it = mVoxelizationObjects[cascade - 1].find(objectInfo.first);
					if (it != mVoxelizationObjects[cascade - 1].end())
					{
						//check if should be removed from current cascade container
						auto it2 = mVoxelizationObjects[cascade].find(objectInfo.first);
						if (it2 != mVoxelizationObjects[cascade].end())
							mVoxelizationObjects[cascade].erase(it2);
						
						continue;
					}
				}

				auto aabbCascade = mVoxelCascadesAABBs[cascade];
				aabbCascade.first.x += mVoxelCameraPositions[cascade].x;
				aabbCascade.first.y += mVoxelCameraPositions[cascade].y;
				aabbCascade.first.z += mVoxelCameraPositions[cascade].z;		
				aabbCascade.second.x += mVoxelCameraPositions[cascade].x;
				aabbCascade.second.y += mVoxelCameraPositions[cascade].y;
				aabbCascade.second.z += mVoxelCameraPositions[cascade].z;

				bool isColliding =
					(aabbObj.first.x <= aabbCascade.second.x && aabbObj.second.x >= aabbCascade.first.x) &&
					(aabbObj.first.y <= aabbCascade.second.y && aabbObj.second.y >= aabbCascade.first.y) &&
					(aabbObj.first.z <= aabbCascade.second.z && aabbObj.second.z >= aabbCascade.first.z);

				auto it = mVoxelizationObjects[cascade].find(objectInfo.first);
				if (isColliding && (it == mVoxelizationObjects[cascade].end()))
					mVoxelizationObjects[cascade].insert(objectInfo);
				else if (!isColliding && (it != mVoxelizationObjects[cascade].end()))
					mVoxelizationObjects[cascade].erase(it);
			}
		}
	}
}