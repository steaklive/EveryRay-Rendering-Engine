#pragma once
#include "Common.h"
#include "Game.h"
#include "Camera.h"
#include "GameTime.h"
#include "ER_GPUTexture.h"
#include "DepthTarget.h"
#include "ConstantBuffer.h"

namespace Library
{
	class Effect;
	class FullScreenRenderTarget;
	class FullScreenQuad;
	class DirectionalLight;
	class ER_QuadRenderer;
}

namespace Rendering
{
	class ColorFilterMaterial;
	class VignetteMaterial;
	class ColorGradingMaterial;
	class FXAAMaterial;
	class FogMaterial;
	class ScreenSpaceReflectionsMaterial;
	class LightShaftsMaterial;
	class CompositeLightingMaterial;

	namespace PostEffectsCBuffers
	{
		struct FXAACB
		{
			XMFLOAT2 ScreenDimensions;
		};	
		struct VignetteCB
		{
			XMFLOAT2 RadiusSoftness;
		};
	}

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

			ER_GPUTexture* LuminanceResource = nullptr;
			ER_GPUTexture* AvgLuminanceResource = nullptr;
			ER_GPUTexture* BrightResource = nullptr;
			ER_GPUTexture* BlurHorizontalResource = nullptr;
			ER_GPUTexture* BlurVerticalResource = nullptr;
			ER_GPUTexture* BlurSummedResource = nullptr;

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
	}

	class PostProcessingStack
	{
	public:

		PostProcessingStack() = delete;
		PostProcessingStack(Game& pGame, Camera& pCamera);
		~PostProcessingStack();

		void UpdateColorGradingMaterial();
		void UpdateSSRMaterial(ID3D11ShaderResourceView* normal, ID3D11ShaderResourceView* depth, ID3D11ShaderResourceView* extra, float time);
		void UpdateFogMaterial();
		void UpdateLightShaftsMaterial();

		void Initialize(bool pTonemap, bool pMotionBlur, bool pColorGrading, bool pVignette, bool pFXAA, bool pSSR = true, bool pFog = false, bool pLightShafts = false);
	
		void Begin(ER_GPUTexture* aInitialRT, DepthTarget* aDepthTarget);
		void End(ER_GPUTexture* aResolveRT = nullptr);
		void BeginRenderingToExtraRT(bool clear);
		void EndRenderingToExtraRT();

		void DrawEffects(const GameTime& gameTime, ER_QuadRenderer* quad);

		void Update();

		void SetDirectionalLight(const DirectionalLight* pLight) { light = pLight; }
		void SetSunOcclusionSRV(ID3D11ShaderResourceView* srv) { mSunOcclusionSRV = srv; }
		void SetSunNDCPos(XMFLOAT2 pos) { mSunNDCPos = pos; }

		void Config() { mShowDebug = !mShowDebug; }

		ID3D11ShaderResourceView* GetExtraColorSRV();

		bool isWindowOpened = false;

	private:
		void PrepareDrawingVignette(ER_GPUTexture* aInputTexture);
		void PrepareDrawingFXAA(ER_GPUTexture* aInputTexture);
	
		void UpdateTonemapConstantBuffer(ID3D11DeviceContext * pD3DImmediateContext, ID3D11Buffer* buffer, int mipLevel0, int mipLevel1, unsigned int width, unsigned int height);
		void ShowPostProcessingWindow();

		Game& game;
		Camera& camera;
		const DirectionalLight* light;

		EffectElements::ColorGradingEffect* mColorGradingEffect;
		EffectElements::TonemapEffect* mTonemapEffect;
		EffectElements::SSREffect* mSSREffect;
		EffectElements::FogEffect* mFogEffect;
		EffectElements::LightShaftsEffect* mLightShaftsEffect;
		
		//TODO change to custom render targets
		FullScreenRenderTarget* mColorGradingRenderTarget;
		FullScreenRenderTarget* mTonemapRenderTarget;
		FullScreenRenderTarget* mSSRRenderTarget;
		FullScreenRenderTarget* mFogRenderTarget;
		FullScreenRenderTarget* mLightShaftsRenderTarget;
		FullScreenRenderTarget* mExtraRenderTarget;

		ID3D11ShaderResourceView* mSunOcclusionSRV = nullptr;

		bool mVignetteLoaded = false;
		bool mColorGradingLoaded = false;
		bool mSSRLoaded = false;
		bool mFogLoaded = false;
		bool mLightShaftsLoaded = false;
		bool mCompositeLightingLoaded = false;

		ER_GPUTexture* mFXAART = nullptr;
		ConstantBuffer<PostEffectsCBuffers::FXAACB> mFXAAConstantBuffer;
		ID3D11PixelShader* mFXAAPS = nullptr;
		bool mUseFXAA = true;

		ER_GPUTexture* mVignetteRT = nullptr;
		ConstantBuffer<PostEffectsCBuffers::VignetteCB> mVignetteConstantBuffer;
		ID3D11PixelShader* mVignettePS = nullptr;
		float mVignetteRadius = 0.75f;
		float mVignetteSoftness = 0.5f;
		bool mUseVignette = true;

		ID3D11PixelShader* mFinalResolvePS = nullptr;

		ER_GPUTexture* mFinalTargetBeforeResolve = nullptr; // just a pointer
		ER_GPUTexture* mFirstTargetBeforePostProcessingPasses = nullptr; // just a pointer

		XMFLOAT2 mSunNDCPos;
		bool mShowDebug = false;
	};
}