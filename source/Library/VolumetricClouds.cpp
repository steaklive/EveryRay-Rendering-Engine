#include "stdafx.h"
#include <stdio.h>
#include "VolumetricClouds.h"
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
#include "PostProcessingStack.h"
#include "Skybox.h"

namespace Library {
	VolumetricClouds::VolumetricClouds(Game& game, Camera& camera, DirectionalLight& light, Rendering::PostProcessingStack& stack, Skybox& skybox)
		: GameComponent(game),
		mCamera(camera), 
		mDirectionalLight(light),
		mPostProcessingStack(stack),
		mSkybox(skybox)
	{
		Initialize();
	}
	VolumetricClouds::~VolumetricClouds()
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
		DeleteObject(mMainRenderTargetCS_Custom);
		DeleteObject(mMainRenderTargetPS);
		DeleteObject(mCompositeRenderTarget);
		DeleteObject(mBlurRenderTarget);
		mFrameConstantBuffer.Release();
		mCloudsConstantBuffer.Release();
	}

	void VolumetricClouds::Initialize() {
		//shaders
		ID3DBlob* blob = nullptr;
		if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\VolumetricClouds\\VolumetricClouds.hlsl").c_str(), "main", "ps_5_0", &blob)))
			throw GameException("Failed to load main pass from shader: VolumetricClouds.hlsl!");
		if (FAILED(mGame->Direct3DDevice()->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mMainPS)))
			throw GameException("Failed to create shader from VolumetricClouds.hlsl!");
		blob->Release();

		blob = nullptr;
		if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\VolumetricClouds\\VolumetricCloudsComposite.hlsl").c_str(), "main", "ps_5_0", &blob)))
			throw GameException("Failed to load main pass from shader: VolumetricCloudsComposite.hlsl!");
		if (FAILED(mGame->Direct3DDevice()->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mCompositePS)))
			throw GameException("Failed to create shader from VolumetricCloudsComposite.hlsl!");
		blob->Release();

		blob = nullptr;
		if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\VolumetricClouds\\VolumetricCloudsBlur.hlsl").c_str(), "main", "ps_5_0", &blob)))
			throw GameException("Failed to load main pass from shader: VolumetricCloudsBlur.hlsl!");
		if (FAILED(mGame->Direct3DDevice()->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mBlurPS)))
			throw GameException("Failed to create shader from VolumetricCloudsBlur.hlsl!");
		blob->Release();

		blob = nullptr;
		if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\VolumetricClouds\\VolumetricCloudsCS.hlsl").c_str(), "main", "cs_5_0", &blob)))
			throw GameException("Failed to load main pass from shader: VolumetricCloudsCS.hlsl!");
		if (FAILED(mGame->Direct3DDevice()->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mMainCS)))
			throw GameException("Failed to create shader from VolumetricCloudsCS.hlsl!");
		blob->Release();

		//textures
		if (FAILED(DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), Utility::GetFilePath(L"content\\textures\\VolumetricClouds\\cloud.dds").c_str(), nullptr, &mCloudTextureSRV)))
			throw GameException("Failed to create Cloud Map.");

		if (FAILED(DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), Utility::GetFilePath(L"content\\textures\\VolumetricClouds\\weather.dds").c_str(), nullptr, &mWeatherTextureSRV)))
			throw GameException("Failed to create Weather Map.");

		if (FAILED(DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), Utility::GetFilePath(L"content\\textures\\VolumetricClouds\\worley.dds").c_str(), nullptr, &mWorleyTextureSRV)))
			throw GameException("Failed to create Worley Map.");

		//cbuffers
		mFrameConstantBuffer.Initialize(mGame->Direct3DDevice());
		mCloudsConstantBuffer.Initialize(mGame->Direct3DDevice());

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
		if (FAILED(mGame->Direct3DDevice()->CreateSamplerState(&sam_desc, &mCloudSS)))
			throw GameException("Failed to create sampler mCloudSS!");

		sam_desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		if (FAILED(mGame->Direct3DDevice()->CreateSamplerState(&sam_desc, &mWeatherSS)))
			throw GameException("Failed to create sampler mWeatherSS!");

		//render targets
		mMainRenderTargetPS = new FullScreenRenderTarget(*mGame);
		mCompositeRenderTarget = new FullScreenRenderTarget(*mGame);
		mBlurRenderTarget = new FullScreenRenderTarget(*mGame);
		mMainRenderTargetCS_Custom = CustomRenderTarget::Create(mGame->Direct3DDevice(), static_cast<UINT>(mGame->ScreenWidth()), static_cast<UINT>(mGame->ScreenHeight()), 1u, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, 1);
	}
	
	void VolumetricClouds::Update(const GameTime& gameTime)
	{
		UpdateImGui();

		if (!mEnabled)
			return;

		ID3D11DeviceContext* context = mGame->Direct3DDeviceContext();

		mFrameConstantBuffer.Data.InvProj = XMMatrixInverse(nullptr, mCamera.ProjectionMatrix());
		mFrameConstantBuffer.Data.InvView = XMMatrixInverse(nullptr, mCamera.ViewMatrix());
		mFrameConstantBuffer.Data.LightDir = -mDirectionalLight.DirectionVector();
		mFrameConstantBuffer.Data.LightCol = XMVECTOR{ mDirectionalLight.GetDirectionalLightColor().x, mDirectionalLight.GetDirectionalLightColor().y, mDirectionalLight.GetDirectionalLightColor().z, 1.0f };
		mFrameConstantBuffer.Data.CameraPos = mCamera.PositionVector();
		mFrameConstantBuffer.Data.Resolution = XMFLOAT2(mGame->ScreenWidth(), mGame->ScreenHeight());
		mFrameConstantBuffer.ApplyChanges(context);

		mCloudsConstantBuffer.Data.AmbientColor = XMVECTOR{ mAmbientColor[0], mAmbientColor[1], mAmbientColor[2], 1.0f };
		mCloudsConstantBuffer.Data.WindDir = XMVECTOR{ 1.0f, 0.0f, 0.0f, 1.0f };
		mCloudsConstantBuffer.Data.WindSpeed = mWindSpeedMultiplier;
		mCloudsConstantBuffer.Data.Time = static_cast<float>(gameTime.TotalGameTime());
		mCloudsConstantBuffer.Data.Crispiness = mCrispiness;
		mCloudsConstantBuffer.Data.Curliness = mCurliness;
		mCloudsConstantBuffer.Data.Coverage = mCoverage;
		mCloudsConstantBuffer.Data.Absorption = mLightAbsorption;
		//mVolumetricCloudsCloudsConstantBuffer.Data.CloudsLayerSphereInnerRadius = mCloudsLayerInnerHeight;
		//mVolumetricCloudsCloudsConstantBuffer.Data.CloudsLayerSphereOuterRadius = mCloudsLayerOuterHeight;
		mCloudsConstantBuffer.ApplyChanges(context);

	}

	void VolumetricClouds::UpdateImGui()
	{
		ImGui::Begin("Volumetric Clouds System");

		ImGui::Checkbox("Enabled", &mEnabled);
		ImGui::ColorEdit3("Ambient color", mAmbientColor);
		ImGui::SliderFloat("Sun light absorption", &mLightAbsorption, 0.0f, 0.015f);
		//ImGui::SliderFloat("Clouds bottom height", &mCloudsLayerInnerHeight, 1000.0f, 10000.0f);
		//ImGui::SliderFloat("Clouds top height", &mCloudsLayerOuterHeight, 10000.0f, 50000.0f);
		ImGui::SliderFloat("Crispiness", &mCrispiness, 0.0f, 100.0f);
		ImGui::SliderFloat("Curliness", &mCurliness, 0.0f, 5.0f);
		ImGui::SliderFloat("Coverage", &mCoverage, 0.0f, 1.0f);
		ImGui::SliderFloat("Wind speed factor", &mWindSpeedMultiplier, 0.0f, 10000.0f);

		ImGui::End();
	}

	void VolumetricClouds::Draw(const GameTime& gametime)
	{
		if (!mEnabled)
			return;

		ID3D11DeviceContext* context = mGame->Direct3DDeviceContext();

		//skybox to empty texture
		mPostProcessingStack.BeginRenderingToExtraRT(true);
		mSkybox.Draw(gametime);
		mPostProcessingStack.EndRenderingToExtraRT();

		//main pass
		ID3D11Buffer* CBs[2] = {
			mFrameConstantBuffer.Buffer(),
			mCloudsConstantBuffer.Buffer()
		};
		ID3D11ShaderResourceView* SR[5] = {
			mPostProcessingStack.GetExtraColorOutputTexture(),
			mWeatherTextureSRV,
			mCloudTextureSRV,
			mWorleyTextureSRV,
			mPostProcessingStack.GetDepthOutputTexture(),
		};
		ID3D11SamplerState* SS[2] = { mCloudSS, mWeatherSS };


		if (mUseComputeShaderVersion) {
			ID3D11UnorderedAccessView* UAV[1] = { mMainRenderTargetCS_Custom->getUAV() };

			context->CSSetShaderResources(0, 5, SR);
			context->CSSetConstantBuffers(0, 2, CBs);
			context->CSSetSamplers(0, 2, SS);
			context->CSSetShader(mMainCS, NULL, NULL);
			context->CSSetUnorderedAccessViews(0, 1, UAV, NULL);
			context->Dispatch(INT_CEIL(mGame->ScreenWidth(), 32), INT_CEIL(mGame->ScreenHeight(), 32), 1);

			ID3D11ShaderResourceView* nullSRV[] = { NULL };
			context->CSSetShaderResources(0, 1, nullSRV);
			ID3D11UnorderedAccessView* nullUAV[] = { NULL };
			context->CSSetUnorderedAccessViews(0, 1, nullUAV, 0);
			ID3D11Buffer* nullCBs[] = { NULL };
			context->CSSetConstantBuffers(0, 1, nullCBs);
			ID3D11SamplerState* nullSSs[] = { NULL };
			context->CSSetSamplers(0, 1, nullSSs);
		}
		else {
			mMainRenderTargetPS->Begin();

			context->PSSetShaderResources(0, 5, SR);
			context->PSSetConstantBuffers(0, 2, CBs);
			context->PSSetSamplers(0, 2, SS);
			context->PSSetShader(mMainPS, NULL, NULL);
			mPostProcessingStack.DrawFullscreenQuad(context);
			mMainRenderTargetPS->End();
		}

		//blur pass
		mBlurRenderTarget->Begin();
		ID3D11ShaderResourceView* SR_Blur[1] = {
			(mUseComputeShaderVersion) ? mMainRenderTargetCS_Custom->getSRV() : mMainRenderTargetPS->OutputColorTexture()
		};
		context->PSSetShaderResources(0, 1, SR_Blur);
		context->PSSetShader(mBlurPS, NULL, NULL);
		mPostProcessingStack.DrawFullscreenQuad(context);
		mBlurRenderTarget->End();

		//composite pass
		mCompositeRenderTarget->Begin();
		ID3D11ShaderResourceView* SR_Composite[3] = {
			mPostProcessingStack.GetPrepassColorOutputTexture(),
			mPostProcessingStack.GetDepthOutputTexture(),
			mBlurRenderTarget->OutputColorTexture()
		};
		context->PSSetShaderResources(0, 3, SR_Composite);
		context->PSSetShader(mCompositePS, NULL, NULL);
		mPostProcessingStack.DrawFullscreenQuad(context);
		mCompositeRenderTarget->End();

		//reset main RT 
		mPostProcessingStack.SetMainRT(mCompositeRenderTarget->OutputColorTexture());
	}
}