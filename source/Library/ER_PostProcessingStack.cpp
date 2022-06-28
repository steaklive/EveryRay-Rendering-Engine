#include "ER_PostProcessingStack.h"
#include "ShaderCompiler.h"
#include "GameException.h"
#include "Utility.h"
#include "ColorHelper.h"
#include "MatrixHelper.h"
#include "DirectionalLight.h"
#include "ER_QuadRenderer.h"
#include "SamplerStates.h"
#include "ER_GBuffer.h"
#include "ER_Camera.h"
#include "ER_VolumetricClouds.h"
#include "ER_VolumetricFog.h"
#include "ER_Illumination.h"

namespace Library {

	ER_PostProcessingStack::ER_PostProcessingStack(Game& pGame, ER_Camera& pCamera)
		: game(pGame), camera(pCamera)
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

		ReleaseObject(mTonemappingPS);
		ReleaseObject(mSSRPS);
		ReleaseObject(mSSSPS);
		ReleaseObject(mColorGradingPS);
		ReleaseObject(mVignettePS);
		ReleaseObject(mFXAAPS);
		ReleaseObject(mLinearFogPS);
		ReleaseObject(mFinalResolvePS);

		mSSRConstantBuffer.Release();
		mSSSConstantBuffer.Release();
		mFXAAConstantBuffer.Release();
		mVignetteConstantBuffer.Release();
		mLinearFogConstantBuffer.Release();
	}

	void ER_PostProcessingStack::Initialize(bool pTonemap, bool pMotionBlur, bool pColorGrading, bool pVignette, bool pFXAA, bool pSSR, bool pFog, bool pLightShafts)
	{
		ID3DBlob* pShaderBlob = nullptr;
		if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\EmptyColorResolve.hlsl").c_str(), "PSMain", "ps_5_0", &pShaderBlob)))
			throw GameException("Failed to load PSMain pass from shader: EmptyColorResolve.hlsl!");
		if (FAILED(game.Direct3DDevice()->CreatePixelShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &mFinalResolvePS)))
			throw GameException("Failed to create PSMain shader from EmptyColorResolve.hlsl!");
		pShaderBlob->Release();


		//Linear fog
		{
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\LinearFog.hlsl").c_str(), "PSMain", "ps_5_0", &pShaderBlob)))
				throw GameException("Failed to load PSMain pass from shader: LinearFog.hlsl!");
			if (FAILED(game.Direct3DDevice()->CreatePixelShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &mLinearFogPS)))
				throw GameException("Failed to create PSMain shader from LinearFog.hlsl!");
			pShaderBlob->Release();

			mLinearFogConstantBuffer.Initialize(game.Direct3DDevice());
			mLinearFogRT = new ER_GPUTexture(game.Direct3DDevice(), static_cast<UINT>(game.ScreenWidth()), static_cast<UINT>(game.ScreenHeight()), 1u,
				DXGI_FORMAT_R11G11B10_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 1);
		}

		mVolumetricFogRT = new ER_GPUTexture(game.Direct3DDevice(), static_cast<UINT>(game.ScreenWidth()), static_cast<UINT>(game.ScreenHeight()), 1u,
			DXGI_FORMAT_R11G11B10_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 1);

		//SSR
		{
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\SSR.hlsl").c_str(), "PSMain", "ps_5_0", &pShaderBlob)))
				throw GameException("Failed to load PSMain pass from shader: SSR.hlsl!");
			if (FAILED(game.Direct3DDevice()->CreatePixelShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &mSSRPS)))
				throw GameException("Failed to create PSMain shader from SSR.hlsl!");
			pShaderBlob->Release();

			mSSRConstantBuffer.Initialize(game.Direct3DDevice());
			mSSRRT = new ER_GPUTexture(game.Direct3DDevice(), static_cast<UINT>(game.ScreenWidth()), static_cast<UINT>(game.ScreenHeight()), 1u,
				DXGI_FORMAT_R11G11B10_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 1);
		}

		//SSS
		{
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\SSS.hlsl").c_str(), "BlurPS", "ps_5_0", &pShaderBlob)))
				throw GameException("Failed to load BlurPS pass from shader: SSS.hlsl!");
			if (FAILED(game.Direct3DDevice()->CreatePixelShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &mSSSPS)))
				throw GameException("Failed to create BlurPS shader from SSS.hlsl!");
			pShaderBlob->Release();

			mSSSConstantBuffer.Initialize(game.Direct3DDevice());
			mSSSRT = new ER_GPUTexture(game.Direct3DDevice(), static_cast<UINT>(game.ScreenWidth()), static_cast<UINT>(game.ScreenHeight()), 1u,
				DXGI_FORMAT_R11G11B10_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 1);
		}

		//Tonemap
		{
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Tonemap.hlsl").c_str(), "PSMain", "ps_5_0", &pShaderBlob)))
				throw GameException("Failed to load PSMain pass from shader: Tonemap.hlsl!");
			if (FAILED(game.Direct3DDevice()->CreatePixelShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &mTonemappingPS)))
				throw GameException("Failed to create PSMain shader from Tonemap.hlsl!");
			pShaderBlob->Release();

			//mSSRConstantBuffer.Initialize(game.Direct3DDevice());
			mTonemappingRT = new ER_GPUTexture(game.Direct3DDevice(), static_cast<UINT>(game.ScreenWidth()), static_cast<UINT>(game.ScreenHeight()), 1u,
				DXGI_FORMAT_R11G11B10_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 1);
		}

		//Color grading
		{
			std::wstring LUTtexture1 = Utility::GetFilePath(L"content\\shaders\\LUT_1.png");
			if (FAILED(DirectX::CreateWICTextureFromFile(game.Direct3DDevice(), game.Direct3DDeviceContext(), LUTtexture1.c_str(), nullptr, &mLUTs[0])))
				throw GameException("Failed to load LUT1 texture.");

			std::wstring LUTtexture2 = Utility::GetFilePath(L"content\\shaders\\LUT_2.png");
			if (FAILED(DirectX::CreateWICTextureFromFile(game.Direct3DDevice(), game.Direct3DDeviceContext(), LUTtexture2.c_str(), nullptr, &mLUTs[1])))
				throw GameException("Failed to load LUT2 texture.");

			std::wstring LUTtexture3 = Utility::GetFilePath(L"content\\shaders\\LUT_3.png");
			if (FAILED(DirectX::CreateWICTextureFromFile(game.Direct3DDevice(), game.Direct3DDeviceContext(), LUTtexture3.c_str(), nullptr, &mLUTs[2])))
				throw GameException("Failed to load LUT3 texture.");

			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\ColorGrading.hlsl").c_str(), "PSMain", "ps_5_0", &pShaderBlob)))
				throw GameException("Failed to load PSMain pass from shader: ColorGrading.hlsl!");
			if (FAILED(game.Direct3DDevice()->CreatePixelShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &mColorGradingPS)))
				throw GameException("Failed to create PSMain shader from ColorGrading.hlsl!");
			pShaderBlob->Release();

			mColorGradingRT = new ER_GPUTexture(game.Direct3DDevice(), static_cast<UINT>(game.ScreenWidth()), static_cast<UINT>(game.ScreenHeight()), 1u,
				DXGI_FORMAT_R11G11B10_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 1);
		}

		//Vignette
		{
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Vignette.hlsl").c_str(), "PSMain", "ps_5_0", &pShaderBlob)))
				throw GameException("Failed to load PSMain pass from shader: Vignette.hlsl!");
			if (FAILED(game.Direct3DDevice()->CreatePixelShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &mVignettePS)))
				throw GameException("Failed to create PSMain shader from Vignette.hlsl!");
			pShaderBlob->Release();

			mVignetteConstantBuffer.Initialize(game.Direct3DDevice());
			mVignetteRT = new ER_GPUTexture(game.Direct3DDevice(), static_cast<UINT>(game.ScreenWidth()), static_cast<UINT>(game.ScreenHeight()), 1u,
				DXGI_FORMAT_R11G11B10_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 1);
		}	
		
		//FXAA
		{
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\FXAA.hlsl").c_str(), "PSMain", "ps_5_0", &pShaderBlob)))
				throw GameException("Failed to load PSMain pass from shader: FXAA.hlsl!");
			if (FAILED(game.Direct3DDevice()->CreatePixelShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &mFXAAPS)))
				throw GameException("Failed to create PSMain shader from FXAA.hlsl!");
			pShaderBlob->Release();

			mFXAAConstantBuffer.Initialize(game.Direct3DDevice());
			mFXAART = new ER_GPUTexture(game.Direct3DDevice(), static_cast<UINT>(game.ScreenWidth()), static_cast<UINT>(game.ScreenHeight()), 1u,
				DXGI_FORMAT_R11G11B10_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 1);
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
			ER_Illumination* illumination = game.GetLevel()->mIllumination;

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
	
	void ER_PostProcessingStack::Begin(ER_GPUTexture* aInitialRT, DepthTarget* aDepthTarget)
	{
		assert(aInitialRT && aDepthTarget);
		mRenderTargetBeforePostProcessingPasses = aInitialRT;
		mDepthTarget = aDepthTarget;
		game.Direct3DDeviceContext()->OMSetRenderTargets(1, aInitialRT->GetRTVs(), aDepthTarget->getDSV());
	}

	void ER_PostProcessingStack::End(ER_GPUTexture* aResolveRT)
	{
		game.ResetRenderTargets();

		//final resolve to main RT (pre-UI)
		{
			ER_QuadRenderer* quad = (ER_QuadRenderer*)game.Services().GetService(ER_QuadRenderer::TypeIdClass());
			assert(quad);
			assert(mRenderTargetBeforeResolve);

			auto context = game.Direct3DDeviceContext();

			ID3D11ShaderResourceView* SR[1] = { aResolveRT ? aResolveRT->GetSRV() : mRenderTargetBeforeResolve->GetSRV() };
			context->PSSetShaderResources(0, 1, SR);
			ID3D11SamplerState* SS[1] = { SamplerStates::TrilinearWrap };
			context->PSSetSamplers(0, 1, SS);
			context->PSSetShader(mFinalResolvePS, NULL, NULL);
			quad->Draw(context);
		}
	}

	void ER_PostProcessingStack::PrepareDrawingTonemapping(ER_GPUTexture* aInputTexture)
	{
		assert(aInputTexture);
		auto context = game.Direct3DDeviceContext();

		//mSSRConstantBuffer.Data.InvProjMatrix = XMMatrixTranspose(XMMatrixInverse(nullptr, camera.ProjectionMatrix()));
		//mSSRConstantBuffer.Data.InvViewMatrix = XMMatrixTranspose(XMMatrixInverse(nullptr, camera.ViewMatrix()));
		//mSSRConstantBuffer.Data.ViewMatrix = XMMatrixTranspose(camera.ViewMatrix());
		//mSSRConstantBuffer.Data.ProjMatrix = XMMatrixTranspose(camera.ProjectionMatrix());
		//mSSRConstantBuffer.Data.CameraPosition = XMFLOAT4(camera.Position().x, camera.Position().y, camera.Position().z, 1.0f);
		//mSSRConstantBuffer.Data.StepSize = mSSRStepSize;
		//mSSRConstantBuffer.Data.MaxThickness = mSSRMaxThickness;
		//mSSRConstantBuffer.Data.Time = static_cast<float>(gameTime.TotalGameTime());
		//mSSRConstantBuffer.Data.MaxRayCount = mSSRRayCount;
		//mSSRConstantBuffer.ApplyChanges(context);
		//ID3D11Buffer* CBs[1] = { mSSRConstantBuffer.Buffer() };
		//context->PSSetConstantBuffers(0, 1, CBs);

		context->PSSetShader(mTonemappingPS, NULL, NULL);

		ID3D11SamplerState* SS[1] = { SamplerStates::TrilinearWrap };
		context->PSSetSamplers(0, 1, SS);

		ID3D11ShaderResourceView* SRs[1] = { aInputTexture->GetSRV() };
		context->PSSetShaderResources(0, 1, SRs);
	}

	void ER_PostProcessingStack::PrepareDrawingSSR(const GameTime& gameTime, ER_GPUTexture* aInputTexture, ER_GBuffer* gbuffer)
	{
		assert(aInputTexture);
		auto context = game.Direct3DDeviceContext();

		mSSRConstantBuffer.Data.InvProjMatrix = XMMatrixTranspose(XMMatrixInverse(nullptr, camera.ProjectionMatrix()));
		mSSRConstantBuffer.Data.InvViewMatrix = XMMatrixTranspose(XMMatrixInverse(nullptr, camera.ViewMatrix()));
		mSSRConstantBuffer.Data.ViewMatrix = XMMatrixTranspose(camera.ViewMatrix());
		mSSRConstantBuffer.Data.ProjMatrix = XMMatrixTranspose(camera.ProjectionMatrix());
		mSSRConstantBuffer.Data.CameraPosition = XMFLOAT4(camera.Position().x,camera.Position().y,camera.Position().z,1.0f);
		mSSRConstantBuffer.Data.StepSize = mSSRStepSize;
		mSSRConstantBuffer.Data.MaxThickness = mSSRMaxThickness;
		mSSRConstantBuffer.Data.Time = static_cast<float>(gameTime.TotalGameTime());
		mSSRConstantBuffer.Data.MaxRayCount = mSSRRayCount;
		mSSRConstantBuffer.ApplyChanges(context);
		ID3D11Buffer* CBs[1] = { mSSRConstantBuffer.Buffer() };
		context->PSSetConstantBuffers(0, 1, CBs);

		context->PSSetShader(mSSRPS, NULL, NULL);

		ID3D11SamplerState* SS[1] = { SamplerStates::TrilinearWrap };
		context->PSSetSamplers(0, 1, SS);

		ID3D11ShaderResourceView* SRs[4] = { aInputTexture->GetSRV(), gbuffer->GetNormals()->GetSRV(), gbuffer->GetExtraBuffer()->GetSRV(), mDepthTarget->getSRV() };
		context->PSSetShaderResources(0, 4, SRs);
	}

	void ER_PostProcessingStack::PrepareDrawingSSS(const GameTime& gameTime, ER_GPUTexture* aInputTexture, ER_GBuffer* gbuffer, bool verticalPass)
	{
		auto context = game.Direct3DDeviceContext();

		ER_Illumination* illumination = game.GetLevel()->mIllumination;
		assert(illumination);
		assert(aInputTexture);

		if (verticalPass)
			mSSSConstantBuffer.Data.SSSStrengthWidthDir = XMFLOAT4(illumination->GetSSSStrength(), illumination->GetSSSWidth(), 1.0f, 0.0f);
		else
			mSSSConstantBuffer.Data.SSSStrengthWidthDir = XMFLOAT4(illumination->GetSSSStrength(), illumination->GetSSSWidth(), 0.0f, 1.0f);
		mSSSConstantBuffer.Data.CameraFOV = camera.FieldOfView();
		mSSSConstantBuffer.ApplyChanges(context);
		ID3D11Buffer* CBs[1] = { mSSSConstantBuffer.Buffer() };
		context->PSSetConstantBuffers(0, 1, CBs);

		context->PSSetShader(mSSSPS, NULL, NULL);

		ID3D11SamplerState* SS[1] = { SamplerStates::TrilinearWrap };
		context->PSSetSamplers(0, 1, SS);

		ID3D11ShaderResourceView* SRs[3] = { aInputTexture->GetSRV(), mDepthTarget->getSRV(), gbuffer->GetExtra2Buffer()->GetSRV() };
		context->PSSetShaderResources(0, 3, SRs);
	}

	void ER_PostProcessingStack::PrepareDrawingLinearFog(ER_GPUTexture* aInputTexture)
	{
		assert(aInputTexture);
		auto context = game.Direct3DDeviceContext();

		mLinearFogConstantBuffer.Data.FogColor = XMFLOAT4{ mLinearFogColor[0], mLinearFogColor[1], mLinearFogColor[2], 1.0f };
		mLinearFogConstantBuffer.Data.FogNear = camera.NearPlaneDistance();
		mLinearFogConstantBuffer.Data.FogFar = camera.FarPlaneDistance();
		mLinearFogConstantBuffer.Data.FogDensity = mLinearFogDensity;
		mLinearFogConstantBuffer.ApplyChanges(context);
		ID3D11Buffer* CBs[1] = { mLinearFogConstantBuffer.Buffer() };
		context->PSSetConstantBuffers(0, 1, CBs);

		context->PSSetShader(mLinearFogPS, NULL, NULL);

		ID3D11SamplerState* SS[1] = { SamplerStates::TrilinearWrap };
		context->PSSetSamplers(0, 1, SS);

		ID3D11ShaderResourceView* SRs[2] = { aInputTexture->GetSRV(), mDepthTarget->getSRV() };
		context->PSSetShaderResources(0, 2, SRs);
	}

	void ER_PostProcessingStack::PrepareDrawingColorGrading(ER_GPUTexture* aInputTexture)
	{
		assert(aInputTexture);
		auto context = game.Direct3DDeviceContext();

		context->PSSetShader(mColorGradingPS, NULL, NULL);

		ID3D11ShaderResourceView* SRs[2] = { mLUTs[mColorGradingCurrentLUTIndex], aInputTexture->GetSRV() };
		context->PSSetShaderResources(0, 2, SRs);
	}

	void ER_PostProcessingStack::PrepareDrawingVignette(ER_GPUTexture* aInputTexture)
	{
		assert(aInputTexture);
		auto context = game.Direct3DDeviceContext();

		mVignetteConstantBuffer.Data.RadiusSoftness = XMFLOAT2(mVignetteRadius, mVignetteSoftness);
		mVignetteConstantBuffer.ApplyChanges(context);
		ID3D11Buffer* CBs[1] = { mVignetteConstantBuffer.Buffer() };
		context->PSSetConstantBuffers(0, 1, CBs);

		context->PSSetShader(mVignettePS, NULL, NULL);

		ID3D11SamplerState* SS[1] = { SamplerStates::TrilinearWrap };
		context->PSSetSamplers(0, 1, SS);

		ID3D11ShaderResourceView* SRs[1] = { aInputTexture->GetSRV() };
		context->PSSetShaderResources(0, 1, SRs);
	}

	void ER_PostProcessingStack::PrepareDrawingFXAA(ER_GPUTexture* aInputTexture)
	{
		assert(aInputTexture);
		auto context = game.Direct3DDeviceContext();

		mFXAAConstantBuffer.Data.ScreenDimensions = XMFLOAT2(static_cast<float>(game.ScreenWidth()),static_cast<float>(game.ScreenHeight()));
		mFXAAConstantBuffer.ApplyChanges(context);
		ID3D11Buffer* CBs[1] = { mFXAAConstantBuffer.Buffer() };
		context->PSSetConstantBuffers(0, 1, CBs);

		context->PSSetShader(mFXAAPS, NULL, NULL);

		ID3D11SamplerState* SS[1] = { SamplerStates::TrilinearWrap };
		context->PSSetSamplers(0, 1, SS);

		ID3D11ShaderResourceView* SRs[1] = { aInputTexture->GetSRV() };
		context->PSSetShaderResources(0, 1, SRs);
	}

	void ER_PostProcessingStack::DrawEffects(const GameTime& gameTime, ER_QuadRenderer* quad, ER_GBuffer* gbuffer, ER_VolumetricClouds* aVolumetricClouds, ER_VolumetricFog* aVolumetricFog)
	{
		assert(quad);
		assert(gbuffer);
		ID3D11DeviceContext* context = game.Direct3DDeviceContext();

		mRenderTargetBeforeResolve = mRenderTargetBeforePostProcessingPasses;

		// Linear fog
		if (mUseLinearFog)
		{
			game.SetCustomRenderTarget(mLinearFogRT, nullptr);
			PrepareDrawingLinearFog(mRenderTargetBeforeResolve);
			quad->Draw(context);
			game.UnsetCustomRenderTarget();

			//[WARNING] Set from last post processing effect
			mRenderTargetBeforeResolve = mLinearFogRT;
		}

		// SSS
		if (mUseSSS)
		{
			ER_Illumination* illumination = game.GetLevel()->mIllumination;
			if (illumination->IsSSSBlurring())
			{
				//vertical
				{
					game.SetCustomRenderTarget(mSSSRT, nullptr);
					PrepareDrawingSSS(gameTime, mRenderTargetBeforeResolve, gbuffer, true);
					quad->Draw(context);
					game.UnsetCustomRenderTarget();
				}
				//horizontal
				{
					game.SetCustomRenderTarget(mSSSRT, nullptr);
					PrepareDrawingSSS(gameTime, mRenderTargetBeforeResolve, gbuffer, false);
					quad->Draw(context);
					game.UnsetCustomRenderTarget();
				}
				//[WARNING] Set from last post processing effect
				mRenderTargetBeforeResolve = mSSSRT;
			}
		}

		// SSR
		if (mUseSSR)
		{
			game.SetCustomRenderTarget(mSSRRT, nullptr);
			PrepareDrawingSSR(gameTime, mRenderTargetBeforeResolve, gbuffer);
			quad->Draw(context);
			game.UnsetCustomRenderTarget();

			//[WARNING] Set from last post processing effect
			mRenderTargetBeforeResolve = mSSRRT;
		}

		// Composite with volumetric clouds (if enabled)
		if (aVolumetricClouds && aVolumetricClouds->IsEnabled())
			aVolumetricClouds->Composite(mRenderTargetBeforeResolve);
		
		// Composite with volumetric fog (if enabled)
		if (aVolumetricFog && aVolumetricFog->IsEnabled())
		{
			game.SetCustomRenderTarget(mVolumetricFogRT, nullptr);
			aVolumetricFog->Composite(mRenderTargetBeforeResolve, gbuffer->GetPositions());
			game.UnsetCustomRenderTarget();

			//[WARNING] Set from last post processing effect
			mRenderTargetBeforeResolve = mVolumetricFogRT;
		}

		// Tonemap
		if (mUseTonemap)
		{
			game.SetCustomRenderTarget(mTonemappingRT, nullptr);
			PrepareDrawingTonemapping(mRenderTargetBeforeResolve);
			quad->Draw(context);
			game.UnsetCustomRenderTarget();

			//[WARNING] Set from last post processing effect
			mRenderTargetBeforeResolve = mTonemappingRT;
		}

		// Color grading
		if (mUseColorGrading)
		{
			game.SetCustomRenderTarget(mColorGradingRT, nullptr);
			PrepareDrawingColorGrading(mRenderTargetBeforeResolve);
			quad->Draw(context);
			game.UnsetCustomRenderTarget();

			//[WARNING] Set from last post processing effect
			mRenderTargetBeforeResolve = mColorGradingRT;
		}
		// Vignette
		if (mUseVignette)
		{
			game.SetCustomRenderTarget(mVignetteRT, nullptr);
			PrepareDrawingVignette(mRenderTargetBeforeResolve);
			quad->Draw(context);
			game.UnsetCustomRenderTarget();

			//[WARNING] Set from last post processing effect
			mRenderTargetBeforeResolve = mVignetteRT;
		}
		// FXAA
		if (mUseFXAA)
		{
			game.SetCustomRenderTarget(mFXAART, nullptr);
			PrepareDrawingFXAA(mRenderTargetBeforeResolve);
			quad->Draw(context);
			game.UnsetCustomRenderTarget();

			//[WARNING] Set from last post processing effect 
			mRenderTargetBeforeResolve = mFXAART;
		}
	}
}