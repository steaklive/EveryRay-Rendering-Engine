#pragma once
#include "Common.h"
#include "ER_Core.h"
#include "ER_CoreTime.h"

#define MAX_POST_EFFECT_VOLUMES 64

namespace EveryRay_Core
{
	class ER_Camera;
	class ER_DirectionalLight;
	class ER_QuadRenderer;
	class ER_GBuffer;
	class ER_VolumetricClouds;
	class ER_VolumetricFog;
	class ER_RenderableAABB;

	namespace PostEffectsCBuffers
	{
		struct ER_ALIGN_GPU_BUFFER FXAACB
		{
			XMFLOAT2 ScreenDimensions;
		};	
		struct ER_ALIGN_GPU_BUFFER VignetteCB
		{
			XMFLOAT2 RadiusSoftness;
		};
		struct ER_ALIGN_GPU_BUFFER LinearFogCB
		{
			XMFLOAT4 FogColor;
			float FogNear;
			float FogFar;
			float FogDensity;
		};
		struct ER_ALIGN_GPU_BUFFER SSRCB
		{
			XMMATRIX InvProjMatrix;
			XMMATRIX InvViewMatrix;
			XMMATRIX ViewProjMatrix;
			XMFLOAT4 CameraPosition;
			float StepSize;
			float MaxThickness;
			float Time;
			int MaxRayCount;
		};
		struct ER_ALIGN_GPU_BUFFER SSSCB
		{
			XMFLOAT4 SSSStrengthWidthDir;
			float CameraFOV;
		};
	}

	struct PostEffectsVolumeValues
	{
		float linearFogColor[3];
		float linearFogDensity;
		bool linearFogEnable;

		float vignetteSoftness;
		float vignetteRadius;
		bool vignetteEnable;

		bool tonemappingEnable;
		
		bool colorGradingEnable;
		int colorGradingLUTIndex;

		bool sssEnable;
		bool ssrEnable;
	};
	struct PostEffectsVolume
	{
		PostEffectsVolume(ER_Core& pCore, const XMFLOAT4X4& aTransform, const PostEffectsVolumeValues& aValues, const std::string& aName);
		~PostEffectsVolume();

		void UpdateDebugVolumeAABB();
		void DrawDebugVolume(ER_RHI_GPUTexture* aRenderTarget, ER_RHI_GPUTexture* aDepth, ER_RHI_GPURootSignature* rs);

		void SetTransform(const XMFLOAT4X4& aTransform, bool updateAABB = true);
		const XMFLOAT4X4& GetTransform() const { return worldTransform; }

		PostEffectsVolumeValues values = {};

		XMFLOAT4X4 worldTransform = 
		{
			1.f, 0.f, 0.f, 0.f,
			0.f, 1.f, 0.f, 0.f,
			0.f, 0.f, 1.f, 0.f,
			0.f, 0.f, 0.f, 1.f
		};
		XMFLOAT3 currentAABBVertices[8];

		ER_RenderableAABB* debugGizmoAABB = nullptr;
		ER_AABB aabb;

		std::string name;
		bool isEnabled = true;
	};

	class ER_PostProcessingStack
	{
	public:
		ER_PostProcessingStack(ER_Core& pCore, ER_Camera& pCamera);
		~ER_PostProcessingStack();

		void Initialize();
	
		void Begin(ER_RHI_GPUTexture* aInitialRT, ER_RHI_GPUTexture* aDepthTarget);
		void End(ER_RHI_GPUTexture* aResolveRT = nullptr);

		void DrawEffects(const ER_CoreTime& gameTime, ER_QuadRenderer* quad, ER_GBuffer* gbuffer, 
			ER_VolumetricClouds* aVolumetricClouds = nullptr, ER_VolumetricFog* aVolumetricFog = nullptr);
		void DrawPostEffectsVolumesDebugGizmos(ER_RHI_GPUTexture* aRenderTarget, ER_RHI_GPUTexture* aDepth, ER_RHI_GPURootSignature* rs);

		void Update();
		void Config() { mShowDebug = !mShowDebug; }

		void SetUseLinearFog(bool value) { mUseLinearFog = value; }
		void SetLinearFogColor(float r, float g, float b) { mLinearFogColor[0] = r; mLinearFogColor[1] = g; mLinearFogColor[2] = b; }
		void SetLinearFogDensity(float value) { mLinearFogDensity = value; }

		void SetUseAntiAliasing(bool value) { mUseFXAA = value; }
		void SetUseSSS(bool value) { mUseSSS = value; }
		void SetUseSSR(bool value) { mUseSSR = value; }

		void SetUseColorGrading(bool value) { mUseColorGrading = value; }
		void SetColorGradingLUT(UINT value) { mColorGradingCurrentLUTIndex = value; }

		void SetUseVignette(bool value) { mUseVignette = value; }
		void SetVignetteSoftness(float value) { mVignetteSoftness = value; }
		void SetVignetteRadius(float value) { mVignetteRadius = value; }

		void SetUseTonemapping(bool value) { mUseTonemap = value; }

		void ReservePostEffectsVolumes(int count);
		bool AddPostEffectsVolume(const XMFLOAT4X4& aTransform, const PostEffectsVolumeValues& aValues, const std::string& aName);
		int GetPostEffectsVolumesCount() const { return static_cast<int>(mPostEffectsVolumes.size()); }
		const PostEffectsVolume& GetPostEffectsVolume(int index) const { return mPostEffectsVolumes[index]; }

		bool isWindowOpened = false;
	private:
		void UpdatePostEffectsVolumes();
		void SetPostEffectsValuesFromVolume(int index = -1);

		void PrepareDrawingTonemapping(ER_RHI_GPUTexture* aInputTexture, ER_GBuffer* gbuffer);
		void PrepareDrawingSSR(const ER_CoreTime& gameTime, ER_RHI_GPUTexture* aInputTexture, ER_GBuffer* gbuffer);
		void PrepareDrawingSSS(const ER_CoreTime& gameTime, ER_RHI_GPUTexture* aInputTexture, ER_GBuffer* gbuffer, bool verticalPass);
		void PrepareDrawingLinearFog(ER_RHI_GPUTexture* aInputTexture);
		void PrepareDrawingColorGrading(ER_RHI_GPUTexture* aInputTexture);
		void PrepareDrawingVignette(ER_RHI_GPUTexture* aInputTexture);
		void PrepareDrawingFXAA(ER_RHI_GPUTexture* aInputTexture);
	
		void ShowPostProcessingWindow();

		ER_Core& mCore;
		ER_Camera& mCamera;

		// volumes
		std::vector<PostEffectsVolume> mPostEffectsVolumes;
		int mCurrentActivePostEffectsVolumeIndex = -1; // the one we are currently in
		int mSelectedEditorPostEffectsVolumeIndex = -1; // the one selected for editing via ImGui
		float mEditorPostEffectsVolumeMatrixTranslation[3];
		float mEditorPostEffectsVolumeMatrixRotation[3];
		float mEditorPostEffectsVolumeMatrixScale[3];
		float mEditorCurrentPostEffectsVolumeTransformMatrix[16] =
		{
			1.f, 0.f, 0.f, 0.f,
			0.f, 1.f, 0.f, 0.f,
			0.f, 0.f, 1.f, 0.f,
			0.f, 0.f, 0.f, 1.f
		};
		const char* mEditorPostEffectsVolumesNames[MAX_POST_EFFECT_VOLUMES] = { nullptr };
		bool mShowDebugVolumes = true;

		// Tonemap
		ER_RHI_GPUTexture* mTonemappingRT = nullptr;
		ER_RHI_GPUShader* mTonemappingPS = nullptr;
		bool mUseTonemapDefault = true;
		bool mUseTonemap = true;
		std::string mTonemapPassPSOName = "ER_RHI_GPUPipelineStateObject: Post Processing - Tonemap";
		ER_RHI_GPURootSignature* mTonemapRS = nullptr;

		// SSR
		ER_RHI_GPUTexture* mSSRRT = nullptr;
		ER_RHI_GPUConstantBuffer<PostEffectsCBuffers::SSRCB> mSSRConstantBuffer;
		ER_RHI_GPUShader* mSSRPS = nullptr;
		bool mUseSSRDefault = false;
		bool mUseSSR = false;
		int mSSRRayCount = 50;
		float mSSRStepSize = 0.741f;
		float mSSRMaxThickness = 0.00021f;
		std::string mSSRPassPSOName = "ER_RHI_GPUPipelineStateObject: Post Processing - SSR";
		ER_RHI_GPURootSignature* mSSRRS = nullptr;

		// SSS
		ER_RHI_GPUTexture* mSSSRT = nullptr;
		bool mUseSSSDefault = true;
		bool mUseSSS = true;
		ER_RHI_GPUShader* mSSSPS = nullptr;
		ER_RHI_GPUConstantBuffer<PostEffectsCBuffers::SSSCB> mSSSConstantBuffer;
		std::string mSSSPassPSOName = "ER_RHI_GPUPipelineStateObject: Post Processing - SSS";
		ER_RHI_GPURootSignature* mSSSRS = nullptr;

		// Linear Fog
		ER_RHI_GPUTexture* mLinearFogRT = nullptr;
		ER_RHI_GPUConstantBuffer<PostEffectsCBuffers::LinearFogCB> mLinearFogConstantBuffer;
		ER_RHI_GPUShader* mLinearFogPS = nullptr;
		bool mUseLinearFogDefault = false;
		bool mUseLinearFog = false;
		float mLinearFogColorDefault[3] = { 166.0f / 255.0f, 188.0f / 255.0f, 196.0f / 255.0f };
		float mLinearFogColor[3] = { 166.0f / 255.0f, 188.0f / 255.0f, 196.0f / 255.0f };
		float mLinearFogDensityDefault = 730.0f;
		float mLinearFogDensity = 730.0f;
		float mLinearFogNearZ = 0.0f;
		float mLinearFogFarZ = 0.0f;
		std::string mLinearFogPassPSOName = "ER_RHI_GPUPipelineStateObject: Post Processing - Linear Fog";
		ER_RHI_GPURootSignature* mLinearFogRS = nullptr;

		ER_RHI_GPUTexture* mVolumetricFogRT = nullptr;

		// LUT Color Grading
		ER_RHI_GPUTexture* mColorGradingRT = nullptr;
		ER_RHI_GPUTexture* mLUTs[3] = { nullptr, nullptr, nullptr };
		ER_RHI_GPUShader* mColorGradingPS = nullptr;
		const UINT mColorGradingCurrentLUTIndexDefault = 2;
		UINT mColorGradingCurrentLUTIndex = 2;
		bool mUseColorGradingDefault = true;
		bool mUseColorGrading = true;
		std::string mColorGradingPassPSOName = "ER_RHI_GPUPipelineStateObject: Post Processing - Color Grading";
		ER_RHI_GPURootSignature* mColorGradingRS = nullptr;

		// FXAA
		ER_RHI_GPUTexture* mFXAART = nullptr;
		ER_RHI_GPUConstantBuffer<PostEffectsCBuffers::FXAACB> mFXAAConstantBuffer;
		ER_RHI_GPUShader* mFXAAPS = nullptr;
		bool mUseFXAA = true;
		std::string mFXAAPassPSOName = "ER_RHI_GPUPipelineStateObject: Post Processing - FXAA";
		ER_RHI_GPURootSignature* mFXAARS = nullptr;

		// Vignette
		ER_RHI_GPUTexture* mVignetteRT = nullptr;
		ER_RHI_GPUConstantBuffer<PostEffectsCBuffers::VignetteCB> mVignetteConstantBuffer;
		ER_RHI_GPUShader* mVignettePS = nullptr;
		float mVignetteRadiusDefault = 0.75f;
		float mVignetteRadius = 0.0f;
		float mVignetteSoftnessDefault = 0.5f;
		float mVignetteSoftness = 0.5f;
		bool mUseVignetteDefault = true;
		bool mUseVignette = true;
		std::string mVignettePassPSOName = "ER_RHI_GPUPipelineStateObject: Post Processing - Vignette";
		ER_RHI_GPURootSignature* mVignetteRS = nullptr;

		ER_RHI_GPUShader* mFinalResolvePS = nullptr;
		std::string mFinalResolvePassPSOName = "ER_RHI_GPUPipelineStateObject: Post Processing - Final Resolve";
		ER_RHI_GPURootSignature* mFinalResolveRS = nullptr;

		// just pointers to RTs (not allocated in this system)
		ER_RHI_GPUTexture* mRenderTargetBeforeResolve = nullptr;
		ER_RHI_GPUTexture* mRenderTargetBeforePostProcessingPasses = nullptr;
		ER_RHI_GPUTexture* mDepthTarget = nullptr;

		bool mShowDebug = false;
	};
}