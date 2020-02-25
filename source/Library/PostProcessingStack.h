#pragma once
#include "Common.h"
#include "Game.h"
#include "Camera.h"
#include "GameTime.h"
#include "CustomRenderTarget.h"

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
		struct TonemapEffect
		{
			TonemapEffect():
				CalcLumPS(nullptr),
				AvgLumPS(nullptr),
				BrightPS(nullptr),
				AddPS(nullptr),
				BlurHPS(nullptr),
				BlurVPS(nullptr),
				ToneMapWithBloomPS(nullptr),
				EmptyPassPS(nullptr)
			{
			}

			~TonemapEffect()
			{
				DeleteObject(LuminanceResource);
				DeleteObject(AvgLuminanceResource);
				DeleteObject(BrightResource);
				DeleteObject(BlurHorizontalResource);
				DeleteObject(BlurVerticalResource);
				DeleteObject(BlurSummedResource);

				ReleaseObject(BloomConstants);
				ReleaseObject(ConstBuffer);
				ReleaseObject(LinearSampler);

				ReleaseObject(CalcLumPS);
				ReleaseObject(AvgLumPS);
				ReleaseObject(BrightPS);
				ReleaseObject(AddPS);
				ReleaseObject(BlurHPS);
				ReleaseObject(BlurVPS);
				ReleaseObject(ToneMapWithBloomPS);
				ReleaseObject(EmptyPassPS);
			}

			CustomRenderTarget* LuminanceResource;
			CustomRenderTarget* AvgLuminanceResource;
			CustomRenderTarget* BrightResource;
			CustomRenderTarget* BlurHorizontalResource;
			CustomRenderTarget* BlurVerticalResource;
			CustomRenderTarget* BlurSummedResource;

			ID3D11PixelShader* CalcLumPS;
			ID3D11PixelShader* AvgLumPS;
			ID3D11PixelShader* BrightPS;
			ID3D11PixelShader* AddPS;
			ID3D11PixelShader* BlurHPS;
			ID3D11PixelShader* BlurVPS;
			ID3D11PixelShader* ToneMapWithBloomPS;
			ID3D11PixelShader* EmptyPassPS;

			ID3D11Buffer* BloomConstants;
			ID3D11Buffer* ConstBuffer;	
			ID3D11SamplerState* LinearSampler;

			float middlegrey = 0.065f;
			float bloomthreshold = 0.23f;
			float bloommultiplier = 0.85f;
			bool isActive = true;
		};
		
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

			bool isActive = true;
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

		void Initialize(bool pTonemap, bool pMotionBlur, bool pColorGrading, bool pVignette, bool pFXAA);
		void Begin();
		void End(const GameTime& gameTime);

		void Update();
		void DrawFullscreenQuad(ID3D11DeviceContext* pContext);
		void ShowPostProcessingWindow();

		bool isWindowOpened = false;

	private:
		void UpdateTonemapConstantBuffer(ID3D11DeviceContext * pD3DImmediateContext, ID3D11Buffer* buffer, int mipLevel0, int mipLevel1, unsigned int width, unsigned int height);
		void ComputeLuminance(ID3D11DeviceContext* pContext, ID3D11ShaderResourceView* pInput, ID3D11RenderTargetView* pOutput);
		void ComputeBrightPass(ID3D11DeviceContext* pContext, ID3D11ShaderResourceView* pInput, ID3D11RenderTargetView* pOutput);
		void ComputeHorizontalBlur(ID3D11DeviceContext* pContext, ID3D11ShaderResourceView* pInput, ID3D11RenderTargetView* pOutput);
		void ComputeVerticalBlur(ID3D11DeviceContext* pContext, ID3D11ShaderResourceView* pInput, ID3D11RenderTargetView* pOutput);
		void ComputeToneMapWithBloom(ID3D11DeviceContext* pContext, ID3D11ShaderResourceView* pInput, ID3D11ShaderResourceView* pAVG, ID3D11ShaderResourceView* pBloom, ID3D11RenderTargetView* pOutput);
		void PerformEmptyPass(ID3D11DeviceContext * pContext, ID3D11ShaderResourceView * pInput, ID3D11RenderTargetView * pOutput);

		Game& game;
		Camera& camera;

		EffectElements::VignetteEffect* mVignetteEffect;
		EffectElements::ColorGradingEffect* mColorGradingEffect;
		EffectElements::MotionBlurEffect* mMotionBlurEffect;
		EffectElements::FXAAEffect* mFXAAEffect;
		EffectElements::TonemapEffect* mTonemapEffect;
		
		FullScreenRenderTarget* mMainRenderTarget;
		FullScreenRenderTarget* mVignetteRenderTarget;
		FullScreenRenderTarget* mColorGradingRenderTarget;
		FullScreenRenderTarget* mMotionBlurRenderTarget;
		FullScreenRenderTarget* mFXAARenderTarget;
		FullScreenRenderTarget* mTonemapRenderTarget;

		ID3D11Buffer* mQuadVB;
		ID3D11VertexShader* mFullScreenQuadVS;
		ID3D11InputLayout* mFullScreenQuadLayout;

		bool mVignetteLoaded = false;
		bool mColorGradingLoaded = false;
		bool mMotionBlurLoaded = false;
		bool mFXAALoaded = false;

	};
}