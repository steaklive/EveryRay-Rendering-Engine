#pragma once
#include "Common.h"
#include "Game.h"
#include "Camera.h"
#include "GameTime.h"
#include "CustomRenderTarget.h"
#include "DepthTarget.h"

namespace Library
{
	class Effect;
	class FullScreenRenderTarget;
	class FullScreenQuad;
	class DirectionalLight;
}

namespace Rendering
{
	class ColorFilterMaterial;
	class VignetteMaterial;
	class ColorGradingMaterial;
	class MotionBlurMaterial;
	class FXAAMaterial;
	class FogMaterial;
	class ScreenSpaceReflectionsMaterial;
	class LightShaftsMaterial;
	class CompositeLightingMaterial;

	namespace EffectElements
	{
		struct CompositeLightingEffect
		{
			CompositeLightingEffect() :
				Material(nullptr), Quad(nullptr)
			{}

			~CompositeLightingEffect()
			{
				DeleteObject(Material);
				DeleteObject(Quad);
			}

			CompositeLightingMaterial* Material;
			FullScreenQuad* Quad;
			//ID3D11ShaderResourceView* InputDirectLightingTexture;
			//ID3D11ShaderResourceView* InputIndirectLightingTexture;

			bool isActive = false;
			bool debugGI = false;
		};

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

			CustomRenderTarget* LuminanceResource = nullptr;
			CustomRenderTarget* AvgLuminanceResource = nullptr;
			CustomRenderTarget* BrightResource = nullptr;
			CustomRenderTarget* BlurHorizontalResource = nullptr;
			CustomRenderTarget* BlurVerticalResource = nullptr;
			CustomRenderTarget* BlurSummedResource = nullptr;

			ID3D11PixelShader* CalcLumPS = nullptr;
			ID3D11PixelShader* AvgLumPS = nullptr;
			ID3D11PixelShader* BrightPS = nullptr;
			ID3D11PixelShader* AddPS = nullptr;
			ID3D11PixelShader* BlurHPS = nullptr;
			ID3D11PixelShader* BlurVPS = nullptr;
			ID3D11PixelShader* ToneMapWithBloomPS = nullptr;
			ID3D11PixelShader* EmptyPassPS = nullptr;

			ID3D11Buffer* BloomConstants = nullptr;
			ID3D11Buffer* ConstBuffer = nullptr;
			ID3D11SamplerState* LinearSampler = nullptr;

			float middlegrey = 0.053f;
			float bloomthreshold = 0.342f;
			float bloommultiplier = 0.837f;
			float luminanceWeights[3] = { 0.27f, 0.67f, 0.06f };
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

		struct FogEffect
		{
			FogEffect() :
				Material(nullptr), Quad(nullptr), OutputTexture(nullptr), DepthTexture(nullptr)
			{}

			~FogEffect()
			{
				DeleteObject(Material);
				DeleteObject(Quad);
				ReleaseObject(OutputTexture);
				ReleaseObject(DepthTexture);
			}

			FogMaterial* Material;
			FullScreenQuad* Quad;
			ID3D11ShaderResourceView* OutputTexture;
			ID3D11ShaderResourceView* DepthTexture;
			bool isActive = true;
			float color[3] = { 166.0f / 255.0f, 188.0f / 255.0f, 196.0f / 255.0f };
			float density = 730.0f;
			float nearZ = 0.0f;
			float farZ = 0.0f;
		};

		struct LightShaftsEffect
		{
			LightShaftsEffect() :
				Material(nullptr), Quad(nullptr), OutputTexture(nullptr), DepthTexture(nullptr)
			{}

			~LightShaftsEffect()
			{
				DeleteObject(Material);
				DeleteObject(Quad);
				ReleaseObject(OutputTexture);
				ReleaseObject(DepthTexture);
			}

			LightShaftsMaterial* Material;
			FullScreenQuad* Quad;
			ID3D11ShaderResourceView* OutputTexture;
			ID3D11ShaderResourceView* DepthTexture;
			bool isActive = true;
			bool isForceEnable = true;
			bool isSunOnScreen = false;

			float decay = 0.995f;
			float weight = 6.65f;
			float exposure = 0.011f;
			float density = 0.94f;
			float maxSunDistanceDelta = 10.0f;
			float intensity = 0.1f;
		};

		struct SSREffect
		{
			SSREffect() :
				Material(nullptr), Quad(nullptr), InputColor(nullptr)
			{}

			~SSREffect()
			{
				DeleteObject(Material);
				DeleteObject(Quad);
				ReleaseObject(InputColor);

			}

			ScreenSpaceReflectionsMaterial* Material;
			FullScreenQuad* Quad;
			ID3D11ShaderResourceView* InputColor;

			bool isActive = true;
			int rayCount = 50;
			float stepSize = 0.741f;
			float maxThickness = 0.00021f;
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
		void UpdateSSRMaterial(ID3D11ShaderResourceView* normal, ID3D11ShaderResourceView* depth, ID3D11ShaderResourceView* extra, float time);
		void UpdateFogMaterial();
		void UpdateLightShaftsMaterial();
		void UpdateCompositeLightingMaterial(ID3D11ShaderResourceView* localIlluminationSRV, ID3D11ShaderResourceView* globalIlluminationSRV, bool debugVoxel, bool debugAO);

		void Initialize(bool pTonemap, bool pMotionBlur, bool pColorGrading, bool pVignette, bool pFXAA, bool pSSR = true, bool pFog = false, bool pLightShafts = false);
		void Begin(bool clear = true, DepthTarget* aDepthTarget = nullptr, bool clearDepth = false);
		void End();
		void BeginRenderingToExtraRT(bool clear);
		void EndRenderingToExtraRT();
		void SetMainRT(ID3D11ShaderResourceView* srv);
		void ResetMainRTtoOriginal();

		void DrawEffects(const GameTime & gameTime);

		void Update();
		void DrawFullscreenQuad(ID3D11DeviceContext* pContext);

		void SetDirectionalLight(const DirectionalLight* pLight) { light = pLight; }
		void SetSunOcclusionSRV(ID3D11ShaderResourceView* srv) { mSunOcclusionSRV = srv; }
		void SetSunNDCPos(XMFLOAT2 pos) { mSunNDCPos = pos; }

		void Config() { mShowDebug = !mShowDebug; }

		CustomRenderTarget* GetMainRenderTarget() { return mMainRenderTarget; }

		ID3D11ShaderResourceView* GetDepthSRV();
		ID3D11ShaderResourceView* GetPrepassColorSRV();
		ID3D11ShaderResourceView* GetExtraColorSRV();

		bool isWindowOpened = false;

	private:
		void UpdateTonemapConstantBuffer(ID3D11DeviceContext * pD3DImmediateContext, ID3D11Buffer* buffer, int mipLevel0, int mipLevel1, unsigned int width, unsigned int height);
		void ComputeLuminance(ID3D11DeviceContext* pContext, ID3D11ShaderResourceView* pInput, ID3D11RenderTargetView* pOutput);
		void ComputeBrightPass(ID3D11DeviceContext* pContext, ID3D11ShaderResourceView* pInput, ID3D11RenderTargetView* pOutput);
		void ComputeHorizontalBlur(ID3D11DeviceContext* pContext, ID3D11ShaderResourceView* pInput, ID3D11RenderTargetView* pOutput);
		void ComputeVerticalBlur(ID3D11DeviceContext* pContext, ID3D11ShaderResourceView* pInput, ID3D11RenderTargetView* pOutput);
		void ComputeToneMapWithBloom(ID3D11DeviceContext* pContext, ID3D11ShaderResourceView* pInput, ID3D11ShaderResourceView* pAVG, ID3D11ShaderResourceView* pBloom, ID3D11RenderTargetView* pOutput);
		void PerformEmptyPass(ID3D11DeviceContext * pContext, ID3D11ShaderResourceView * pInput, ID3D11RenderTargetView * pOutput);
		void ShowPostProcessingWindow();

		Game& game;
		Camera& camera;
		const DirectionalLight* light;

		EffectElements::VignetteEffect* mVignetteEffect;
		EffectElements::ColorGradingEffect* mColorGradingEffect;
		EffectElements::MotionBlurEffect* mMotionBlurEffect;
		EffectElements::FXAAEffect* mFXAAEffect;
		EffectElements::TonemapEffect* mTonemapEffect;
		EffectElements::SSREffect* mSSREffect;
		EffectElements::FogEffect* mFogEffect;
		EffectElements::LightShaftsEffect* mLightShaftsEffect;
		EffectElements::CompositeLightingEffect* mCompositeLightingEffect;
		
		CustomRenderTarget* mMainRenderTarget = nullptr;
		DepthTarget* mMainDepthTarget = nullptr;

		//TODO change to custom render targets
		FullScreenRenderTarget* mVignetteRenderTarget;
		FullScreenRenderTarget* mColorGradingRenderTarget;
		FullScreenRenderTarget* mMotionBlurRenderTarget;
		FullScreenRenderTarget* mFXAARenderTarget;
		FullScreenRenderTarget* mTonemapRenderTarget;
		FullScreenRenderTarget* mSSRRenderTarget;
		FullScreenRenderTarget* mFogRenderTarget;
		FullScreenRenderTarget* mLightShaftsRenderTarget;
		FullScreenRenderTarget* mCompositeLightingRenderTarget;
		FullScreenRenderTarget* mExtraRenderTarget;

		ID3D11Buffer* mQuadVB;
		ID3D11VertexShader* mFullScreenQuadVS;
		ID3D11InputLayout* mFullScreenQuadLayout;

		ID3D11ShaderResourceView* mOriginalMainRTSRV = nullptr;
		ID3D11ShaderResourceView* mSunOcclusionSRV = nullptr;

		bool mVignetteLoaded = false;
		bool mColorGradingLoaded = false;
		bool mMotionBlurLoaded = false;
		bool mFXAALoaded = false;
		bool mSSRLoaded = false;
		bool mFogLoaded = false;
		bool mLightShaftsLoaded = false;
		bool mCompositeLightingLoaded = false;

		XMFLOAT2 mSunNDCPos;

		bool mShowDebug = false;
	};
}