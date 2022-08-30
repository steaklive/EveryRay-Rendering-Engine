#pragma once
#include "Common.h"
#include "ER_CoreComponent.h"
#include "RHI/ER_RHI.h"

namespace EveryRay_Core
{
	class ER_Camera;
	class ER_CoreTime;

	namespace SkyCBufferData {
		struct ER_ALIGN16 SunData {
			XMMATRIX InvProj;
			XMMATRIX InvView;
			XMFLOAT4 SunDir;
			XMFLOAT4 SunColor;
			float SunExponent;
			float SunBrightness;
		};

		struct ER_ALIGN16 SkyboxData
		{
			XMMATRIX WorldViewProjection;
			XMFLOAT4 SunColor;
			XMFLOAT4 TopColor;
			XMFLOAT4 BottomColor;
		};
	}

	class ER_Skybox : public ER_CoreComponent
	{
	public:
		ER_Skybox(ER_Core& game, ER_Camera& camera, float scale);
		~ER_Skybox();

		void Initialize();
		void Draw(ER_Camera* aCustomCamera = nullptr);
		void Update(ER_Camera* aCustomCamera = nullptr);
		void UpdateSun(const ER_CoreTime& gameTime, ER_Camera* aCustomCamera = nullptr);
		void DrawSun(ER_Camera* aCustomCamera = nullptr, ER_RHI_GPUTexture* aSky = nullptr, ER_RHI_GPUTexture* aSceneDepth = nullptr);

		void SetMovable(bool value) { mIsMovable = value; };
		void SetUseCustomSkyColor(bool value) { mUseCustomColor = value; }
		void SetSkyColors(const XMFLOAT4& bottom, const XMFLOAT4& top) {
			mBottomColor = bottom;
			mTopColor = top;
		}
		void SetSunData(bool rendered, const XMFLOAT4& sunDir, const XMFLOAT4& sunColor, float brightness, float exponent) {
			mDrawSun = rendered;
			mSunDir = sunDir;
			mSunColor = sunColor;
			mSunBrightness = brightness;
			mSunExponent = exponent;
		}
	private:

		XMFLOAT4 CalculateSunPositionOnSkybox(XMFLOAT3 dir, ER_Camera* aCustomCamera = nullptr);
		
		ER_Core& mCore;
		ER_Camera& mCamera;

		ER_RHI_GPUBuffer* mVertexBuffer = nullptr;
		ER_RHI_GPUBuffer* mIndexBuffer = nullptr;
		ER_RHI_InputLayout* mInputLayout = nullptr;
		UINT mIndexCount;

		XMFLOAT4X4 mWorldMatrix;
		XMFLOAT4X4 mScaleMatrix;

		bool mIsMovable = false;
		bool mUseCustomColor = false;

		XMFLOAT4 mBottomColor;
		XMFLOAT4 mTopColor;

		bool mDrawSun = true;
		ER_RHI_GPUShader* mSunPS = nullptr;
		ER_RHI_GPUShader* mSunOcclusionPS = nullptr;
		ER_RHI_GPUConstantBuffer<SkyCBufferData::SunData> mSunConstantBuffer;	
		
		ER_RHI_GPUShader* mSkyboxVS = nullptr;
		ER_RHI_GPUShader* mSkyboxPS = nullptr;
		ER_RHI_GPUConstantBuffer<SkyCBufferData::SkyboxData> mSkyboxConstantBuffer;

		XMFLOAT4 mSunDir;
		XMFLOAT4 mSunColor;
		float mSunExponent;
		float mSunBrightness;
		float mScale;
	};
}