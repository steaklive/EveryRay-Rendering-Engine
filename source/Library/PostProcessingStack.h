#pragma once
#include "Common.h"
#include "Game.h"
#include "Camera.h"
#include "GameTime.h"

namespace Library
{
	class Effect;
	class FullScreenRenderTarget;
	class FullScreenQuad;
}


namespace Rendering
{
	class ColorFilterMaterial;
	class VignetteMaterial;
	class ColorGradingMaterial;
	class MotionBlurMaterial;
	class FXAAMaterial;

	namespace EffectElements
	{
		struct VignetteEffect
		{
			VignetteEffect() :
				Material(nullptr), Quad(nullptr), OutputTexture(nullptr)
			{}

			~VignetteEffect()
			{
				DeleteObject(Material);
				DeleteObject(Quad);
				ReleaseObject(OutputTexture);
			}

			VignetteMaterial* Material;
			FullScreenQuad* Quad;
			ID3D11ShaderResourceView* OutputTexture;

			bool isActive = false;

			float radius = 0.75f;
			float softness = 0.5f;
		};

		struct FXAAEffect
		{
			FXAAEffect() :
				Material(nullptr), Quad(nullptr), OutputTexture(nullptr)
			{}

			~FXAAEffect()
			{
				DeleteObject(Material);
				DeleteObject(Quad);
				ReleaseObject(OutputTexture);
			}

			FXAAMaterial* Material;
			FullScreenQuad* Quad;
			ID3D11ShaderResourceView* OutputTexture;

			bool isActive = false;
		};

		struct ColorGradingEffect
		{
			ColorGradingEffect() :
				Material(nullptr), Quad(nullptr), OutputTexture(nullptr),
				LUT1(nullptr),LUT2(nullptr),LUT3(nullptr)
			{}

			~ColorGradingEffect()
			{
				DeleteObject(Material);
				DeleteObject(Quad);
				ReleaseObject(LUT1);
				ReleaseObject(LUT2);
				ReleaseObject(LUT3);
				ReleaseObject(OutputTexture);

			}

			ColorGradingMaterial* Material;
			FullScreenQuad* Quad;
			ID3D11ShaderResourceView* OutputTexture;

			ID3D11ShaderResourceView* LUT1;
			ID3D11ShaderResourceView* LUT2;
			ID3D11ShaderResourceView* LUT3;

			bool isActive = false;
			int currentLUT = 0;

		};

		struct MotionBlurEffect
		{
			MotionBlurEffect() :
				Material(nullptr), Quad(nullptr), OutputTexture(nullptr),
				DepthMap(nullptr)
			{}

			~MotionBlurEffect()
			{
				DeleteObject(Material);
				DeleteObject(Quad);
				ReleaseObject(DepthMap);
				ReleaseObject(OutputTexture);

			}

			MotionBlurMaterial* Material;
			FullScreenQuad* Quad;
			ID3D11ShaderResourceView* OutputTexture;
			ID3D11ShaderResourceView* DepthMap;

			XMMATRIX WVP = XMMatrixIdentity();
			XMMATRIX preWVP = XMMatrixIdentity();
			XMMATRIX WIT = XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixIdentity()));

			bool isActive = false;

			float amount = 17.0f;
		};


	}

	class PostProcessingStack
	{
	public:

		PostProcessingStack() = delete;
		PostProcessingStack(Game& pGame, Camera& pCamera);
		~PostProcessingStack();

		void UpdateVignetteMaterial();
		void UpdateColorGradingMaterial();
		void UpdateMotionBlurMaterial();
		void UpdateFXAAMaterial();

		void Initialize(bool pMotionBlur, bool pColorGrading, bool pVignette, bool pFXAA);
		void Begin();
		void End(const GameTime& gameTime);

		void Update();

		void ShowPostProcessingWindow();

		bool isWindowOpened = false;

	private:
		EffectElements::VignetteEffect* mVignetteEffect;
		EffectElements::ColorGradingEffect* mColorGradingEffect;
		EffectElements::MotionBlurEffect* mMotionBlurEffect;
		EffectElements::FXAAEffect* mFXAAEffect;

		Game& game;
		Camera& camera;

		bool mVignetteLoaded = false;
		bool mColorGradingLoaded = false;
		bool mMotionBlurLoaded = false;
		bool mFXAALoaded = false;

		FullScreenRenderTarget* mMainRenderTarget;
		FullScreenRenderTarget* mVignetteRenderTarget;
		FullScreenRenderTarget* mColorGradingRenderTarget;
		FullScreenRenderTarget* mMotionBlurRenderTarget;
		FullScreenRenderTarget* mFXAARenderTarget;
	};
}