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
#include "Utility.h"
#include "VertexDeclarations.h"
#include "RasterizerStates.h"
#include "ShaderCompiler.h"

namespace Library {
	Illumination::Illumination(Game& game, Camera& camera, DirectionalLight& light)
		: 
		GameComponent(game),
		mCamera(camera),
		mDirectionalLight(light)
	{
		Initialize();
	}

	Illumination::~Illumination()
	{
		ReleaseObject(mCloudSS);
		ReleaseObject(mWeatherSS);
		ReleaseObject(mCloudTextureSRV);
		ReleaseObject(mWeatherTextureSRV);
		ReleaseObject(mWorleyTextureSRV);
		ReleaseObject(mMainCS);
		ReleaseObject(mMainPS);
		ReleaseObject(mCompositePS);
		ReleaseObject(mBlurPS);
		DeleteObject(mCustomMainRenderTargetCS);
		DeleteObject(mMainRenderTargetPS);
		DeleteObject(mCompositeRenderTarget);
		DeleteObject(mBlurRenderTarget);
		mFrameConstantBuffer.Release();
		mCloudsConstantBuffer.Release();
	}

	Illumination::Initialize()
	{
		//shaders
		ID3DBlob* vertexShaderBlob = nullptr;
		{
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\GI\\VoxelConeTracingVoxelization.hlsl").c_str(), "VSMain", "vs_5_0", &vertexShaderBlob)))
				throw GameException("Failed to load VSMain from shader: VoxelConeTracingVoxelization.hlsl!");
			if (FAILED(mGame->Direct3DDevice()->CreateVertexShader(vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), NULL, &mVCTVoxelizationVS)))
				throw GameException("Failed to create vertex shader from VoxelConeTracingVoxelization.hlsl!");

			ID3DBlob* blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\GI\\VoxelConeTracingVoxelization.hlsl").c_str(), "GSMain", "gs_5_0", &blob)))
				throw GameException("Failed to load GSMain from shader: VoxelConeTracingVoxelization.hlsl!");
			if (FAILED(mGame->Direct3DDevice()->CreateGeometryShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mVCTVoxelizationGS)))
				throw GameException("Failed to create geometry shader from VoxelConeTracingVoxelization.hlsl!");
			blob->Release();

			blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\GI\\VoxelConeTracingVoxelization.hlsl").c_str(), "PSMain", "ps_5_0", &blob)))
				throw GameException("Failed to load PSMain from shader: VoxelConeTracingVoxelization.hlsl!");
			if (FAILED(mGame->Direct3DDevice()->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mVCTVoxelizationPS)))
				throw GameException("Failed to create pixel shader from VoxelConeTracingVoxelization.hlsl!");
			blob->Release();

			blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\GI\\VoxelConeTracingMain.hlsl").c_str(), "CSMain", "cs_5_0", &blob)))
				throw GameException("Failed to load CSMain from shader: VoxelConeTracingMain.hlsl!");
			if (FAILED(mGame->Direct3DDevice()->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mVCTMainCS)))
				throw GameException("Failed to create shader from VoxelConeTracingMain.hlsl!");
			blob->Release();
		}
		
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;

		//TODO 3D
		mVCTVoxelization3DRT = new CustomRenderTarget(mGame->Direct3DDevice(), VCT_SCENE_VOLUME_SIZE, VCT_SCENE_VOLUME_SIZE, 1u, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, 1);
		mVCTMainRT = new CustomRenderTarget(mGame->Direct3DDevice(), static_cast<UINT>(mGame->ScreenWidth()), static_cast<UINT>(mGame->ScreenHeight()), 1u, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, 1);
		

		D3D11_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		mGame.Direct3DDevice()->CreateInputLayout(inputElementDescs, 1, vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), &mVCTVoxelizationInputLayout);
		vertexShaderBlob->Release();

	}

	Illumination::Draw(const GameTime& gameTime, const std::vector<RenderingObject*>& aSceneObjects)
	{
		static const float clearColorBlack[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		ID3D11DeviceContext* context = mGame->Direct3DDeviceContext();

		D3D11_VIEWPORT viewport = D3D11_VIEWPORT(0.0f, 0.0f, mGame->ScreenWidth(), mGame->ScreenHeight());
		D3D11_RECT rect = D3D11_RECT(0.0f, 0.0f, mGame->ScreenWidth(), mGame->ScreenHeight());

		//voxelization
		{
			D3D11_VIEWPORT vctViewport = D3D11_VIEWPORT(0.0f, 0.0f, VCT_SCENE_VOLUME_SIZE, VCT_SCENE_VOLUME_SIZE);
			D3D11_RECT vctRect = D3D11_RECT(0.0f, 0.0f, VCT_SCENE_VOLUME_SIZE, VCT_SCENE_VOLUME_SIZE);
			ID3D11UnorderedAccessView* UAV[1] = { mVCTVoxelization3DRT->getUAV() };

			context->RSSetViewports(1, &vctViewport);
			context->RSSetScissorRects(1, &vctRect);
			context->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 0, 1, UAV, NULL);
			context->ClearUnorderedAccessViewFloat(UAV[0], clearColorBlack);

			context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			context->IASetInputLayout(mVCTVoxelizationInputLayout);

			context->VSSetShader(mVCTVoxelizationVS, NULL, NULL);
			context->GSSetShader(mVCTVoxelizationGS, NULL, NULL);
			context->PSSetShader(mVCTVoxelizationPS, NULL, NULL);

			for (auto& obj : aSceneObjects) {
				obj->Draw(MaterialHelper::voxelizationGIMaterialName);
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
}