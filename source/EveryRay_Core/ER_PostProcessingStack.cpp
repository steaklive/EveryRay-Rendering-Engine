#include "ER_PostProcessingStack.h"
#include "ER_CoreException.h"
#include "ER_Utility.h"
#include "ER_ColorHelper.h"
#include "ER_MatrixHelper.h"
#include "ER_DirectionalLight.h"
#include "ER_QuadRenderer.h"
#include "ER_GBuffer.h"
#include "ER_Camera.h"
#include "ER_VolumetricClouds.h"
#include "ER_VolumetricFog.h"
#include "ER_Illumination.h"

#define LINEARFOG_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define LINEARFOG_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 1

#define SSS_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define SSS_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 1

#define SSR_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define SSR_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 1

#define TONEMAP_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0

#define COLORGRADING_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0

#define VIGNETTE_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define VIGNETTE_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 1

#define FXAA_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define FXAA_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 1

#define FINALRESOLVE_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0

namespace EveryRay_Core {

	ER_PostProcessingStack::ER_PostProcessingStack(ER_Core& pCore, ER_Camera& pCamera)
		: mCore(pCore), camera(pCamera)
	{
	}

	ER_PostProcessingStack::~ER_PostProcessingStack()
	{
		DeleteObject(mTonemappingRT);
		DeleteObject(mSSRRT);
		DeleteObject(mSSSRT);
		DeleteObject(mColorGradingRT);
		DeleteObject(mVignetteRT);
		DeleteObject(mFXAART);
		DeleteObject(mLinearFogRT);
		DeleteObject(mVolumetricFogRT);

		DeleteObject(mTonemappingPS);
		DeleteObject(mSSRPS);
		DeleteObject(mSSSPS);
		DeleteObject(mColorGradingPS);
		DeleteObject(mVignettePS);
		DeleteObject(mFXAAPS);
		DeleteObject(mLinearFogPS);
		DeleteObject(mFinalResolvePS);

		DeleteObject(mTonemapRS);
		DeleteObject(mSSRRS);
		DeleteObject(mSSSRS);
		DeleteObject(mColorGradingRS);
		DeleteObject(mVignetteRS);
		DeleteObject(mFXAARS);
		DeleteObject(mLinearFogRS);
		DeleteObject(mFinalResolveRS);

		mSSRConstantBuffer.Release();
		mSSSConstantBuffer.Release();
		mFXAAConstantBuffer.Release();
		mVignetteConstantBuffer.Release();
		mLinearFogConstantBuffer.Release();
	}

	void ER_PostProcessingStack::Initialize(bool pTonemap, bool pMotionBlur, bool pColorGrading, bool pVignette, bool pFXAA, bool pSSR, bool pFog, bool pLightShafts)
	{
		auto rhi = mCore.GetRHI();

		mFinalResolvePS = rhi->CreateGPUShader();
		mFinalResolvePS->CompileShader(rhi, "content\\shaders\\EmptyColorResolve.hlsl", "PSMain", ER_PIXEL);
		mFinalResolveRS = rhi->CreateRootSignature(1, 1);
		if (mFinalResolveRS)
		{
			mFinalResolveRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_PIXEL);
			mFinalResolveRS->InitDescriptorTable(rhi, FINALRESOLVE_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
			mFinalResolveRS->Finalize(rhi, "ER_RHI_GPURootSignature: Final Resolve Pass", true);
		}

		//Linear fog
		{
			mLinearFogPS = rhi->CreateGPUShader();
			mLinearFogPS->CompileShader(rhi, "content\\shaders\\LinearFog.hlsl", "PSMain", ER_PIXEL);
			
			mLinearFogConstantBuffer.Initialize(rhi);

			mLinearFogRT = rhi->CreateGPUTexture();
			mLinearFogRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore.ScreenWidth()), static_cast<UINT>(mCore.ScreenHeight()), 1u,
				ER_FORMAT_R11G11B10_FLOAT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, 1);

			mLinearFogRS = rhi->CreateRootSignature(2, 1);
			if (mLinearFogRS)
			{
				mLinearFogRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mLinearFogRS->InitDescriptorTable(rhi, LINEARFOG_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 2 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mLinearFogRS->InitDescriptorTable(rhi, LINEARFOG_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mLinearFogRS->Finalize(rhi, "ER_RHI_GPURootSignature: Linear Fog Pass", true);
			}
		}

		mVolumetricFogRT = rhi->CreateGPUTexture();
		mVolumetricFogRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore.ScreenWidth()), static_cast<UINT>(mCore.ScreenHeight()), 1u,
			ER_FORMAT_R11G11B10_FLOAT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, 1);

		//SSR
		{
			mSSRPS = rhi->CreateGPUShader();
			mSSRPS->CompileShader(rhi, "content\\shaders\\SSR.hlsl", "PSMain", ER_PIXEL);

			mSSRConstantBuffer.Initialize(rhi);
			
			mSSRRT = rhi->CreateGPUTexture();
			mSSRRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore.ScreenWidth()), static_cast<UINT>(mCore.ScreenHeight()), 1u,
				ER_FORMAT_R11G11B10_FLOAT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, 1);

			mSSRRS = rhi->CreateRootSignature(2, 1);
			if (mSSRRS)
			{
				mSSRRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mSSRRS->InitDescriptorTable(rhi, SSR_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 4 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mSSRRS->InitDescriptorTable(rhi, SSR_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mSSRRS->Finalize(rhi, "ER_RHI_GPURootSignature: SSR Pass", true);
			}
		}

		//SSS
		{
			mSSSPS = rhi->CreateGPUShader();
			mSSSPS->CompileShader(rhi, "content\\shaders\\SSS.hlsl", "BlurPS", ER_PIXEL);

			mSSSConstantBuffer.Initialize(rhi);

			mSSSRT = rhi->CreateGPUTexture();
			mSSSRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore.ScreenWidth()), static_cast<UINT>(mCore.ScreenHeight()), 1u,
				ER_FORMAT_R11G11B10_FLOAT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, 1);

			mSSSRS = rhi->CreateRootSignature(2, 1);
			if (mSSSRS)
			{
				mSSSRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mSSSRS->InitDescriptorTable(rhi, SSS_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 3 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mSSSRS->InitDescriptorTable(rhi, SSS_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mSSSRS->Finalize(rhi, "ER_RHI_GPURootSignature: SSS Pass", true);
			}
		}

		//Tonemap
		{		
			mTonemappingPS = rhi->CreateGPUShader();
			mTonemappingPS->CompileShader(rhi, "content\\shaders\\Tonemap.hlsl", "PSMain", ER_PIXEL);

			mTonemappingRT = rhi->CreateGPUTexture();
			mTonemappingRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore.ScreenWidth()), static_cast<UINT>(mCore.ScreenHeight()), 1u,
				ER_FORMAT_R11G11B10_FLOAT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, 1);

			mTonemapRS = rhi->CreateRootSignature(1, 1);
			if (mTonemapRS)
			{
				mTonemapRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mTonemapRS->InitDescriptorTable(rhi, TONEMAP_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mTonemapRS->Finalize(rhi, "ER_RHI_GPURootSignature: Tonemap Pass", true);
			}
		}

		//Color grading
		{
			mLUTs[0] = rhi->CreateGPUTexture();
			mLUTs[0]->CreateGPUTextureResource(rhi, "content\\shaders\\LUT_1.png");
			mLUTs[1] = rhi->CreateGPUTexture();
			mLUTs[1]->CreateGPUTextureResource(rhi, "content\\shaders\\LUT_2.png");
			mLUTs[2] = rhi->CreateGPUTexture();
			mLUTs[2]->CreateGPUTextureResource(rhi, "content\\shaders\\LUT_3.png");
			
			mColorGradingPS = rhi->CreateGPUShader();
			mColorGradingPS->CompileShader(rhi, "content\\shaders\\ColorGrading.hlsl", "PSMain", ER_PIXEL);

			mColorGradingRT = rhi->CreateGPUTexture();
			mColorGradingRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore.ScreenWidth()), static_cast<UINT>(mCore.ScreenHeight()), 1u,
				ER_FORMAT_R11G11B10_FLOAT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, 1);

			mColorGradingRS = rhi->CreateRootSignature(1, 0);
			if (mColorGradingRS)
			{
				mColorGradingRS->InitDescriptorTable(rhi, COLORGRADING_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 2 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mColorGradingRS->Finalize(rhi, "ER_RHI_GPURootSignature: Color Grading Pass", true);
			}
		}

		//Vignette
		{
			mVignettePS = rhi->CreateGPUShader();
			mVignettePS->CompileShader(rhi, "content\\shaders\\Vignette.hlsl", "PSMain", ER_PIXEL);

			mVignetteConstantBuffer.Initialize(rhi);
			
			mVignetteRT = rhi->CreateGPUTexture();
			mVignetteRT->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore.ScreenWidth()), static_cast<UINT>(mCore.ScreenHeight()), 1u,
				ER_FORMAT_R11G11B10_FLOAT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, 1);

			mVignetteRS = rhi->CreateRootSignature(2, 1);
			if (mVignetteRS)
			{
				mVignetteRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mVignetteRS->InitDescriptorTable(rhi, VIGNETTE_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mVignetteRS->InitDescriptorTable(rhi, VIGNETTE_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mVignetteRS->Finalize(rhi, "ER_RHI_GPURootSignature: Vignette Pass", true);
			}
		}	
		
		//FXAA
		{
			mFXAAPS = rhi->CreateGPUShader();
			mFXAAPS->CompileShader(rhi, "content\\shaders\\FXAA.hlsl", "PSMain", ER_PIXEL);

			mFXAAConstantBuffer.Initialize(rhi);

			mFXAART = rhi->CreateGPUTexture();
			mFXAART->CreateGPUTextureResource(rhi, static_cast<UINT>(mCore.ScreenWidth()), static_cast<UINT>(mCore.ScreenHeight()), 1u,
				ER_FORMAT_R11G11B10_FLOAT, ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, 1);

			mFXAARS = rhi->CreateRootSignature(2, 1);
			if (mFXAARS)
			{
				mFXAARS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mFXAARS->InitDescriptorTable(rhi, FXAA_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mFXAARS->InitDescriptorTable(rhi, FXAA_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				mFXAARS->Finalize(rhi, "ER_RHI_GPURootSignature: FXAA Pass", true);
			}
		}
	}

	void ER_PostProcessingStack::Update()
	{	
		ShowPostProcessingWindow();
	}

	void ER_PostProcessingStack::ShowPostProcessingWindow()
	{
		if (!mShowDebug)
			return;

		ImGui::Begin("Post Processing Stack Config");

		if (ImGui::CollapsingHeader("Linear Fog"))
		{
			ImGui::Checkbox("Fog - On", &mUseLinearFog);
			ImGui::ColorEdit3("Color", mLinearFogColor);
			ImGui::SliderFloat("Density", &mLinearFogDensity, 1.0f, 10000.0f);
		}

		if (ImGui::CollapsingHeader("Separable Subsurface Scattering"))
		{
			ER_Illumination* illumination = mCore.GetLevel()->mIllumination;

			ImGui::Checkbox("SSS - On", &mUseSSS);
			illumination->SetSSS(mUseSSS);

			float strength = illumination->GetSSSStrength();
			ImGui::SliderFloat("SSS Strength", &strength, 0.0f, 15.0f);
			illumination->SetSSSStrength(strength);

			float translucency = illumination->GetSSSTranslucency();
			ImGui::SliderFloat("SSS Translucency", &translucency, 0.0f, 1.0f);
			illumination->SetSSSTranslucency(translucency);

			float width = illumination->GetSSSWidth();
			ImGui::SliderFloat("SSS Width", &width, 0.0f, 0.1f);
			illumination->SetSSSWidth(width);

			float lightPlaneScale = illumination->GetSSSDirLightPlaneScale();
			ImGui::SliderFloat("Dir light plane scale", &lightPlaneScale, 0.0f, 10.0f);
			illumination->SetSSSDirLightPlaneScale(lightPlaneScale);
		}

		if (ImGui::CollapsingHeader("Screen Space Reflections"))
		{
			ImGui::Checkbox("SSR - On", &mUseSSR);
			ImGui::SliderInt("Ray count", &mSSRRayCount, 0, 100);
			ImGui::SliderFloat("Step Size", &mSSRStepSize, 0.0f, 10.0f);
			ImGui::SliderFloat("Max Thickness", &mSSRMaxThickness, 0.0f, 0.01f);
		}

		if (ImGui::CollapsingHeader("Tonemap"))
		{
			ImGui::Checkbox("Tonemap - On", &mUseTonemap);
		}

		if (ImGui::CollapsingHeader("Color Grading"))
		{
			ImGui::Checkbox("Color Grading - On", &mUseColorGrading);

			ImGui::TextWrapped("Current LUT");
			ImGui::RadioButton("LUT 1", &mColorGradingCurrentLUTIndex, 0);
			ImGui::RadioButton("LUT 2", &mColorGradingCurrentLUTIndex, 1);
			ImGui::RadioButton("LUT 3", &mColorGradingCurrentLUTIndex, 2);
		}

		if (ImGui::CollapsingHeader("Vignette"))
		{
			ImGui::Checkbox("Vignette - On", &mUseVignette);
			ImGui::SliderFloat("Radius", &mVignetteRadius, 0.000f, 1.0f);
			ImGui::SliderFloat("Softness", &mVignetteSoftness, 0.000f, 1.0f);
		}

		if (ImGui::CollapsingHeader("FXAA"))
		{
			ImGui::Checkbox("FXAA - On", &mUseFXAA);
		}

		ImGui::End();
	}
	
	void ER_PostProcessingStack::Begin(ER_RHI_GPUTexture* aInitialRT, ER_RHI_GPUTexture* aDepthTarget)
	{
		assert(aInitialRT && aDepthTarget);
		mRenderTargetBeforePostProcessingPasses = aInitialRT;
		mDepthTarget = aDepthTarget;

		auto rhi = mCore.GetRHI();
		rhi->SetRenderTargets({ aInitialRT }, aDepthTarget);
	}

	void ER_PostProcessingStack::End(ER_RHI_GPUTexture* aResolveRT)
	{
		auto rhi = mCore.GetRHI();
		rhi->UnbindRenderTargets();

		rhi->SetMainRenderTargets();

		rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		rhi->SetRootSignature(mFinalResolveRS);

		//final resolve to main RT (pre-UI)
		{
			ER_QuadRenderer* quad = (ER_QuadRenderer*)mCore.GetServices().FindService(ER_QuadRenderer::TypeIdClass());
			assert(quad);
			assert(mRenderTargetBeforeResolve);

			if (!rhi->IsPSOReady(mFinalResolvePassPSOName))
			{
				rhi->InitializePSO(mFinalResolvePassPSOName);
				rhi->SetShader(mFinalResolvePS);
				rhi->SetMainRenderTargetFormats();
				rhi->SetBlendState(ER_NO_BLEND);
				rhi->SetDepthStencilState(ER_DEPTH_ONLY_WRITE_COMPARISON_LESS_EQUAL);
				rhi->SetRasterizerState(ER_NO_CULLING);
				rhi->SetTopologyTypeToPSO(mFinalResolvePassPSOName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				rhi->SetRootSignatureToPSO(mFinalResolvePassPSOName, mFinalResolveRS);
				quad->PrepareDraw(rhi);
				rhi->FinalizePSO(mFinalResolvePassPSOName);
			}
			rhi->SetPSO(mFinalResolvePassPSOName);
			rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
			rhi->SetShaderResources(ER_PIXEL, { aResolveRT ? aResolveRT : mRenderTargetBeforeResolve }, 0, mFinalResolveRS, FINALRESOLVE_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);
			quad->Draw(rhi);
			rhi->UnsetPSO();
		}

		rhi->UnbindResourcesFromShader(ER_PIXEL);
	}

	void ER_PostProcessingStack::PrepareDrawingTonemapping(ER_RHI_GPUTexture* aInputTexture)
	{
		assert(aInputTexture);
		auto rhi = mCore.GetRHI();

		rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
		rhi->SetShaderResources(ER_PIXEL, { aInputTexture }, 0, mTonemapRS, TONEMAP_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);
	}

	void ER_PostProcessingStack::PrepareDrawingSSR(const ER_CoreTime& gameTime, ER_RHI_GPUTexture* aInputTexture, ER_GBuffer* gbuffer)
	{
		assert(aInputTexture);
		auto rhi = mCore.GetRHI();

		mSSRConstantBuffer.Data.InvProjMatrix = XMMatrixTranspose(XMMatrixInverse(nullptr, camera.ProjectionMatrix()));
		mSSRConstantBuffer.Data.InvViewMatrix = XMMatrixTranspose(XMMatrixInverse(nullptr, camera.ViewMatrix()));
		mSSRConstantBuffer.Data.ViewMatrix = XMMatrixTranspose(camera.ViewMatrix());
		mSSRConstantBuffer.Data.ProjMatrix = XMMatrixTranspose(camera.ProjectionMatrix());
		mSSRConstantBuffer.Data.CameraPosition = XMFLOAT4(camera.Position().x,camera.Position().y,camera.Position().z,1.0f);
		mSSRConstantBuffer.Data.StepSize = mSSRStepSize;
		mSSRConstantBuffer.Data.MaxThickness = mSSRMaxThickness;
		mSSRConstantBuffer.Data.Time = static_cast<float>(gameTime.TotalCoreTime());
		mSSRConstantBuffer.Data.MaxRayCount = mSSRRayCount;
		mSSRConstantBuffer.ApplyChanges(rhi);

		rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
		rhi->SetShaderResources(ER_PIXEL, { aInputTexture, gbuffer->GetNormals(), gbuffer->GetExtraBuffer(), mDepthTarget }, 0,
			mSSSRS, SSR_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);
		rhi->SetConstantBuffers(ER_PIXEL, { mSSRConstantBuffer.Buffer() }, 0, mSSRRS, SSR_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
	}

	void ER_PostProcessingStack::PrepareDrawingSSS(const ER_CoreTime& gameTime, ER_RHI_GPUTexture* aInputTexture, ER_GBuffer* gbuffer, bool verticalPass)
	{
		auto rhi = mCore.GetRHI();

		ER_Illumination* illumination = mCore.GetLevel()->mIllumination;
		assert(illumination);
		assert(aInputTexture);

		if (verticalPass)
			mSSSConstantBuffer.Data.SSSStrengthWidthDir = XMFLOAT4(illumination->GetSSSStrength(), illumination->GetSSSWidth(), 1.0f, 0.0f);
		else
			mSSSConstantBuffer.Data.SSSStrengthWidthDir = XMFLOAT4(illumination->GetSSSStrength(), illumination->GetSSSWidth(), 0.0f, 1.0f);
		mSSSConstantBuffer.Data.CameraFOV = camera.FieldOfView();
		mSSSConstantBuffer.ApplyChanges(rhi);

		rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
		rhi->SetShaderResources(ER_PIXEL, { aInputTexture, mDepthTarget, gbuffer->GetExtra2Buffer() }, 0, mSSSRS, SSS_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);
		rhi->SetConstantBuffers(ER_PIXEL, { mSSSConstantBuffer.Buffer() }, 0, mSSSRS, SSS_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
	}

	void ER_PostProcessingStack::PrepareDrawingLinearFog(ER_RHI_GPUTexture* aInputTexture)
	{
		assert(aInputTexture);
		auto rhi = mCore.GetRHI();

		mLinearFogConstantBuffer.Data.FogColor = XMFLOAT4{ mLinearFogColor[0], mLinearFogColor[1], mLinearFogColor[2], 1.0f };
		mLinearFogConstantBuffer.Data.FogNear = camera.NearPlaneDistance();
		mLinearFogConstantBuffer.Data.FogFar = camera.FarPlaneDistance();
		mLinearFogConstantBuffer.Data.FogDensity = mLinearFogDensity;
		mLinearFogConstantBuffer.ApplyChanges(rhi);

		rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
		rhi->SetShaderResources(ER_PIXEL, { aInputTexture, mDepthTarget }, 0, mLinearFogRS, LINEARFOG_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);
		rhi->SetConstantBuffers(ER_PIXEL, { mLinearFogConstantBuffer.Buffer() }, 0, mLinearFogRS, LINEARFOG_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
	}

	void ER_PostProcessingStack::PrepareDrawingColorGrading(ER_RHI_GPUTexture* aInputTexture)
	{
		assert(aInputTexture);
		auto rhi = mCore.GetRHI();

		rhi->SetShaderResources(ER_PIXEL, { mLUTs[mColorGradingCurrentLUTIndex], aInputTexture }, 0, mColorGradingRS, COLORGRADING_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);
	}

	void ER_PostProcessingStack::PrepareDrawingVignette(ER_RHI_GPUTexture* aInputTexture)
	{
		assert(aInputTexture);
		auto rhi = mCore.GetRHI();

		mVignetteConstantBuffer.Data.RadiusSoftness = XMFLOAT2(mVignetteRadius, mVignetteSoftness);
		mVignetteConstantBuffer.ApplyChanges(rhi);

		rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
		rhi->SetConstantBuffers(ER_PIXEL, { mVignetteConstantBuffer.Buffer() }, 0, mVignetteRS, VIGNETTE_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
		rhi->SetShaderResources(ER_PIXEL, { aInputTexture }, 0, mVignetteRS, VIGNETTE_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);
	}

	void ER_PostProcessingStack::PrepareDrawingFXAA(ER_RHI_GPUTexture* aInputTexture)
	{
		assert(aInputTexture);
		auto rhi = mCore.GetRHI();

		mFXAAConstantBuffer.Data.ScreenDimensions = XMFLOAT2(static_cast<float>(mCore.ScreenWidth()),static_cast<float>(mCore.ScreenHeight()));
		mFXAAConstantBuffer.ApplyChanges(rhi);

		rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
		rhi->SetShaderResources(ER_PIXEL, { aInputTexture }, 0, mFXAARS, FXAA_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);
		rhi->SetConstantBuffers(ER_PIXEL, { mFXAAConstantBuffer.Buffer() }, 0, mFXAARS, FXAA_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
	}

	void ER_PostProcessingStack::DrawEffects(const ER_CoreTime& gameTime, ER_QuadRenderer* quad, ER_GBuffer* gbuffer, ER_VolumetricClouds* aVolumetricClouds, ER_VolumetricFog* aVolumetricFog)
	{
		assert(quad);
		assert(gbuffer);
		auto rhi = mCore.GetRHI();

		mRenderTargetBeforeResolve = mRenderTargetBeforePostProcessingPasses;
		rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Linear fog
		if (mUseLinearFog)
		{
			rhi->SetRenderTargets({ mLinearFogRT });
			rhi->SetRootSignature(mLinearFogRS);
			if (!rhi->IsPSOReady(mLinearFogPassPSOName))
			{
				rhi->InitializePSO(mLinearFogPassPSOName);
				rhi->SetShader(mLinearFogPS);
				rhi->SetRenderTargetFormats({ mLinearFogRT });
				rhi->SetBlendState(ER_NO_BLEND);
				rhi->SetRasterizerState(ER_NO_CULLING);
				rhi->SetRootSignatureToPSO(mLinearFogPassPSOName, mLinearFogRS);
				rhi->SetTopologyTypeToPSO(mLinearFogPassPSOName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				quad->PrepareDraw(rhi);
				rhi->FinalizePSO(mLinearFogPassPSOName);
			}
			rhi->SetPSO(mLinearFogPassPSOName);
			PrepareDrawingLinearFog(mRenderTargetBeforeResolve);
			quad->Draw(rhi);
			rhi->UnsetPSO();
			
			rhi->UnbindRenderTargets();

			//[WARNING] Set from last post processing effect
			mRenderTargetBeforeResolve = mLinearFogRT;
		}

		// SSS
		if (mUseSSS)
		{
			ER_Illumination* illumination = mCore.GetLevel()->mIllumination;
			if (illumination->IsSSSBlurring())
			{
				rhi->SetRenderTargets({ mSSSRT });
				rhi->SetRootSignature(mSSSRS);
				if (!rhi->IsPSOReady(mSSSPassPSOName))
				{
					rhi->InitializePSO(mSSSPassPSOName);
					rhi->SetShader(mSSSPS);
					rhi->SetRenderTargetFormats({ mSSSRT });
					rhi->SetBlendState(ER_NO_BLEND);
					rhi->SetRasterizerState(ER_NO_CULLING);
					rhi->SetRootSignatureToPSO(mSSSPassPSOName, mSSSRS);
					rhi->SetTopologyTypeToPSO(mSSSPassPSOName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					quad->PrepareDraw(rhi);
					rhi->FinalizePSO(mSSSPassPSOName);
				}
				rhi->SetPSO(mSSSPassPSOName);

				//vertical
				{
					PrepareDrawingSSS(gameTime, mRenderTargetBeforeResolve, gbuffer, true);
					quad->Draw(rhi);
				}
				//horizontal
				{
					PrepareDrawingSSS(gameTime, mRenderTargetBeforeResolve, gbuffer, false);
					quad->Draw(rhi);
				}
				rhi->UnsetPSO();

				rhi->UnbindRenderTargets();
				//[WARNING] Set from last post processing effect
				mRenderTargetBeforeResolve = mSSSRT;
			}
		}

		// SSR
		if (mUseSSR)
		{
			rhi->SetRenderTargets({ mSSRRT });
			rhi->SetRootSignature(mSSRRS);
			if (!rhi->IsPSOReady(mSSRPassPSOName))
			{
				rhi->InitializePSO(mSSRPassPSOName);
				rhi->SetShader(mSSRPS);
				rhi->SetRenderTargetFormats({ mSSRRT });
				rhi->SetBlendState(ER_NO_BLEND);
				rhi->SetRasterizerState(ER_NO_CULLING);
				rhi->SetRootSignatureToPSO(mSSRPassPSOName, mSSRRS);
				rhi->SetTopologyTypeToPSO(mSSRPassPSOName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				quad->PrepareDraw(rhi);
				rhi->FinalizePSO(mSSRPassPSOName);
			}
			rhi->SetPSO(mSSRPassPSOName);
			PrepareDrawingSSR(gameTime, mRenderTargetBeforeResolve, gbuffer);
			quad->Draw(rhi);
			rhi->UnsetPSO();

			rhi->UnbindRenderTargets();

			//[WARNING] Set from last post processing effect
			mRenderTargetBeforeResolve = mSSRRT;
		}

		// Composite with volumetric clouds (if enabled)
		if (aVolumetricClouds && aVolumetricClouds->IsEnabled())
			aVolumetricClouds->Composite(mRenderTargetBeforeResolve);
		
		// Composite with volumetric fog (if enabled)
		if (aVolumetricFog && aVolumetricFog->IsEnabled())
		{
			rhi->SetRenderTargets({ mVolumetricFogRT });
			aVolumetricFog->Composite(mVolumetricFogRT, mRenderTargetBeforeResolve, gbuffer->GetPositions());
			rhi->UnbindRenderTargets();

			//[WARNING] Set from last post processing effect
			mRenderTargetBeforeResolve = mVolumetricFogRT;
		}

		// Tonemap
		if (mUseTonemap)
		{
			rhi->SetRenderTargets({ mTonemappingRT });
			rhi->SetRootSignature(mTonemapRS);
			if (!rhi->IsPSOReady(mTonemapPassPSOName))
			{
				rhi->InitializePSO(mTonemapPassPSOName);
				rhi->SetShader(mTonemappingPS);
				rhi->SetBlendState(ER_NO_BLEND);
				rhi->SetRasterizerState(ER_NO_CULLING);
				rhi->SetRenderTargetFormats({ mTonemappingRT });
				rhi->SetRootSignatureToPSO(mTonemapPassPSOName, mTonemapRS);
				rhi->SetTopologyTypeToPSO(mTonemapPassPSOName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				quad->PrepareDraw(rhi);
				rhi->FinalizePSO(mTonemapPassPSOName);
			}
			rhi->SetPSO(mTonemapPassPSOName);
			PrepareDrawingTonemapping(mRenderTargetBeforeResolve);
			quad->Draw(rhi);
			rhi->UnsetPSO();

			rhi->UnbindRenderTargets();

			//[WARNING] Set from last post processing effect
			mRenderTargetBeforeResolve = mTonemappingRT;
		}

		// Color grading
		if (mUseColorGrading)
		{
			rhi->SetRenderTargets({ mColorGradingRT });
			rhi->SetRootSignature(mColorGradingRS);
			if (!rhi->IsPSOReady(mColorGradingPassPSOName))
			{
				rhi->InitializePSO(mColorGradingPassPSOName);
				rhi->SetShader(mColorGradingPS);
				rhi->SetRenderTargetFormats({ mColorGradingRT });
				rhi->SetBlendState(ER_NO_BLEND);
				rhi->SetRasterizerState(ER_NO_CULLING);
				rhi->SetRootSignatureToPSO(mColorGradingPassPSOName, mColorGradingRS);
				rhi->SetTopologyTypeToPSO(mColorGradingPassPSOName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				quad->PrepareDraw(rhi);
				rhi->FinalizePSO(mColorGradingPassPSOName);
			}
			rhi->SetPSO(mColorGradingPassPSOName);
			PrepareDrawingColorGrading(mRenderTargetBeforeResolve);
			quad->Draw(rhi);
			rhi->UnsetPSO();

			rhi->UnbindRenderTargets();

			//[WARNING] Set from last post processing effect
			mRenderTargetBeforeResolve = mColorGradingRT;
		}

		// Vignette
		if (mUseVignette)
		{
			rhi->SetRenderTargets({ mVignetteRT });
			rhi->SetRootSignature(mVignetteRS);
			if (!rhi->IsPSOReady(mVignettePassPSOName))
			{
				rhi->InitializePSO(mVignettePassPSOName);
				rhi->SetShader(mVignettePS);
				rhi->SetBlendState(ER_NO_BLEND);
				rhi->SetRasterizerState(ER_NO_CULLING);
				rhi->SetRenderTargetFormats({ mVignetteRT });
				rhi->SetRootSignatureToPSO(mVignettePassPSOName, mVignetteRS);
				rhi->SetTopologyTypeToPSO(mVignettePassPSOName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				quad->PrepareDraw(rhi);
				rhi->FinalizePSO(mVignettePassPSOName);
			}
			rhi->SetPSO(mVignettePassPSOName);
			PrepareDrawingVignette(mRenderTargetBeforeResolve);
			quad->Draw(rhi);
			rhi->UnsetPSO();

			rhi->UnbindRenderTargets();

			//[WARNING] Set from last post processing effect
			mRenderTargetBeforeResolve = mVignetteRT;
		}

		// FXAA
		if (mUseFXAA)
		{
			rhi->SetRenderTargets({ mFXAART });
			rhi->SetRootSignature(mFXAARS);
			if (!rhi->IsPSOReady(mFXAAPassPSOName))
			{
				rhi->InitializePSO(mFXAAPassPSOName);
				rhi->SetShader(mFXAAPS);
				rhi->SetBlendState(ER_NO_BLEND);
				rhi->SetRasterizerState(ER_NO_CULLING);
				rhi->SetRenderTargetFormats({ mFXAART });
				rhi->SetRootSignatureToPSO(mFXAAPassPSOName, mFXAARS);
				rhi->SetTopologyTypeToPSO(mFXAAPassPSOName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				quad->PrepareDraw(rhi);
				rhi->FinalizePSO(mFXAAPassPSOName);
			}
			rhi->SetPSO(mFXAAPassPSOName);
			PrepareDrawingFXAA(mRenderTargetBeforeResolve);
			quad->Draw(rhi);
			rhi->UnsetPSO();

			rhi->UnbindRenderTargets();

			//[WARNING] Set from last post processing effect 
			mRenderTargetBeforeResolve = mFXAART;
		}
	}
}