#include "stdafx.h"
#include <stdio.h>
#include "ER_VolumetricClouds.h"
#include "ER_CoreTime.h"
#include "ER_Camera.h"
#include "DirectionalLight.h"
#include "ER_CoreException.h"
#include "ER_Model.h"
#include "ER_Mesh.h"
#include "ER_Core.h"
#include "ER_MatrixHelper.h"
#include "ER_Utility.h"
#include "ER_VertexDeclarations.h"
#include "ER_Skybox.h"
#include "ER_QuadRenderer.h"

namespace Library {
	ER_VolumetricClouds::ER_VolumetricClouds(ER_Core& game, ER_Camera& camera, DirectionalLight& light, ER_Skybox& skybox)
		: ER_CoreComponent(game),
		mCamera(camera), 
		mDirectionalLight(light),
		mSkybox(skybox)
	{
	}
	ER_VolumetricClouds::~ER_VolumetricClouds()
	{
		DeleteObject(mCloudTextureSRV);
		DeleteObject(mWeatherTextureSRV);
		DeleteObject(mWorleyTextureSRV);
		DeleteObject(mMainCS);
		DeleteObject(mCompositePS);
		DeleteObject(mBlurPS);
		DeleteObject(mMainRT);
		DeleteObject(mSkyRT);
		DeleteObject(mSkyAndSunRT);
		DeleteObject(mUpsampleAndBlurRT);
		DeleteObject(mBlurRT);
		mFrameConstantBuffer.Release();
		mCloudsConstantBuffer.Release();
		mUpsampleBlurConstantBuffer.Release();
	}

	void ER_VolumetricClouds::Initialize(ER_RHI_GPUTexture* aIlluminationDepth) 
	{
		//shaders
		auto rhi = GetCore()->GetRHI();

		mCompositePS = rhi->CreateGPUShader();
		mCompositePS->CompileShader(rhi, "content\\shaders\\VolumetricClouds\\VolumetricCloudsComposite.hlsl", "main", ER_PIXEL);

		mBlurPS = rhi->CreateGPUShader();
		mBlurPS->CompileShader(rhi, "content\\shaders\\VolumetricClouds\\VolumetricCloudsBlur.hlsl", "main", ER_PIXEL);

		mMainCS = rhi->CreateGPUShader();
		mMainCS->CompileShader(rhi, "content\\shaders\\VolumetricClouds\\VolumetricCloudsCS.hlsl", "main", ER_COMPUTE);

		mUpsampleBlurCS = rhi->CreateGPUShader();
		mUpsampleBlurCS->CompileShader(rhi, "content\\shaders\\UpsampleBlur.hlsl", "CSMain", ER_COMPUTE);


		//cbuffers
		mFrameConstantBuffer.Initialize(rhi);
		mCloudsConstantBuffer.Initialize(rhi);
		mUpsampleBlurConstantBuffer.Initialize(rhi);

		assert(aIlluminationDepth);
		mIlluminationResultDepthTarget = aIlluminationDepth;

		//textures
		mCloudTextureSRV = rhi->CreateGPUTexture();
		mCloudTextureSRV->CreateGPUTextureResource(rhi, ER_Utility::GetFilePath(L"content\\textures\\VolumetricClouds\\cloud.dds"), true, true);	
		mWeatherTextureSRV = rhi->CreateGPUTexture();
		mWeatherTextureSRV->CreateGPUTextureResource(rhi, ER_Utility::GetFilePath(L"content\\textures\\VolumetricClouds\\weather.dds"), true);
		mWorleyTextureSRV = rhi->CreateGPUTexture();
		mWorleyTextureSRV->CreateGPUTextureResource(rhi, ER_Utility::GetFilePath(L"content\\textures\\VolumetricClouds\\worley.dds"), true, true);

		mMainRT = rhi->CreateGPUTexture();
		mMainRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore->ScreenWidth()) * mDownscaleFactor, static_cast<UINT>(mCore->ScreenHeight()) * mDownscaleFactor, 1u, ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_SHADER_RESOURCE | ER_BIND_UNORDERED_ACCESS);
	
		mUpsampleAndBlurRT = rhi->CreateGPUTexture();
		mUpsampleAndBlurRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore->ScreenWidth()), static_cast<UINT>(mCore->ScreenHeight()), 1u, ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_SHADER_RESOURCE | ER_BIND_UNORDERED_ACCESS);
		
		mBlurRT = rhi->CreateGPUTexture();
		mBlurRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore->ScreenWidth()), static_cast<UINT>(mCore->ScreenHeight()), 1u, ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET);

		mSkyRT = rhi->CreateGPUTexture();
		mSkyRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore->ScreenWidth()), static_cast<UINT>(mCore->ScreenHeight()), 1u, ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET);
		
		mSkyAndSunRT = rhi->CreateGPUTexture();
		mSkyAndSunRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore->ScreenWidth()), static_cast<UINT>(mCore->ScreenHeight()), 1u, ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET);
	}
	
	void ER_VolumetricClouds::Update(const ER_CoreTime& gameTime)
	{
		UpdateImGui();

		if (!mEnabled)
			return;

		auto rhi = mCore->GetRHI();

		mFrameConstantBuffer.Data.InvProj = XMMatrixInverse(nullptr, mCamera.ProjectionMatrix());
		mFrameConstantBuffer.Data.InvView = XMMatrixInverse(nullptr, mCamera.ViewMatrix());
		mFrameConstantBuffer.Data.LightDir = -mDirectionalLight.DirectionVector();
		mFrameConstantBuffer.Data.LightCol = XMVECTOR{ mDirectionalLight.GetDirectionalLightColor().x, mDirectionalLight.GetDirectionalLightColor().y, mDirectionalLight.GetDirectionalLightColor().z, 1.0f };
		mFrameConstantBuffer.Data.CameraPos = mCamera.PositionVector();
		mFrameConstantBuffer.Data.UpsampleRatio = XMFLOAT2(1.0f / mDownscaleFactor, 1.0f / mDownscaleFactor);
		mFrameConstantBuffer.ApplyChanges(rhi);

		mCloudsConstantBuffer.Data.AmbientColor = XMVECTOR{ mAmbientColor[0], mAmbientColor[1], mAmbientColor[2], 1.0f };
		mCloudsConstantBuffer.Data.WindDir = XMVECTOR{ 1.0f, 0.0f, 0.0f, 1.0f };
		mCloudsConstantBuffer.Data.WindSpeed = mWindSpeedMultiplier;
		mCloudsConstantBuffer.Data.Time = static_cast<float>(gameTime.TotalGameTime());
		mCloudsConstantBuffer.Data.Crispiness = mCrispiness;
		mCloudsConstantBuffer.Data.Curliness = mCurliness;
		mCloudsConstantBuffer.Data.Coverage = mCoverage;
		mCloudsConstantBuffer.Data.Absorption = mLightAbsorption;
		mCloudsConstantBuffer.Data.CloudsBottomHeight = mCloudsBottomHeight;
		mCloudsConstantBuffer.Data.CloudsTopHeight = mCloudsTopHeight;
		mCloudsConstantBuffer.Data.DensityFactor = mDensityFactor;
		mCloudsConstantBuffer.ApplyChanges(rhi);

		mUpsampleBlurConstantBuffer.Data.Upsample = true;
		mUpsampleBlurConstantBuffer.ApplyChanges(rhi);
	}

	void ER_VolumetricClouds::UpdateImGui()
	{
		if (!mShowDebug)
			return;

		ImGui::Begin("Volumetric Clouds System");
		ImGui::Checkbox("Enabled", &mEnabled);
		ImGui::ColorEdit3("Ambient color", mAmbientColor);
		ImGui::SliderFloat("Sun light absorption", &mLightAbsorption, 0.0f, 0.015f);
		ImGui::SliderFloat("Clouds bottom height", &mCloudsBottomHeight, 1000.0f, 10000.0f);
		ImGui::SliderFloat("Clouds top height", &mCloudsTopHeight, 10000.0f, 50000.0f);
		ImGui::SliderFloat("Density", &mDensityFactor, 0.0f, 0.5f);
		ImGui::SliderFloat("Crispiness", &mCrispiness, 0.0f, 100.0f);
		ImGui::SliderFloat("Curliness", &mCurliness, 0.0f, 5.0f);
		ImGui::SliderFloat("Coverage", &mCoverage, 0.0f, 1.0f);
		ImGui::SliderFloat("Wind speed factor", &mWindSpeedMultiplier, 0.0f, 10000.0f);
		ImGui::End();
	}

	void ER_VolumetricClouds::Draw(const ER_CoreTime& gametime)
	{
		if (!mEnabled)
			return;

		assert(mIlluminationResultDepthTarget);
		auto rhi = mCore->GetRHI();

		rhi->SetRenderTargets({ mSkyRT });
		mSkybox.Draw();
		rhi->UnbindRenderTargets();

		rhi->SetRenderTargets({ mSkyAndSunRT });
		mSkybox.DrawSun(nullptr, mSkyRT, mIlluminationResultDepthTarget);
		rhi->UnbindRenderTargets();

		ER_QuadRenderer* quadRenderer = (ER_QuadRenderer*)mCore->GetServices().FindService(ER_QuadRenderer::TypeIdClass());
		assert(quadRenderer);

		// main pass
		{
			rhi->SetShader(mMainCS);
			rhi->SetSamplers(ER_COMPUTE, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SAMPLER_STATE::ER_BILINEAR_WRAP });
			rhi->SetUnorderedAccessResources(ER_COMPUTE, { mMainRT });
			rhi->SetShaderResources(ER_COMPUTE, { mSkyAndSunRT,	mWeatherTextureSRV,	mCloudTextureSRV, mWorleyTextureSRV, mIlluminationResultDepthTarget });
			rhi->SetConstantBuffers(ER_COMPUTE, { mFrameConstantBuffer.Buffer(), mCloudsConstantBuffer.Buffer() });
			rhi->Dispatch(DivideByMultiple(static_cast<UINT>(mMainRT->GetWidth()), 8u), DivideByMultiple(static_cast<UINT>(mMainRT->GetHeight()), 8u), 1u);
			rhi->UnbindResourcesFromShader(ER_COMPUTE);
		}

		//upsample and blur
		{
			mUpsampleBlurConstantBuffer.Data.Upsample = true;
			mUpsampleBlurConstantBuffer.ApplyChanges(rhi);

			rhi->SetShader(mUpsampleBlurCS);
			rhi->SetSamplers(ER_COMPUTE, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
			rhi->SetUnorderedAccessResources(ER_COMPUTE, { mUpsampleAndBlurRT });
			rhi->SetShaderResources(ER_COMPUTE, { mMainRT });
			rhi->SetConstantBuffers(ER_COMPUTE, { mUpsampleBlurConstantBuffer.Buffer() });
			rhi->Dispatch(DivideByMultiple(static_cast<UINT>(mUpsampleAndBlurRT->GetWidth()), 8u), DivideByMultiple(static_cast<UINT>(mUpsampleAndBlurRT->GetHeight()), 8u), 1u);
			rhi->UnbindResourcesFromShader(ER_COMPUTE);
		}

		//composite pass (happens in PostProcessing)
	}

	void ER_VolumetricClouds::Composite(ER_RHI_GPUTexture* aRenderTarget)
	{
		auto rhi = mCore->GetRHI();

		rhi->SetRenderTargets({ aRenderTarget });

		rhi->SetShader(mCompositePS);
		rhi->SetShaderResources(ER_PIXEL, { mIlluminationResultDepthTarget, /*mBlurRT*/mUpsampleAndBlurRT });
		
		ER_QuadRenderer* quadRenderer = (ER_QuadRenderer*)mCore->GetServices().FindService(ER_QuadRenderer::TypeIdClass());
		assert(quadRenderer);
		quadRenderer->Draw(rhi);

		rhi->UnbindRenderTargets();
		rhi->UnbindResourcesFromShader(ER_PIXEL);
	}
}