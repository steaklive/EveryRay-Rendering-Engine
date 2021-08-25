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

namespace Library {
	Illumination::Illumination(Game& game, Camera& camera, DirectionalLight& light, const Scene* scene)
		: 
		GameComponent(game),
		mCamera(camera),
		mDirectionalLight(light)
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
		//mFrameConstantBuffer.Release();
		//mCloudsConstantBuffer.Release();
	}

	void Illumination::Initialize(const Scene* scene)
	{
		for (auto& obj: scene->objects) {
			obj.second->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::voxelizationGIMaterialName, [&](int meshIndex) { UpdateVoxelizationGIMaterialVariables(obj.second, meshIndex); });
		}

		////shaders
		//{
		//	ID3DBlob* blob = nullptr;
		//	if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\GI\\VoxelConeTracingVoxelization.hlsl").c_str(), "VSMain", "vs_5_0", &blob)))
		//		throw GameException("Failed to load VSMain from shader: VoxelConeTracingVoxelization.hlsl!");
		//	if (FAILED(mGame->Direct3DDevice()->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mVCTVoxelizationVS)))
		//		throw GameException("Failed to create vertex shader from VoxelConeTracingVoxelization.hlsl!");
		//	blob->Release();

		//	if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\GI\\VoxelConeTracingVoxelization.hlsl").c_str(), "GSMain", "gs_5_0", &blob)))
		//		throw GameException("Failed to load GSMain from shader: VoxelConeTracingVoxelization.hlsl!");
		//	if (FAILED(mGame->Direct3DDevice()->CreateGeometryShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mVCTVoxelizationGS)))
		//		throw GameException("Failed to create geometry shader from VoxelConeTracingVoxelization.hlsl!");
		//	blob->Release();

		//	blob = nullptr;
		//	if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\GI\\VoxelConeTracingVoxelization.hlsl").c_str(), "PSMain", "ps_5_0", &blob)))
		//		throw GameException("Failed to load PSMain from shader: VoxelConeTracingVoxelization.hlsl!");
		//	if (FAILED(mGame->Direct3DDevice()->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mVCTVoxelizationPS)))
		//		throw GameException("Failed to create pixel shader from VoxelConeTracingVoxelization.hlsl!");
		//	blob->Release();

		//	blob = nullptr;
		//	if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\GI\\VoxelConeTracingMain.hlsl").c_str(), "CSMain", "cs_5_0", &blob)))
		//		throw GameException("Failed to load CSMain from shader: VoxelConeTracingMain.hlsl!");
		//	if (FAILED(mGame->Direct3DDevice()->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mVCTMainCS)))
		//		throw GameException("Failed to create shader from VoxelConeTracingMain.hlsl!");
		//	blob->Release();
		//}
		
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;

		//TODO 3D
		mVCTVoxelization3DRT = new CustomRenderTarget(mGame->Direct3DDevice(), VCT_SCENE_VOLUME_SIZE, VCT_SCENE_VOLUME_SIZE, 1u, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, 1, VCT_SCENE_VOLUME_SIZE);
		mVCTMainRT = new CustomRenderTarget(mGame->Direct3DDevice(), static_cast<UINT>(mGame->ScreenWidth()), static_cast<UINT>(mGame->ScreenHeight()), 1u, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, 1);
	}

	void Illumination::Draw(const GameTime& gameTime, const Scene* scene)
	{
		static const float clearColorBlack[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		ID3D11DeviceContext* context = mGame->Direct3DDeviceContext();

		D3D11_VIEWPORT viewport = { 0.0f, 0.0f, mGame->ScreenWidth(), mGame->ScreenHeight() };
		D3D11_RECT rect = { 0.0f, 0.0f, mGame->ScreenWidth(), mGame->ScreenHeight() };

		//voxelization
		{
			D3D11_VIEWPORT vctViewport = { 0.0f, 0.0f, VCT_SCENE_VOLUME_SIZE, VCT_SCENE_VOLUME_SIZE };
			D3D11_RECT vctRect = { 0.0f, 0.0f, VCT_SCENE_VOLUME_SIZE, VCT_SCENE_VOLUME_SIZE };
			ID3D11UnorderedAccessView* UAV[1] = { mVCTVoxelization3DRT->getUAV() };

			context->RSSetViewports(1, &vctViewport);
			context->RSSetScissorRects(1, &vctRect);
			context->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 0, 1, UAV, NULL);
			context->ClearUnorderedAccessViewFloat(UAV[0], clearColorBlack);

			//context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			//context->IASetInputLayout(mVCTVoxelizationInputLayout);

			//context->VSSetShader(mVCTVoxelizationVS, NULL, NULL);
			//context->GSSetShader(mVCTVoxelizationGS, NULL, NULL);
			//context->PSSetShader(mVCTVoxelizationPS, NULL, NULL);

			for (auto& obj : scene->objects) {
				obj.second->Draw(MaterialHelper::voxelizationGIMaterialName);
			}

			//reset back
			context->RSSetViewports(1, &viewport);
			context->RSSetScissorRects(1, &rect);
		}

		//main vct
		{

		}

		//upsample & blur
		{

		}
	}

	void Illumination::Update(const GameTime& gameTime)
	{

	}

	void Illumination::UpdateVoxelizationGIMaterialVariables(Rendering::RenderingObject* obj, int meshIndex)
	{
		XMMATRIX vp = mCamera.ViewMatrix() * mCamera.ProjectionMatrix();

		///XMMATRIX shadowMatrices[3] =
		///{
		///	mShadowMapper->GetViewMatrix(0) *  mShadowMapper->GetProjectionMatrix(0) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix()) ,
		///	mShadowMapper->GetViewMatrix(1) *  mShadowMapper->GetProjectionMatrix(1) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix()) ,
		///	mShadowMapper->GetViewMatrix(2) *  mShadowMapper->GetProjectionMatrix(2) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix())
		///};
		///
		///ID3D11ShaderResourceView* shadowMaps[3] =
		///{
		///	mShadowMapper->GetShadowTexture(0),
		///	mShadowMapper->GetShadowTexture(1),
		///	mShadowMapper->GetShadowTexture(2)
		///};
		ID3D11ShaderResourceView* shadowMap = nullptr;

		static_cast<Rendering::VoxelizationGIMaterial*>(obj->GetMaterials()[MaterialHelper::voxelizationGIMaterialName])->ViewProjection() << vp;
		static_cast<Rendering::VoxelizationGIMaterial*>(obj->GetMaterials()[MaterialHelper::voxelizationGIMaterialName])->LightViewProjection() << vp;
		static_cast<Rendering::VoxelizationGIMaterial*>(obj->GetMaterials()[MaterialHelper::voxelizationGIMaterialName])->WorldVoxelScale() << mWorldVoxelScale;
		static_cast<Rendering::VoxelizationGIMaterial*>(obj->GetMaterials()[MaterialHelper::voxelizationGIMaterialName])->MeshWorld() << obj->GetTransformationMatrix();
		static_cast<Rendering::VoxelizationGIMaterial*>(obj->GetMaterials()[MaterialHelper::voxelizationGIMaterialName])->MeshAlbedo() << obj->GetTextureData(meshIndex).AlbedoMap;
		static_cast<Rendering::VoxelizationGIMaterial*>(obj->GetMaterials()[MaterialHelper::voxelizationGIMaterialName])->ShadowMap() << shadowMap;
	}
}