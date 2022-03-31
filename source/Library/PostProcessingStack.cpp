#include "PostProcessingStack.h"
#include "ScreenSpaceReflectionsMaterial.h"
#include "LightShaftsMaterial.h"
#include "FullScreenQuad.h"
#include "FullScreenRenderTarget.h"
#include "ShaderCompiler.h"
#include "GameException.h"
#include "Utility.h"
#include "ColorHelper.h"
#include "MatrixHelper.h"
#include "DirectionalLight.h"
#include "ER_QuadRenderer.h"
#include "SamplerStates.h"

namespace Rendering {
	const XMVECTORF32 ClearBackgroundColor = ColorHelper::Black;

	PostProcessingStack::PostProcessingStack(Game& pGame, Camera& pCamera)
		:
		mSSREffect(nullptr),
		mLightShaftsEffect(nullptr),
		game(pGame), camera(pCamera)
	{
	}

	PostProcessingStack::~PostProcessingStack()
	{
		DeleteObject(mSSREffect);
		DeleteObject(mLightShaftsEffect);

		DeleteObject(mSSRRenderTarget);
		DeleteObject(mLightShaftsRenderTarget);

		DeleteObject(mExtraRenderTarget);

		DeleteObject(mColorGradingRT);
		DeleteObject(mVignetteRT);
		DeleteObject(mFXAART);
		DeleteObject(mLinearFogRT);

		ReleaseObject(mColorGradingPS);
		ReleaseObject(mVignettePS);
		ReleaseObject(mFXAAPS);
		ReleaseObject(mLinearFogPS);

		ReleaseObject(mFinalResolvePS);

		mFXAAConstantBuffer.Release();
	}

	void PostProcessingStack::Initialize(bool pTonemap, bool pMotionBlur, bool pColorGrading, bool pVignette, bool pFXAA, bool pSSR, bool pFog, bool pLightShafts)
	{
		mExtraRenderTarget = new FullScreenRenderTarget(game);

		Effect* lightShaftsFX = new Effect(game);
 		lightShaftsFX->CompileFromFile(Utility::GetFilePath(L"content\\effects\\LightShafts.fx"));
		mLightShaftsEffect = new EffectElements::LightShaftsEffect();
		mLightShaftsEffect->Material = new LightShaftsMaterial();
		mLightShaftsEffect->Material->Initialize(lightShaftsFX);
		mLightShaftsEffect->Quad = new FullScreenQuad(game, *mLightShaftsEffect->Material);
		mLightShaftsEffect->Quad->Initialize();
		mLightShaftsEffect->Quad->SetActiveTechnique("light_shafts", "p0");
		mLightShaftsEffect->Quad->SetCustomUpdateMaterial(std::bind(&PostProcessingStack::UpdateLightShaftsMaterial, this));
		mLightShaftsLoaded = true;
		mLightShaftsRenderTarget = new FullScreenRenderTarget(game);
		mLightShaftsEffect->OutputTexture = mLightShaftsRenderTarget->OutputColorTexture();
		mLightShaftsEffect->isActive = pLightShafts;

		Effect* ssrEffectFX = new Effect(game);
		ssrEffectFX->CompileFromFile(Utility::GetFilePath(L"content\\effects\\SSR.fx"));
		mSSREffect = new EffectElements::SSREffect();
		mSSREffect->Material = new ScreenSpaceReflectionsMaterial();
		mSSREffect->Material->Initialize(ssrEffectFX);
		mSSREffect->Quad = new FullScreenQuad(game, *mSSREffect->Material);
		mSSREffect->Quad->Initialize();
		mSSREffect->Quad->SetActiveTechnique("ssr", "p0");
		//mSSREffect->Quad->SetCustomUpdateMaterial(std::bind(&PostProcessingStack::UpdateSSRMaterial, this));
		mSSRRenderTarget = new FullScreenRenderTarget(game);
		mSSREffect->InputColor = mSSRRenderTarget->OutputColorTexture();
		mSSREffect->isActive = pSSR;
		mSSRLoaded = true;

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
				DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 1);
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
				DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 1);
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
				DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 1);
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
				DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 1);
		}

	}

	void PostProcessingStack::Update()
	{	
		ShowPostProcessingWindow();
	}

	void PostProcessingStack::UpdateSSRMaterial(ID3D11ShaderResourceView* normal, ID3D11ShaderResourceView* depth, ID3D11ShaderResourceView* extra, float time)
	{
		XMMATRIX vp = camera.ViewMatrix() * camera.ProjectionMatrix();

		mSSREffect->Material->ColorTexture() << mSSREffect->InputColor;
		mSSREffect->Material->GBufferDepth() << depth;
		mSSREffect->Material->GBufferNormals() << normal;
		mSSREffect->Material->GBufferExtra() << extra;
		mSSREffect->Material->ViewProjection() << vp;
		mSSREffect->Material->InvProjMatrix() << XMMatrixInverse(nullptr, camera.ProjectionMatrix());
		mSSREffect->Material->InvViewMatrix() << XMMatrixInverse(nullptr, camera.ViewMatrix());
		mSSREffect->Material->ViewMatrix() << camera.ViewMatrix();
		mSSREffect->Material->ProjMatrix() << camera.ProjectionMatrix();
		mSSREffect->Material->CameraPosition() << camera.PositionVector();
		mSSREffect->Material->StepSize() << mSSREffect->stepSize;
		mSSREffect->Material->MaxThickness() << mSSREffect->maxThickness;
		mSSREffect->Material->Time() << time;
		mSSREffect->Material->MaxRayCount() << mSSREffect->rayCount;
	}
	
	void PostProcessingStack::UpdateLightShaftsMaterial()
	{
		mLightShaftsEffect->isSunOnScreen = (mSunNDCPos.x >= 0.0f && mSunNDCPos.x <= 1.0f && mSunNDCPos.y >= 0.0f && mSunNDCPos.y <= 1.0f);

		mLightShaftsEffect->Material->ColorTexture() << mLightShaftsEffect->OutputTexture;
		mLightShaftsEffect->Material->DepthTexture() << mLightShaftsEffect->DepthTexture;
		mLightShaftsEffect->Material->OcclusionTexture() << mSunOcclusionSRV;
		mLightShaftsEffect->Material->SunPosNDC() << XMVECTOR{ mSunNDCPos.x,mSunNDCPos.y, 0, 0 };
		mLightShaftsEffect->Material->Decay() << mLightShaftsEffect->decay;
		mLightShaftsEffect->Material->Exposure() << mLightShaftsEffect->exposure;
		mLightShaftsEffect->Material->Weight() << mLightShaftsEffect->weight;
		mLightShaftsEffect->Material->Density() << mLightShaftsEffect->density;
		mLightShaftsEffect->Material->MaxSunDistanceDelta() << mLightShaftsEffect->maxSunDistanceDelta;
		mLightShaftsEffect->Material->Intensity() << mLightShaftsEffect->intensity;
	}

	void PostProcessingStack::ShowPostProcessingWindow()
	{
		if (!mShowDebug)
			return;

		ImGui::Begin("Post Processing Stack Config");

		if (mLightShaftsLoaded)
		{
			if (ImGui::CollapsingHeader("Light Shafts"))
			{
				ImGui::Checkbox("Light Shafts - On", &mLightShaftsEffect->isActive);
				ImGui::Checkbox("Enabled when sun is on screen", &mLightShaftsEffect->isForceEnable);
				ImGui::Text("Sun Screen Position: (%.1f,%.1f)", mSunNDCPos.x, mSunNDCPos.y);
				//ImGui::SliderInt("Ray count", &mSSREffect->rayCount, 0, 100);
				ImGui::SliderFloat("Decay", &mLightShaftsEffect->decay, 0.5f, 1.0f);
				ImGui::SliderFloat("Weight", &mLightShaftsEffect->weight, 0.0f, 25.0f);
				ImGui::SliderFloat("Exposure", &mLightShaftsEffect->exposure, 0.0f, 0.01f);
				ImGui::SliderFloat("Density", &mLightShaftsEffect->density, 0.0f, 1.0f);
				//ImGui::SliderFloat("Max Sun Distance Delta", &mLightShaftsEffect->maxSunDistanceDelta, 0.01f, 100.0f);
				ImGui::SliderFloat("Intensity", &mLightShaftsEffect->intensity, 0.0f, 1.0f);
			}
		}
		if (mSSRLoaded)
		{
			if (ImGui::CollapsingHeader("Screen Space Reflections"))
			{
				ImGui::Checkbox("SSR - On", &mSSREffect->isActive);
				ImGui::SliderInt("Ray count", &mSSREffect->rayCount, 0, 100);
				ImGui::SliderFloat("Step Size", &mSSREffect->stepSize, 0.0f, 10.0f);
				ImGui::SliderFloat("Max Thickness", &mSSREffect->maxThickness, 0.0f, 0.01f);
			}
		}

		if (ImGui::CollapsingHeader("Linear Fog"))
		{
			ImGui::Checkbox("Fog - On", &mUseLinearFog);
			ImGui::ColorEdit3("Color", mLinearFogColor);
			ImGui::SliderFloat("Density", &mLinearFogDensity, 1.0f, 10000.0f);
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

	ID3D11ShaderResourceView* PostProcessingStack::GetExtraColorSRV()
	{
		return mExtraRenderTarget->OutputColorTexture();
	}
	
	void PostProcessingStack::Begin(ER_GPUTexture* aInitialRT, DepthTarget* aDepthTarget)
	{
		assert(aInitialRT && aDepthTarget);
		mFirstTargetBeforePostProcessingPasses = aInitialRT;
		mDepthTarget = aDepthTarget;
		game.Direct3DDeviceContext()->OMSetRenderTargets(1, aInitialRT->GetRTVs(), aDepthTarget->getDSV());
	}

	void PostProcessingStack::End(ER_GPUTexture* aResolveRT)
	{
		game.ResetRenderTargets();

		//final resolve to main swapchain
		{
			ER_QuadRenderer* quad = (ER_QuadRenderer*)game.Services().GetService(ER_QuadRenderer::TypeIdClass());
			assert(quad);
			assert(mFinalTargetBeforeResolve);

			auto context = game.Direct3DDeviceContext();

			ID3D11ShaderResourceView* SR[1] = { aResolveRT ? aResolveRT->GetSRV() : mFinalTargetBeforeResolve->GetSRV() };
			context->PSSetShaderResources(0, 1, SR);
			ID3D11SamplerState* SS[1] = { SamplerStates::TrilinearWrap };
			context->PSSetSamplers(0, 1, SS);
			context->PSSetShader(mFinalResolvePS, NULL, NULL);
			quad->Draw(context);
		}
	}

	void PostProcessingStack::BeginRenderingToExtraRT(bool clear)
	{
		mExtraRenderTarget->Begin();

		if (clear) {
			game.Direct3DDeviceContext()->ClearRenderTargetView(mExtraRenderTarget->RenderTargetView(), ClearBackgroundColor);
			game.Direct3DDeviceContext()->ClearDepthStencilView(mExtraRenderTarget->DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0);
		}
	}

	void PostProcessingStack::EndRenderingToExtraRT()
	{
		mExtraRenderTarget->End();
	}

	void PostProcessingStack::PrepareDrawingLinearFog(ER_GPUTexture* aInputTexture)
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

	void PostProcessingStack::PrepareDrawingColorGrading(ER_GPUTexture* aInputTexture)
	{
		assert(aInputTexture);
		auto context = game.Direct3DDeviceContext();

		context->PSSetShader(mColorGradingPS, NULL, NULL);

		ID3D11ShaderResourceView* SRs[2] = { mLUTs[mColorGradingCurrentLUTIndex], aInputTexture->GetSRV() };
		context->PSSetShaderResources(0, 2, SRs);
	}

	void PostProcessingStack::PrepareDrawingVignette(ER_GPUTexture* aInputTexture)
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

	void PostProcessingStack::PrepareDrawingFXAA(ER_GPUTexture* aInputTexture)
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

	void PostProcessingStack::DrawEffects(const GameTime& gameTime, ER_QuadRenderer* quad)
	{
		assert(quad);
		ID3D11DeviceContext* context = game.Direct3DDeviceContext();

		mFinalTargetBeforeResolve = mFirstTargetBeforePostProcessingPasses;

		/*
		// LIGHT SHAFTS
		mLightShaftsRenderTarget->Begin();
		//mLightShaftsEffect->OutputTexture = mMainRenderTarget->GetSRV();
		//mLightShaftsEffect->DepthTexture = mMainDepthTarget->getSRV();
		game.Direct3DDeviceContext()->ClearRenderTargetView(mLightShaftsRenderTarget->RenderTargetView(), ClearBackgroundColor);
		game.Direct3DDeviceContext()->ClearDepthStencilView(mLightShaftsRenderTarget->DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0);
		mLightShaftsEffect->Quad->Draw(gameTime);
		mLightShaftsRenderTarget->End();

		//if (mIsScreenSpaceLightshaftsEnabled)
		//{
			//PrepareScreenSpaceLightshafts();
			//mQuadRenderer->Draw();
		//}

		// SSR
		mSSRRenderTarget->Begin();
		mSSREffect->InputColor = mLightShaftsRenderTarget->OutputColorTexture();
		game.Direct3DDeviceContext()->ClearRenderTargetView(mSSRRenderTarget->RenderTargetView(), ClearBackgroundColor);
		game.Direct3DDeviceContext()->ClearDepthStencilView(mSSRRenderTarget->DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0);
		mSSREffect->Quad->Draw(gameTime);
		mSSRRenderTarget->End();

		// FOG
		mFogRenderTarget->Begin();
		mFogEffect->OutputTexture = mSSRRenderTarget->OutputColorTexture();
		mFogEffect->DepthTexture = nullptr;//TODO mMainDepthTarget->getSRV();
		game.Direct3DDeviceContext()->ClearRenderTargetView(mFogRenderTarget->RenderTargetView(), ClearBackgroundColor);
		game.Direct3DDeviceContext()->ClearDepthStencilView(mFogRenderTarget->DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0);
		mFogEffect->Quad->Draw(gameTime);
		mFogRenderTarget->End();

		//if (mIsLinearFogEnabled)
		//{
			//PrepareLinearFog();
			//mQuadRenderer->Draw();
		//}

		// TONEMAP
		mTonemapRenderTarget->Begin();
		context->ClearRenderTargetView(mTonemapRenderTarget->RenderTargetView(), ClearBackgroundColor);
		context->ClearDepthStencilView(mTonemapRenderTarget->DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0);

		if (mTonemapEffect->isActive)
		{
			float pBuffer[] = { 
				mTonemapEffect->luminanceWeights[0],
				mTonemapEffect->luminanceWeights[1],
				mTonemapEffect->luminanceWeights[2],
				1.0f,
				mTonemapEffect->middlegrey,
				mTonemapEffect->bloomthreshold,
				mTonemapEffect->bloommultiplier, 
				0.0f 
			};
			context->UpdateSubresource(mTonemapEffect->BloomConstants, 0, NULL, pBuffer, sizeof(float) * 8, 0);
			context->PSSetConstantBuffers(1, 1, &mTonemapEffect->BloomConstants);
			D3D11_VIEWPORT viewport;
			UINT numViewPorts = 1;
			context->RSGetViewports(&numViewPorts, &viewport);

			unsigned int width = 512;
			unsigned int height = 512;

			int gameWidth = game.ScreenWidth();
			int gameHeight = game.ScreenHeight();

			D3D11_VIEWPORT quadViewPort;
			quadViewPort.Height = (float)height;
			quadViewPort.Width = (float)width;
			quadViewPort.MaxDepth = 1.0f;
			quadViewPort.MinDepth = 0.0f;
			quadViewPort.TopLeftX = 0;
			quadViewPort.TopLeftY = 0;

			context->PSSetSamplers(0, 1, &mTonemapEffect->LinearSampler);
			context->RSSetViewports(1, &quadViewPort);

			UpdateTonemapConstantBuffer(context, mTonemapEffect->ConstBuffer, 0, 0, width, height);
			ComputeLuminance(context, mFogRenderTarget->OutputColorTexture(), *mTonemapEffect->LuminanceResource->GetRTVs());

			quadViewPort.Height = (float)(gameHeight / 2);
			quadViewPort.Width = (float)(gameWidth / 2);
			context->RSSetViewports(1, &quadViewPort);

			ComputeBrightPass(context, mFogRenderTarget->OutputColorTexture(), *mTonemapEffect->BrightResource->GetRTVs());
			ID3D11RenderTargetView* pNULLRT[] = { NULL };
			context->OMSetRenderTargets(1, pNULLRT, NULL);
			context->GenerateMips(mTonemapEffect->BrightResource->GetSRV());

			int mipLevel0 = 3;
			int mipLevel1 = 4;
			width = gameWidth >> (mipLevel0 + 1);
			height = gameHeight >> (mipLevel0 + 1);
			quadViewPort.Height = (float)height;
			quadViewPort.Width = (float)width;
			context->RSSetViewports(1, &quadViewPort);
			UpdateTonemapConstantBuffer(context, mTonemapEffect->ConstBuffer, mipLevel0, mipLevel1, width, height);
			ComputeHorizontalBlur(context, mTonemapEffect->BrightResource->GetSRV(), mTonemapEffect->BlurHorizontalResource->GetRTVs()[mipLevel0]);
			ComputeVerticalBlur(context, mTonemapEffect->BlurHorizontalResource->GetSRV(), mTonemapEffect->BlurVerticalResource->GetRTVs()[mipLevel0]);

			mipLevel0--;
			mipLevel1--;
			width = gameWidth >> (mipLevel0 + 1);;
			height = gameHeight >> (mipLevel0 + 1);
			quadViewPort.Height = (float)height;
			quadViewPort.Width = (float)width;
			context->RSSetViewports(1, &quadViewPort);

			UpdateTonemapConstantBuffer(context, mTonemapEffect->ConstBuffer, mipLevel0, mipLevel1, width, height);
			context->OMSetRenderTargets(1, &mTonemapEffect->BlurSummedResource->GetRTVs()[mipLevel0], NULL);
			ID3D11ShaderResourceView* pSR[] = { mTonemapEffect->BrightResource->GetSRV(),  mTonemapEffect->BlurVerticalResource->GetSRV() };
			context->PSSetShaderResources(0, 2, pSR);
			context->PSSetShader(mTonemapEffect->AddPS, NULL, NULL);
			DrawFullscreenQuad(context);
			ComputeHorizontalBlur(context, mTonemapEffect->BlurSummedResource->GetSRV(), mTonemapEffect->BlurHorizontalResource->GetRTVs()[mipLevel0]);
			ComputeVerticalBlur(context, mTonemapEffect->BlurHorizontalResource->GetSRV(), mTonemapEffect->BlurVerticalResource->GetRTVs()[mipLevel0]);

			mipLevel0--;
			mipLevel1--;
			width = gameWidth >> (mipLevel0 + 1);
			height = gameHeight >> (mipLevel0 + 1);
			quadViewPort.Height = (float)height;
			quadViewPort.Width = (float)width;
			context->RSSetViewports(1, &quadViewPort);

			UpdateTonemapConstantBuffer(context, mTonemapEffect->ConstBuffer, mipLevel0, mipLevel1, width, height);
			context->OMSetRenderTargets(1, &mTonemapEffect->BlurSummedResource->GetRTVs()[mipLevel0], NULL);
			ID3D11ShaderResourceView* pSR2[] = { mTonemapEffect->BrightResource->GetSRV(), mTonemapEffect->BlurVerticalResource->GetSRV() };
			context->PSSetShaderResources(0, 2, pSR2);
			context->PSSetShader(mTonemapEffect->AddPS, NULL, NULL);
			DrawFullscreenQuad(context);
			ComputeHorizontalBlur(context, mTonemapEffect->BlurSummedResource->GetSRV(), mTonemapEffect->BlurHorizontalResource->GetRTVs()[mipLevel0]);
			ComputeVerticalBlur(context, mTonemapEffect->BlurHorizontalResource->GetSRV(), mTonemapEffect->BlurVerticalResource->GetRTVs()[mipLevel0]);

			mipLevel0--;
			mipLevel1--;
			width = gameWidth >> (mipLevel0 + 1);
			height = gameHeight >> (mipLevel0 + 1);
			quadViewPort.Height = (float)height;
			quadViewPort.Width = (float)width;
			context->RSSetViewports(1, &quadViewPort);

			UpdateTonemapConstantBuffer(context, mTonemapEffect->ConstBuffer, mipLevel0, mipLevel1, width, height);
			context->OMSetRenderTargets(1, &mTonemapEffect->BlurSummedResource->GetRTVs()[mipLevel0], NULL);
			ID3D11ShaderResourceView* pSR3[] = { mTonemapEffect->BrightResource->GetSRV(), mTonemapEffect->BlurVerticalResource->GetSRV() };
			context->PSSetShaderResources(0, 2, pSR3);
			context->PSSetShader(mTonemapEffect->AddPS, NULL, NULL);
			DrawFullscreenQuad(context);
			ComputeHorizontalBlur(context, mTonemapEffect->BlurSummedResource->GetSRV(), mTonemapEffect->BlurHorizontalResource->GetRTVs()[mipLevel0]);
			ComputeVerticalBlur(context, mTonemapEffect->BlurHorizontalResource->GetSRV(), mTonemapEffect->BlurVerticalResource->GetRTVs()[mipLevel0]);

			context->RSSetViewports(1, &viewport);
			ComputeToneMapWithBloom(context, mFogRenderTarget->OutputColorTexture(), mTonemapEffect->AvgLuminanceResource->GetSRV(), mTonemapEffect->BlurVerticalResource->GetSRV(), mTonemapRenderTarget->RenderTargetView());
		}
		else
		{
			PerformEmptyPass(context, mFogRenderTarget->OutputColorTexture(), mTonemapRenderTarget->RenderTargetView());
		}
		mTonemapRenderTarget->End();

		// MOTION BLUR
		mMotionBlurRenderTarget->Begin();
		game.Direct3DDeviceContext()->ClearRenderTargetView(mMotionBlurRenderTarget->RenderTargetView(), ClearBackgroundColor);
		game.Direct3DDeviceContext()->ClearDepthStencilView(mMotionBlurRenderTarget->DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0);
		mMotionBlurEffect->OutputTexture = mTonemapRenderTarget->OutputColorTexture();
		mMotionBlurEffect->DepthMap = nullptr;// TODO mMainDepthTarget->getSRV();
		mMotionBlurEffect->Quad->Draw(gameTime);
		mMotionBlurRenderTarget->End();

		// COLOR GRADING
		mColorGradingRenderTarget->Begin();
		mColorGradingEffect->OutputTexture = mMotionBlurRenderTarget->OutputColorTexture();
		game.Direct3DDeviceContext()->ClearRenderTargetView(mColorGradingRenderTarget->RenderTargetView(), ClearBackgroundColor);
		game.Direct3DDeviceContext()->ClearDepthStencilView(mColorGradingRenderTarget->DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0);
		mColorGradingEffect->Quad->Draw(gameTime);
		mColorGradingRenderTarget->End();
		
		// VIGNETTE
		mVignetteRenderTarget->Begin();
		game.Direct3DDeviceContext()->ClearRenderTargetView(mVignetteRenderTarget->RenderTargetView(), ClearBackgroundColor);
		game.Direct3DDeviceContext()->ClearDepthStencilView(mVignetteRenderTarget->DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0);
		mVignetteEffect->OutputTexture = mColorGradingRenderTarget->OutputColorTexture();
		mVignetteEffect->Quad->Draw(gameTime);
		mVignetteRenderTarget->End();
		*/

		// Linear fog
		if (mUseLinearFog)
		{
			game.SetCustomRenderTarget(mLinearFogRT, nullptr);
			PrepareDrawingLinearFog(mFinalTargetBeforeResolve);
			quad->Draw(context);
			game.UnsetCustomRenderTarget();

			//[WARNING] Set from last post processing effect
			mFinalTargetBeforeResolve = mLinearFogRT;
		}

		// Color grading
		if (mUseColorGrading)
		{
			game.SetCustomRenderTarget(mColorGradingRT, nullptr);
			PrepareDrawingColorGrading(mFinalTargetBeforeResolve);
			quad->Draw(context);
			game.UnsetCustomRenderTarget();

			//[WARNING] Set from last post processing effect
			mFinalTargetBeforeResolve = mColorGradingRT;
		}
		// Vignette
		if (mUseVignette)
		{
			game.SetCustomRenderTarget(mVignetteRT, nullptr);
			PrepareDrawingVignette(mFinalTargetBeforeResolve);
			quad->Draw(context);
			game.UnsetCustomRenderTarget();

			//[WARNING] Set from last post processing effect
			mFinalTargetBeforeResolve = mVignetteRT;
		}
		// FXAA
		if (mUseFXAA)
		{
			game.SetCustomRenderTarget(mFXAART, nullptr);
			PrepareDrawingFXAA(mFinalTargetBeforeResolve);
			quad->Draw(context);
			game.UnsetCustomRenderTarget();

			//[WARNING] Set from last post processing effect 
			mFinalTargetBeforeResolve = mFXAART;
		}
	}
}