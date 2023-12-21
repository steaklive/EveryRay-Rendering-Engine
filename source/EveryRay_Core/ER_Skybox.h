#pragma once
#include "Common.h"
#include "ER_CoreComponent.h"
#include "RHI/ER_RHI.h"

namespace EveryRay_Core
{
	class ER_Camera;
	class ER_CoreTime;
	class ER_DirectionalLight;

	namespace SkyCBufferData {
		struct ER_ALIGN_GPU_BUFFER SunData {
			XMMATRIX InvProj;
			XMMATRIX InvView;
			XMFLOAT4 SunDir;
			XMFLOAT4 SunColor;
			float SunExponent;
			float SunBrightness;
		};

		struct ER_ALIGN_GPU_BUFFER SkyboxData
		{
			XMMATRIX WorldViewProjection;
			XMFLOAT4 SunColor;
			XMFLOAT4 TopColor;
			XMFLOAT4 BottomColor;
			XMFLOAT2 SkyHeight;
		};
	}

	class ER_Skybox : public ER_CoreComponent
	{
	public:
		ER_Skybox(ER_Core& game, ER_Camera& camera, ER_DirectionalLight& light, float scale);
		~ER_Skybox();

		void Initialize();
		void Draw(ER_RHI_GPUTexture* aRenderTarget, ER_Camera* aCustomCamera = nullptr, ER_RHI_GPUTexture* aSceneDepth = nullptr, bool isVolumetricCloudsPass = false);
		void DrawSun(ER_RHI_GPUTexture* aRenderTarget, ER_Camera* aCustomCamera = nullptr, ER_RHI_GPUTexture* aSky = nullptr, ER_RHI_GPUTexture* aSceneDepth = nullptr, bool isVolumetricCloudsPass = false);
		void Update(const ER_CoreTime& gameTime, ER_Camera* aCustomCamera = nullptr);
		void UpdateSun(const ER_CoreTime& gameTime, ER_Camera* aCustomCamera = nullptr);

		bool mUseCustomSkyboxColor = true;
		float mBottomColorEditor[4] = { 245.0f / 255.0f, 245.0f / 255.0f, 245.0f / 255.0f, 1.0f };
		float mTopColorEditor[4] = { 0.0f / 255.0f, 133.0f / 255.0f, 191.0f / 255.0f, 1.0f };
		float mSkyMinHeightEditor = 0.191f;
		float mSkyMaxHeightEditor = 4.2f;
	private:
		XMFLOAT4 CalculateSunPositionOnSkybox(XMFLOAT3 dir, ER_Camera* aCustomCamera = nullptr);
		
		ER_Core& mCore;
		ER_Camera& mCamera;
		ER_DirectionalLight& mSun;

		ER_RHI_GPUBuffer* mVertexBuffer = nullptr;
		ER_RHI_GPUBuffer* mIndexBuffer = nullptr;
		ER_RHI_InputLayout* mInputLayout = nullptr;
		UINT mIndexCount;

		XMFLOAT4X4 mWorldMatrix;
		XMFLOAT4X4 mScaleMatrix;

		bool mDrawSun = true; // on top of skybox
		ER_RHI_GPUShader* mSunPS = nullptr;
		ER_RHI_GPUShader* mSunOcclusionPS = nullptr;
		ER_RHI_GPUConstantBuffer<SkyCBufferData::SunData> mSunConstantBuffer;	
		ER_RHI_GPURootSignature* mSunRS = nullptr;
		const std::string mSunPassPSOName = "ER_RHI_GPUPipelineStateObject: Sun Pass";
		const std::string mSunPassVolumetricCloudsPSOName = "ER_RHI_GPUPipelineStateObject: Sun Pass (Volumetric Clouds)"; // due to RT format mismatch

		ER_RHI_GPUShader* mSkyboxVS = nullptr;
		ER_RHI_GPUShader* mSkyboxPS = nullptr;
		ER_RHI_GPUConstantBuffer<SkyCBufferData::SkyboxData> mSkyboxConstantBuffer;
		ER_RHI_GPURootSignature* mSkyRS = nullptr;
		const std::string mSkyboxPassPSOName = "ER_RHI_GPUPipelineStateObject: Skybox Pass";
		const std::string mSkyboxPassVolumetricCloudsPSOName = "ER_RHI_GPUPipelineStateObject: Skybox Pass (Volumetric Clouds)"; // due to RT format mismatch

		//sky
		XMFLOAT4 mBottomColor;
		XMFLOAT4 mTopColor;
		XMFLOAT2 mSkyHeight;
		float mScale;

		//sun
		XMFLOAT4 mSunDir;
		XMFLOAT4 mSunColor;
		float mSunExponent;
		float mSunBrightness;
	};
}