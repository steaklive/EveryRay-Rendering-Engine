#pragma once
#include "Common.h"
#include "Game.h"
#include "Camera.h"

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

	namespace EffectElements
	{
		struct VignetteEffect
		{
			VignetteEffect() :
				Material(nullptr), Quad(nullptr), RenderTarget(nullptr)
			{}

			~VignetteEffect()
			{
				DeleteObject(Material);
				DeleteObject(Quad);
				DeleteObject(RenderTarget);
			}

			VignetteMaterial* Material;
			FullScreenQuad* Quad;
			FullScreenRenderTarget* RenderTarget;

			bool isActive = false;

			float radius = 0.75f;
			float softness = 0.5f;
		};

		struct ColorGradingEffect
		{
			ColorGradingEffect() :
				Material(nullptr), Quad(nullptr), RenderTarget(nullptr),
				LUT1(nullptr),LUT2(nullptr),LUT3(nullptr)
			{}

			~ColorGradingEffect()
			{
				DeleteObject(Material);
				DeleteObject(Quad);
				ReleaseObject(LUT1);
				ReleaseObject(LUT2);
				ReleaseObject(LUT3);
				DeleteObject(RenderTarget);

			}

			ColorGradingMaterial* Material;
			FullScreenQuad* Quad;
			FullScreenRenderTarget* RenderTarget;

			ID3D11ShaderResourceView* LUT1;
			ID3D11ShaderResourceView* LUT2;
			ID3D11ShaderResourceView* LUT3;

			bool isActive = false;
			int currentLUT = 0;

		};

		struct MotionBlurEffect
		{
			MotionBlurEffect() :
				Material(nullptr), Quad(nullptr), RenderTarget(nullptr),
				DepthMap(nullptr)
			{}

			~MotionBlurEffect()
			{
				DeleteObject(Material);
				DeleteObject(Quad);
				//ReleaseObject(DepthMap);
				DeleteObject(RenderTarget);

			}

			MotionBlurMaterial* Material;
			FullScreenQuad* Quad;
			FullScreenRenderTarget* RenderTarget;

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

		void Initialize();
		void DrawVignette(const GameTime& gameTime);
		void DrawColorGrading(const GameTime& gameTime);
		void DrawMotionBlur(const GameTime& gameTime);

		void Update();

		void SetVignetteRT(FullScreenRenderTarget* rt);
		void SetColorGradingRT(FullScreenRenderTarget* rt);
		void SetMotionBlurRT(FullScreenRenderTarget* rt, ID3D11ShaderResourceView* depthMap);


		void ShowPostProcessingWindow();

		bool isWindowOpened = false;

	private:

		EffectElements::VignetteEffect* mVignetteEffect;
		EffectElements::ColorGradingEffect* mColorGradingEffect;
		EffectElements::MotionBlurEffect* mMotionBlurEffect;

		Game& game;
		Camera& camera;

	};
}