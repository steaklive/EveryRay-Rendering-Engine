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

namespace Library {
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

		DeleteObject(mVCTVoxelization3DRT);
		DeleteObject(mVCTVoxelizationDebugRT);
		DeleteObject(mVCTMainRT);
		DeleteObject(mVCTMainUpsampleAndBlurRT);
		DeleteObject(mDepthBuffer);
		ReleaseObject(mDepthStencilStateRW);
		ReleaseObject(mLinearSamplerState);

		mVoxelizationConstantBuffer.Release();
		mVoxelConeTracingConstantBuffer.Release();
	}

	void Illumination::Initialize(const Scene* scene)
	{
		for (auto& obj: scene->objects) {
			obj.second->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::voxelizationGIMaterialName, [&](int meshIndex) { UpdateVoxelizationGIMaterialVariables(obj.second, meshIndex); });
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
		}

		//sampler states
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

		//cbuffers
		mVoxelizationConstantBuffer.Initialize(mGame->Direct3DDevice());
		mVoxelConeTracingConstantBuffer.Initialize(mGame->Direct3DDevice());

		//RTs
		mVCTVoxelization3DRT = new CustomRenderTarget(mGame->Direct3DDevice(), VCT_SCENE_VOLUME_SIZE, VCT_SCENE_VOLUME_SIZE, 1u, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS, 6, VCT_SCENE_VOLUME_SIZE);
		mVCTMainRT = new CustomRenderTarget(mGame->Direct3DDevice(), static_cast<UINT>(mGame->ScreenWidth()), static_cast<UINT>(mGame->ScreenHeight()), 1u, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, 1);
		mVCTVoxelizationDebugRT = new CustomRenderTarget(mGame->Direct3DDevice(), static_cast<UINT>(mGame->ScreenWidth()), static_cast<UINT>(mGame->ScreenHeight()), 1u, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 1);
		mDepthBuffer = DepthTarget::Create(mGame->Direct3DDevice(), mGame->ScreenWidth(), mGame->ScreenHeight(), 1u, DXGI_FORMAT_D24_UNORM_S8_UINT);
	}

	void Illumination::Draw(const GameTime& gameTime, const Scene* scene, GBuffer* gbuffer)
	{
		static const float clearColorBlack[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		ID3D11DeviceContext* context = mGame->Direct3DDeviceContext();

		if (!mEnabled)
		{
			context->ClearUnorderedAccessViewFloat(mVCTMainRT->getUAV(), clearColorBlack);
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
			D3D11_VIEWPORT vctViewport = { 0.0f, 0.0f, VCT_SCENE_VOLUME_SIZE, VCT_SCENE_VOLUME_SIZE};
			D3D11_RECT vctRect = { 0.0f, 0.0f, VCT_SCENE_VOLUME_SIZE, VCT_SCENE_VOLUME_SIZE };
			ID3D11UnorderedAccessView* UAV[1] = { mVCTVoxelization3DRT->getUAV() };
		
			context->RSSetState(RasterizerStates::NoCullingNoDepthEnabledScissorRect);
			context->RSSetViewports(1, &vctViewport);
			context->RSSetScissorRects(1, &vctRect);
			context->OMSetRenderTargets(1, nullRTVs, nullptr);
			context->ClearUnorderedAccessViewFloat(UAV[0], clearColorBlack);

			for (auto& obj : scene->objects) {
				obj.second->GetMaterials()[MaterialHelper::voxelizationGIMaterialName]->GetEffect()->
					GetEffect()->GetVariableByName("outputTexture")->AsUnorderedAccessView()->SetUnorderedAccessView(UAV[0]);
				
				obj.second->Draw(MaterialHelper::voxelizationGIMaterialName);

				//obj.second->GetMaterials()[MaterialHelper::voxelizationGIMaterialName]->GetEffect()->
				//	GetEffect()->GetVariableByName("outputTexture")->AsUnorderedAccessView()->SetUnorderedAccessView(nullptr);
				//obj.second->GetMaterials()[MaterialHelper::voxelizationGIMaterialName]->GetEffect()->
				//	GetEffect()->GetTechniqueByIndex(0)->GetPassByIndex(0)->Apply(0, context);
			}
		
			//reset back
			context->RSSetViewports(1, &viewport);
			context->RSSetScissorRects(1, &rect);
		}

		mVoxelizationConstantBuffer.Data.WorldVoxelCube = XMMatrixTranslation(-VCT_SCENE_VOLUME_SIZE * 0.25f, -VCT_SCENE_VOLUME_SIZE * 0.25f, -VCT_SCENE_VOLUME_SIZE * 0.25f) * XMMatrixScaling(1.0f, -1.0f, 1.0f);;
		mVoxelizationConstantBuffer.Data.ViewProjection = mCamera.ViewMatrix() * mCamera.ProjectionMatrix();
		mVoxelizationConstantBuffer.Data.WorldVoxelScale = mWorldVoxelScale;
		mVoxelizationConstantBuffer.ApplyChanges(context);

		//voxelization debug 
		if (mVoxelizationDebugView)
		{
			ID3D11Buffer* CBs[1] = { mVoxelizationConstantBuffer.Buffer() };
			ID3D11ShaderResourceView* SRVs[1] = { mVCTVoxelization3DRT->getSRV() };
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

			context->DrawInstanced(VCT_SCENE_VOLUME_SIZE * VCT_SCENE_VOLUME_SIZE * VCT_SCENE_VOLUME_SIZE, 1, 0, 0);
			
			context->RSSetState(oldRS);
			context->OMSetRenderTargets(1, nullRTVs, NULL);
		} 
		else
		{
			context->OMSetRenderTargets(1, nullRTVs, NULL);

			context->GenerateMips(mVCTVoxelization3DRT->getSRV());

			mVoxelConeTracingConstantBuffer.Data.CameraPos = XMFLOAT4(mCamera.Position().x, mCamera.Position().y, mCamera.Position().z, 1);
			mVoxelConeTracingConstantBuffer.Data.UpsampleRatio = XMFLOAT2(/*mGbufferRTs[0]->GetWidth() / mVCTMainRT->GetWidth(), mGbufferRTs[0]->GetHeight() / mVCTMainRT->GetHeight()*/1.0f, 1.0f);
			mVoxelConeTracingConstantBuffer.Data.IndirectDiffuseStrength = mVCTIndirectDiffuseStrength;
			mVoxelConeTracingConstantBuffer.Data.IndirectSpecularStrength = mVCTIndirectSpecularStrength;
			mVoxelConeTracingConstantBuffer.Data.MaxConeTraceDistance = mVCTMaxConeTraceDistance;
			mVoxelConeTracingConstantBuffer.Data.AOFalloff = mVCTAoFalloff;
			mVoxelConeTracingConstantBuffer.Data.SamplingFactor = mVCTSamplingFactor;
			mVoxelConeTracingConstantBuffer.Data.VoxelSampleOffset = mVCTVoxelSampleOffset;
			mVoxelConeTracingConstantBuffer.Data.GIPower = mVCTGIPower;
			mVoxelConeTracingConstantBuffer.ApplyChanges(context);

			ID3D11UnorderedAccessView* UAV[1] = { mVCTMainRT->getUAV() };
			ID3D11Buffer* CBs[2] = { mVoxelizationConstantBuffer.Buffer(), mVoxelConeTracingConstantBuffer.Buffer() };
			ID3D11ShaderResourceView* SRVs[4] = {
				gbuffer->GetAlbedo()->getSRV(),
				gbuffer->GetNormals()->getSRV(),
				gbuffer->GetPositions()->getSRV(),
				mVCTVoxelization3DRT->getSRV()
			};
			ID3D11SamplerState* SSs[] = { mLinearSamplerState };

			context->CSSetSamplers(0, 1, SSs);
			context->CSSetShaderResources(0, 4, SRVs);
			context->CSSetConstantBuffers(0, 2, CBs);
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

		//TODO upsample & blur
		{

		}
	}

	void Illumination::Update(const GameTime& gameTime)
	{
		UpdateImGui();
	}

	void Illumination::UpdateImGui()
	{
		if (!mShowDebug)
			return;

		ImGui::Begin("Global Illumination System");
		ImGui::Checkbox("Enabled", &mEnabled);
		ImGui::Checkbox("Show voxelization debug view", &mVoxelizationDebugView);
		ImGui::SliderFloat("VCT GI Intensity", &mVCTGIPower, 0.0f, 5.0f);
		ImGui::SliderFloat("VCT Diffuse Strength", &mVCTIndirectDiffuseStrength, 0.0f, 1.0f);
		ImGui::SliderFloat("VCT Specular Strength", &mVCTIndirectSpecularStrength, 0.0f, 1.0f);
		ImGui::SliderFloat("VCT Max Cone Trace Dist", &mVCTMaxConeTraceDistance, 0.0f, 2500.0f);
		ImGui::SliderFloat("VCT AO Falloff", &mVCTAoFalloff, 0.0f, 2.0f);
		ImGui::SliderFloat("VCT Sampling Factor", &mVCTSamplingFactor, 0.01f, 3.0f);
		ImGui::SliderFloat("VCT Sample Offset", &mVCTVoxelSampleOffset, -0.1f, 0.1f);
		ImGui::End();
	}

	void Illumination::UpdateVoxelizationGIMaterialVariables(Rendering::RenderingObject* obj, int meshIndex)
	{
		XMMATRIX vp = mCamera.ViewMatrix() * mCamera.ProjectionMatrix();

		XMMATRIX shadowMatrices[3] =
		{
			mShadowMapper.GetViewMatrix(0) *  mShadowMapper.GetProjectionMatrix(0) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix()) ,
			mShadowMapper.GetViewMatrix(1) *  mShadowMapper.GetProjectionMatrix(1) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix()) ,
			mShadowMapper.GetViewMatrix(2) *  mShadowMapper.GetProjectionMatrix(2) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix())
		};

		ID3D11ShaderResourceView* shadowMaps[3] =
		{
			mShadowMapper.GetShadowTexture(0),
			mShadowMapper.GetShadowTexture(1),
			mShadowMapper.GetShadowTexture(2)
		};
		ID3D11ShaderResourceView* shadowMap = nullptr;

		static_cast<Rendering::VoxelizationGIMaterial*>(obj->GetMaterials()[MaterialHelper::voxelizationGIMaterialName])->ViewProjection() << vp;
		static_cast<Rendering::VoxelizationGIMaterial*>(obj->GetMaterials()[MaterialHelper::voxelizationGIMaterialName])->ShadowMatrices().SetMatrixArray(shadowMatrices, 0, MAX_NUM_OF_CASCADES);
		static_cast<Rendering::VoxelizationGIMaterial*>(obj->GetMaterials()[MaterialHelper::voxelizationGIMaterialName])->ShadowTexelSize() << XMVECTOR{ 1.0f / mShadowMapper.GetResolution(), 1.0f, 1.0f , 1.0f };
		static_cast<Rendering::VoxelizationGIMaterial*>(obj->GetMaterials()[MaterialHelper::voxelizationGIMaterialName])->ShadowCascadeDistances() << XMVECTOR{ mCamera.GetCameraFarCascadeDistance(0), mCamera.GetCameraFarCascadeDistance(1), mCamera.GetCameraFarCascadeDistance(2), 1.0f };
		static_cast<Rendering::VoxelizationGIMaterial*>(obj->GetMaterials()[MaterialHelper::voxelizationGIMaterialName])->WorldVoxelScale() << mWorldVoxelScale;
		static_cast<Rendering::VoxelizationGIMaterial*>(obj->GetMaterials()[MaterialHelper::voxelizationGIMaterialName])->MeshWorld() << obj->GetTransformationMatrix();
		static_cast<Rendering::VoxelizationGIMaterial*>(obj->GetMaterials()[MaterialHelper::voxelizationGIMaterialName])->MeshAlbedo() << obj->GetTextureData(meshIndex).AlbedoMap;
		static_cast<Rendering::VoxelizationGIMaterial*>(obj->GetMaterials()[MaterialHelper::voxelizationGIMaterialName])->CascadedShadowTextures().SetResourceArray(shadowMaps, 0, MAX_NUM_OF_CASCADES);

	}
}