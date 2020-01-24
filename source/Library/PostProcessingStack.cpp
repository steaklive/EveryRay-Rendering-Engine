#include "PostProcessingStack.h"
#include "VignetteMaterial.h"
#include "MotionBlurMaterial.h"
#include "ColorGradingMaterial.h"
#include "FullScreenQuad.h"
#include "FullScreenRenderTarget.h"
#include "GameException.h"
#include <WICTextureLoader.h>
#include "Utility.h"

#include "MatrixHelper.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

namespace Rendering {

	PostProcessingStack::PostProcessingStack(Game& pGame, Camera& pCamera)
		:
		mVignetteEffect(nullptr),
		mColorGradingEffect(nullptr),
		mMotionBlurEffect(nullptr),
		game(pGame), camera(pCamera)
	{
	}

	PostProcessingStack::~PostProcessingStack()
	{
		DeleteObject(mVignetteEffect);
		DeleteObject(mColorGradingEffect);
		DeleteObject(mMotionBlurEffect);
	}

	void PostProcessingStack::Initialize(bool vignetteOn = false, bool colorGradeOn = false, bool blurOn = false)
	{
		// Effect shaders
		if (vignetteOn)
		{
			Effect* vignetteEffectFX = new Effect(game);
			vignetteEffectFX->CompileFromFile(Utility::GetFilePath(L"content\\effects\\Vignette.fx"));
			mVignetteEffect = new EffectElements::VignetteEffect();
			mVignetteEffect->Material = new VignetteMaterial();
			mVignetteEffect->Material->Initialize(vignetteEffectFX);
			mVignetteEffect->Quad = new FullScreenQuad(game, *mVignetteEffect->Material);
			mVignetteEffect->Quad->Initialize();
			mVignetteEffect->Quad->SetActiveTechnique("vignette_filter", "p0");
			mVignetteEffect->Quad->SetCustomUpdateMaterial(std::bind(&PostProcessingStack::UpdateVignetteMaterial, this));
			mVignetteLoaded = true;
		}

		if (colorGradeOn)
		{
			Effect* colorgradingEffectFX = new Effect(game);
			colorgradingEffectFX->CompileFromFile(Utility::GetFilePath(L"content\\effects\\ColorGrading.fx"));
			mColorGradingEffect = new EffectElements::ColorGradingEffect();
			mColorGradingEffect->Material = new ColorGradingMaterial();
			mColorGradingEffect->Material->Initialize(colorgradingEffectFX);
			mColorGradingEffect->Quad = new FullScreenQuad(game, *mColorGradingEffect->Material);
			mColorGradingEffect->Quad->Initialize();
			mColorGradingEffect->Quad->SetActiveTechnique("color_grading_filter", "p0");
			mColorGradingEffect->Quad->SetCustomUpdateMaterial(std::bind(&PostProcessingStack::UpdateColorGradingMaterial, this));

			std::wstring LUTtexture1 = Utility::GetFilePath(L"content\\effects\\LUT_1.png");
			if (FAILED(DirectX::CreateWICTextureFromFile(game.Direct3DDevice(), game.Direct3DDeviceContext(), LUTtexture1.c_str(), nullptr, &mColorGradingEffect->LUT1)))
			{
				throw GameException("Failed to load LUT1 texture.");
			}

			std::wstring LUTtexture2 = Utility::GetFilePath(L"content\\effects\\LUT_2.png");
			if (FAILED(DirectX::CreateWICTextureFromFile(game.Direct3DDevice(), game.Direct3DDeviceContext(), LUTtexture2.c_str(), nullptr, &mColorGradingEffect->LUT2)))
			{
				throw GameException("Failed to load LUT2 texture.");
			}

			std::wstring LUTtexture3 = Utility::GetFilePath(L"content\\effects\\LUT_3.png");
			if (FAILED(DirectX::CreateWICTextureFromFile(game.Direct3DDevice(), game.Direct3DDeviceContext(), LUTtexture3.c_str(), nullptr, &mColorGradingEffect->LUT3)))
			{
				throw GameException("Failed to load LUT3 texture.");
			}

			mColorGradingLoaded = true;
		}

		if (blurOn)
		{
			Effect* motionblurEffectFX = new Effect(game);
			motionblurEffectFX->CompileFromFile(Utility::GetFilePath(L"content\\effects\\MotionBlur.fx"));
			mMotionBlurEffect = new EffectElements::MotionBlurEffect();
			mMotionBlurEffect->Material = new MotionBlurMaterial();
			mMotionBlurEffect->Material->Initialize(motionblurEffectFX);
			mMotionBlurEffect->Quad = new FullScreenQuad(game, *mMotionBlurEffect->Material);
			mMotionBlurEffect->Quad->Initialize();
			mMotionBlurEffect->Quad->SetActiveTechnique("blur_filter", "p0");
			mMotionBlurEffect->Quad->SetCustomUpdateMaterial(std::bind(&PostProcessingStack::UpdateMotionBlurMaterial, this));
		
			mMotionBlurLoaded = true;
		}

	}
	void PostProcessingStack::Update()
	{
		if (mVignetteLoaded)
		{
			if (mVignetteEffect->isActive) mVignetteEffect->Quad->SetActiveTechnique("vignette_filter", "p0");
			else mVignetteEffect->Quad->SetActiveTechnique("no_filter", "p0");
		}

		if (mColorGradingLoaded) 
		{
			if (mColorGradingEffect->isActive) mColorGradingEffect->Quad->SetActiveTechnique("color_grading_filter", "p0");
			else mColorGradingEffect->Quad->SetActiveTechnique("no_filter", "p0");
		}

		if (mMotionBlurLoaded)
		{
			if (mMotionBlurEffect->isActive) mMotionBlurEffect->Quad->SetActiveTechnique("blur_filter", "p0");
			else mMotionBlurEffect->Quad->SetActiveTechnique("no_filter", "p0");
			mMotionBlurEffect->preWVP = mMotionBlurEffect->WVP;
			mMotionBlurEffect->WVP = XMMatrixIdentity() * camera.ViewMatrix() * camera.ProjectionMatrix();
		}
	}

	void PostProcessingStack::SetVignetteRT(FullScreenRenderTarget* rt)
	{
		mVignetteEffect->RenderTarget = rt;
	}
	void PostProcessingStack::DrawVignette(const GameTime& gameTime)
	{
		mVignetteEffect->Quad->Draw(gameTime);

	}
	void PostProcessingStack::UpdateVignetteMaterial()
	{

		mVignetteEffect->Material->ColorTexture() << mVignetteEffect->RenderTarget->OutputColorTexture();
		mVignetteEffect->Material->radius() << mVignetteEffect->radius;
		mVignetteEffect->Material->softness() << mVignetteEffect->softness;
	}

	void PostProcessingStack::SetColorGradingRT(FullScreenRenderTarget* rt)
	{
		mColorGradingEffect->RenderTarget = rt;
	}
	void PostProcessingStack::DrawColorGrading(const GameTime& gameTime)
	{
		mColorGradingEffect->Quad->Draw(gameTime);
	}
	void PostProcessingStack::UpdateColorGradingMaterial()
	{

		mColorGradingEffect->Material->ColorTexture() << mColorGradingEffect->RenderTarget->OutputColorTexture();
		switch (mColorGradingEffect->currentLUT)
		{
		case 0:
			mColorGradingEffect->Material->lut() << mColorGradingEffect->LUT1;
			break;
		case 1:
			mColorGradingEffect->Material->lut() << mColorGradingEffect->LUT2;
			break;
		case 2:
			mColorGradingEffect->Material->lut() << mColorGradingEffect->LUT3;
			break;
		default:
			mColorGradingEffect->Material->lut() << mColorGradingEffect->LUT1;
			break;
		}

	}

	void PostProcessingStack::SetMotionBlurRT(FullScreenRenderTarget* rt, ID3D11ShaderResourceView* depthMap)
	{
		mMotionBlurEffect->RenderTarget = rt;
		mMotionBlurEffect->DepthMap = depthMap;
	}
	void PostProcessingStack::DrawMotionBlur(const GameTime& gameTime)
	{
		mMotionBlurEffect->Quad->Draw(gameTime);

	}
	void PostProcessingStack::UpdateMotionBlurMaterial()
	{
		mMotionBlurEffect->Material->ColorTexture() << mMotionBlurEffect->RenderTarget->OutputColorTexture();
		mMotionBlurEffect->Material->DepthTexture() << mMotionBlurEffect->DepthMap;

		mMotionBlurEffect->Material->WorldViewProjection()		 << mMotionBlurEffect->WVP;
		mMotionBlurEffect->Material->InvWorldViewProjection()	 << XMMatrixInverse(nullptr, mMotionBlurEffect->WVP);
		mMotionBlurEffect->Material->preWorldViewProjection()	 << mMotionBlurEffect->preWVP;
		mMotionBlurEffect->Material->preInvWorldViewProjection() << XMMatrixInverse(nullptr, mMotionBlurEffect->preWVP);
		mMotionBlurEffect->Material->WorldInverseTranspose()	 << mMotionBlurEffect->WIT;
		mMotionBlurEffect->Material->amount() << mMotionBlurEffect->amount;
	}
	
	void PostProcessingStack::ShowPostProcessingWindow()
	{
		ImGui::Begin("Post Processing Stack Config");

		if (mMotionBlurLoaded)
		{
			if (ImGui::CollapsingHeader("Motion Blur"))
			{
				ImGui::Checkbox("Motion Blur - On", &mMotionBlurEffect->isActive);
				ImGui::SliderFloat("Amount", &mMotionBlurEffect->amount, 1.000f, 50.0f);
			}
		}
		
		if (mColorGradingLoaded) 
		{
			if (ImGui::CollapsingHeader("Color Grading"))
			{
				ImGui::Checkbox("Color Grading - On", &mColorGradingEffect->isActive);

				ImGui::TextWrapped("Current LUT");
				ImGui::RadioButton("LUT 1", &mColorGradingEffect->currentLUT, 0);
				ImGui::RadioButton("LUT 2", &mColorGradingEffect->currentLUT, 1);
				ImGui::RadioButton("LUT 3", &mColorGradingEffect->currentLUT, 2);
			}
		}

		if (mVignetteLoaded)
		{
			if (ImGui::CollapsingHeader("Vignette"))
			{
				ImGui::Checkbox("Vignette - On", &mVignetteEffect->isActive);
				ImGui::SliderFloat("Radius", &mVignetteEffect->radius, 0.000f, 1.0f);
				ImGui::SliderFloat("Softness", &mVignetteEffect->softness, 0.000f, 1.0f);
			}
		}

		ImGui::End();
	}

}








