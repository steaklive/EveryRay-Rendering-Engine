#include "stdafx.h"
#include <stdio.h>
#include "ER_VolumetricClouds.h"
#include "ER_CoreTime.h"
#include "ER_Camera.h"
#include "ER_DirectionalLight.h"
#include "ER_CoreException.h"
#include "ER_Model.h"
#include "ER_Mesh.h"
#include "ER_Core.h"
#include "ER_MatrixHelper.h"
#include "ER_Utility.h"
#include "ER_VertexDeclarations.h"
#include "ER_Skybox.h"
#include "ER_QuadRenderer.h"
#include "ER_Wind.h"

#define MAIN_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define MAIN_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX 1
#define MAIN_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 2

#define UPSAMPLEBLUR_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define UPSAMPLEBLUR_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX 1
#define UPSAMPLEBLUR_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 2

#define COMPOSITE_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0

namespace EveryRay_Core {
	ER_VolumetricClouds::ER_VolumetricClouds(ER_Core& game, ER_Camera& camera, ER_DirectionalLight& light, ER_Skybox& skybox, VolumetricCloudsQuality aQuality)
		: ER_CoreComponent(game),
		mCamera(camera), 
		mDirectionalLight(light),
		mSkybox(skybox),
		mCurrentQuality(aQuality)
	{
	}
	ER_VolumetricClouds::~ER_VolumetricClouds()
	{
		if (mCurrentQuality == VolumetricCloudsQuality::VC_DISABLED)
			return;

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
		DeleteObject(mMainPassRS);
		DeleteObject(mUpsampleBlurPassRS);
		DeleteObject(mCompositePassRS);
		mFrameConstantBuffer.Release();
		mCloudsConstantBuffer.Release();
		mUpsampleBlurConstantBuffer.Release();
	}

	void ER_VolumetricClouds::Initialize(ER_RHI_GPUTexture* aIlluminationDepth) 
	{
		switch (mCurrentQuality)
		{
		case VolumetricCloudsQuality::VC_DISABLED:
			return;
			break;	
		case VolumetricCloudsQuality::VC_LOW:
			mDownscaleFactor = 0.5f;
			break;	
		case VolumetricCloudsQuality::VC_MEDIUM:
			mDownscaleFactor = 0.75f;
			break;
		case VolumetricCloudsQuality::VC_HIGH:
			mDownscaleFactor = 0.9f;
			break;
		}

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

		// root-signatures
		mMainPassRS = rhi->CreateRootSignature(3, 2);
		if (mMainPassRS)
		{
			mMainPassRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP);
			mMainPassRS->InitStaticSampler(rhi, 1, ER_RHI_SAMPLER_STATE::ER_BILINEAR_WRAP);
			mMainPassRS->InitDescriptorTable(rhi, MAIN_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 5 });
			mMainPassRS->InitDescriptorTable(rhi, MAIN_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_UAV }, { 0 }, { 1 });
			mMainPassRS->InitDescriptorTable(rhi, MAIN_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 2 });
			mMainPassRS->Finalize(rhi, "ER_RHI_GPURootSignature: Volumetric Clouds Main Pass");
		}

		mUpsampleBlurPassRS = rhi->CreateRootSignature(3, 2);
		if (mUpsampleBlurPassRS)
		{
			mUpsampleBlurPassRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP);
			mUpsampleBlurPassRS->InitStaticSampler(rhi, 1, ER_RHI_SAMPLER_STATE::ER_BILINEAR_WRAP);
			mUpsampleBlurPassRS->InitDescriptorTable(rhi, MAIN_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 1 });
			mUpsampleBlurPassRS->InitDescriptorTable(rhi, MAIN_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_UAV }, { 0 }, { 1 });
			mUpsampleBlurPassRS->InitDescriptorTable(rhi, MAIN_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 1 });
			mUpsampleBlurPassRS->Finalize(rhi, "ER_RHI_GPURootSignature: Volumetric Clouds Upsample & Blur Pass");
		}

		mCompositePassRS = rhi->CreateRootSignature(1, 1);
		if (mCompositePassRS)
		{
			mCompositePassRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP);
			mCompositePassRS->InitDescriptorTable(rhi, COMPOSITE_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 2 });
			mCompositePassRS->Finalize(rhi, "ER_RHI_GPURootSignature: Volumetric Clouds Composite Pass", true);
		}

		//cbuffers
		mFrameConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: Volumetric Clouds View CB");
		mCloudsConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: Volumetric Clouds Main CB");
		mUpsampleBlurConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: Volumetric Clouds Upsample+Blur CB");

		assert(aIlluminationDepth);
		mIlluminationResultDepthTarget = aIlluminationDepth;

		//textures
		mCloudTextureSRV = rhi->CreateGPUTexture(L"");
		mCloudTextureSRV->CreateGPUTextureResource(rhi, ER_Utility::GetFilePath(L"content\\textures\\VolumetricClouds\\cloud.dds"), true, true);	
		mWeatherTextureSRV = rhi->CreateGPUTexture(L"");
		mWeatherTextureSRV->CreateGPUTextureResource(rhi, ER_Utility::GetFilePath(L"content\\textures\\VolumetricClouds\\weather.dds"), true);
		mWorleyTextureSRV = rhi->CreateGPUTexture(L"");
		mWorleyTextureSRV->CreateGPUTextureResource(rhi, ER_Utility::GetFilePath(L"content\\textures\\VolumetricClouds\\worley.dds"), true, true);

		mMainRT = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Volumetric Clouds Main RT");
		mMainRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore->ScreenWidth()) * mDownscaleFactor, static_cast<UINT>(mCore->ScreenHeight()) * mDownscaleFactor, 1u, ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_SHADER_RESOURCE | ER_BIND_UNORDERED_ACCESS);
	
		mUpsampleAndBlurRT = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Volumetric Clouds Upsample+Blur RT");
		mUpsampleAndBlurRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore->ScreenWidth()), static_cast<UINT>(mCore->ScreenHeight()), 1u, ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_SHADER_RESOURCE | ER_BIND_UNORDERED_ACCESS);
		
		mBlurRT = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Volumetric Clouds Blur RT");
		mBlurRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore->ScreenWidth()), static_cast<UINT>(mCore->ScreenHeight()), 1u, ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET);

		mSkyRT = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Volumetric Clouds Sky RT");
		mSkyRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore->ScreenWidth()), static_cast<UINT>(mCore->ScreenHeight()), 1u, ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET);
		
		mSkyAndSunRT = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Volumetric Clouds Sky + Sun RT");
		mSkyAndSunRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore->ScreenWidth()), static_cast<UINT>(mCore->ScreenHeight()), 1u, ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET);
	}
	
	void ER_VolumetricClouds::Update(const ER_CoreTime& gameTime)
	{
		if (mCurrentQuality == VolumetricCloudsQuality::VC_DISABLED)
			return;

		UpdateImGui();

		if (!mEnabled)
			return;

		auto rhi = mCore->GetRHI();
		const ER_Wind* wind = mCore->GetLevel()->mWind;

		mFrameConstantBuffer.Data.InvProj = XMMatrixInverse(nullptr, mCamera.ProjectionMatrix());
		mFrameConstantBuffer.Data.InvView = XMMatrixInverse(nullptr, mCamera.ViewMatrix());
		mFrameConstantBuffer.Data.LightDir = -mDirectionalLight.DirectionVector();
		mFrameConstantBuffer.Data.LightCol = XMVECTOR{ mDirectionalLight.GetColor().x, mDirectionalLight.GetColor().y, mDirectionalLight.GetColor().z, 1.0f };
		mFrameConstantBuffer.Data.CameraPos = mCamera.PositionVector();
		mFrameConstantBuffer.Data.UpsampleRatio = XMFLOAT2(1.0f / mDownscaleFactor, 1.0f / mDownscaleFactor);
		mFrameConstantBuffer.ApplyChanges(rhi);

		mCloudsConstantBuffer.Data.AmbientColor = XMVECTOR{ mAmbientColor[0], mAmbientColor[1], mAmbientColor[2], 1.0f };
		mCloudsConstantBuffer.Data.WindDir = XMVECTOR{ -wind->Direction().x, -wind->Direction().y, -wind->Direction().z, 1.0f };
		mCloudsConstantBuffer.Data.WindSpeed = mWindSpeedMultiplier * wind->GetStrength();
		mCloudsConstantBuffer.Data.Time = static_cast<float>(gameTime.TotalCoreTime());
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
		if (!mEnabled || mCurrentQuality == VolumetricCloudsQuality::VC_DISABLED)
			return;

		assert(mIlluminationResultDepthTarget);
		auto rhi = mCore->GetRHI();

		rhi->BeginEventTag("EveryRay: Volumetric Clouds (skybox)");
		{
			rhi->SetRenderTargets({ mSkyRT });
			mSkybox.Draw(mSkyRT, nullptr, nullptr, true);
			rhi->UnbindRenderTargets();
		}
		rhi->EndEventTag();

		rhi->BeginEventTag("EveryRay: Volumetric Clouds (sun)");
		{
			rhi->SetRenderTargets({ mSkyAndSunRT });
			mSkybox.DrawSun(mSkyAndSunRT, nullptr, mSkyRT, mIlluminationResultDepthTarget, true);
			rhi->UnbindRenderTargets();
		}
		rhi->EndEventTag();

		ER_QuadRenderer* quadRenderer = (ER_QuadRenderer*)mCore->GetServices().FindService(ER_QuadRenderer::TypeIdClass());
		assert(quadRenderer);

		rhi->BeginEventTag("EveryRay: Volumetric Clouds (main pass)");
		// main pass
		{
			rhi->SetRootSignature(mMainPassRS, true);
			if (!rhi->IsPSOReady(mMainPassPSOName, true))
			{
				rhi->InitializePSO(mMainPassPSOName, true);
				rhi->SetShader(mMainCS);
				rhi->SetRootSignatureToPSO(mMainPassPSOName, mMainPassRS, true);
				rhi->FinalizePSO(mMainPassPSOName, true);
			}
			rhi->SetPSO(mMainPassPSOName, true);
			rhi->SetSamplers(ER_COMPUTE, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SAMPLER_STATE::ER_BILINEAR_WRAP });
			rhi->SetShaderResources(ER_COMPUTE, { mSkyAndSunRT,	mWeatherTextureSRV,	mCloudTextureSRV, mWorleyTextureSRV, mIlluminationResultDepthTarget }, 0,
				mMainPassRS, MAIN_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, true);
			rhi->SetUnorderedAccessResources(ER_COMPUTE, { mMainRT }, 0, mMainPassRS, MAIN_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX, true);
			rhi->SetConstantBuffers(ER_COMPUTE, { mFrameConstantBuffer.Buffer(), mCloudsConstantBuffer.Buffer() }, 0, mMainPassRS, MAIN_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, true);
			rhi->Dispatch(ER_DivideByMultiple(static_cast<UINT>(mMainRT->GetWidth()), 8u), ER_DivideByMultiple(static_cast<UINT>(mMainRT->GetHeight()), 8u), 1u);
			rhi->UnsetPSO();
			
			rhi->UnbindResourcesFromShader(ER_COMPUTE);
		}
		rhi->EndEventTag();

		rhi->BeginEventTag("EveryRay: Volumetric Clouds (upsample+blur)");
		//upsample and blur
		{
			mUpsampleBlurConstantBuffer.Data.Upsample = true;
			mUpsampleBlurConstantBuffer.ApplyChanges(rhi);

			rhi->SetRootSignature(mUpsampleBlurPassRS, true);
			if (!rhi->IsPSOReady(mUpsampleBlurPSOName, true))
			{
				rhi->InitializePSO(mUpsampleBlurPSOName, true);
				rhi->SetShader(mUpsampleBlurCS);
				rhi->SetRootSignatureToPSO(mUpsampleBlurPSOName, mUpsampleBlurPassRS, true);
				rhi->FinalizePSO(mUpsampleBlurPSOName, true);
			}
			rhi->SetPSO(mUpsampleBlurPSOName, true);
			rhi->SetSamplers(ER_COMPUTE, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
			rhi->SetShaderResources(ER_COMPUTE, { mMainRT }, 0, mUpsampleBlurPassRS, UPSAMPLEBLUR_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, true);
			rhi->SetUnorderedAccessResources(ER_COMPUTE, { mUpsampleAndBlurRT }, 0, mUpsampleBlurPassRS, UPSAMPLEBLUR_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX, true);
			rhi->SetConstantBuffers(ER_COMPUTE, { mUpsampleBlurConstantBuffer.Buffer() }, 0, mUpsampleBlurPassRS, UPSAMPLEBLUR_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, true);
			rhi->Dispatch(ER_DivideByMultiple(static_cast<UINT>(mUpsampleAndBlurRT->GetWidth()), 8u), ER_DivideByMultiple(static_cast<UINT>(mUpsampleAndBlurRT->GetHeight()), 8u), 1u);
			rhi->UnsetPSO();
			
			rhi->UnbindResourcesFromShader(ER_COMPUTE);
		}
		rhi->EndEventTag();

		//composite pass (happens in PostProcessing)
	}

	void ER_VolumetricClouds::Composite(ER_RHI_GPUTexture* aRenderTarget)
	{
		if (mCurrentQuality == VolumetricCloudsQuality::VC_DISABLED)
			return;

		ER_QuadRenderer* quadRenderer = (ER_QuadRenderer*)mCore->GetServices().FindService(ER_QuadRenderer::TypeIdClass());
		assert(quadRenderer);

		auto rhi = mCore->GetRHI();

		rhi->SetRenderTargets({ aRenderTarget });
		rhi->SetRootSignature(mCompositePassRS);
		rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		if (!rhi->IsPSOReady(mCompositePassPSOName))
		{
			rhi->InitializePSO(mCompositePassPSOName);
			rhi->SetShader(mCompositePS);
			rhi->SetRenderTargetFormats({ aRenderTarget });
			rhi->SetBlendState(ER_NO_BLEND);
			rhi->SetRasterizerState(ER_NO_CULLING);
			rhi->SetTopologyTypeToPSO(mCompositePassPSOName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			rhi->SetRootSignatureToPSO(mCompositePassPSOName, mCompositePassRS);
			quadRenderer->PrepareDraw(rhi);
			rhi->FinalizePSO(mCompositePassPSOName);
		}
		rhi->SetPSO(mCompositePassPSOName);
		rhi->SetShaderResources(ER_PIXEL, { mIlluminationResultDepthTarget, /*mBlurRT*/mUpsampleAndBlurRT }, 0, mCompositePassRS, COMPOSITE_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);
		quadRenderer->Draw(rhi);
		rhi->UnsetPSO();

		rhi->UnbindRenderTargets();
		rhi->UnbindResourcesFromShader(ER_PIXEL);
	}
}