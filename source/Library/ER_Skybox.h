#pragma once
#include "Common.h"
#include "ER_CoreComponent.h"
#include "ConstantBuffer.h"
#include "ER_GPUTexture.h"

namespace Library
{
	class ER_Camera;
	class ER_CoreTime;

	namespace SkyCBufferData {
		struct SunData {
			XMMATRIX InvProj;
			XMMATRIX InvView;
			XMFLOAT4 SunDir;
			XMFLOAT4 SunColor;
			float SunExponent;
			float SunBrightness;
		};

		struct SkyboxData
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
		ER_Skybox(Game& game, ER_Camera& camera, float scale);
		~ER_Skybox();

		void Initialize();
		void Draw(ER_Camera* aCustomCamera = nullptr);
		void Update(ER_Camera* aCustomCamera = nullptr);
		void UpdateSun(const ER_CoreTime& gameTime, ER_Camera* aCustomCamera = nullptr);
		void DrawSun(ER_Camera* aCustomCamera = nullptr, ER_GPUTexture* aSky = nullptr, ER_GPUTexture* aSceneDepth = nullptr);

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
		ID3D11ShaderResourceView* GetSunOcclusionOutputTexture() const;
	private:

		XMFLOAT4 CalculateSunPositionOnSkybox(XMFLOAT3 dir, ER_Camera* aCustomCamera = nullptr);
		
		Game& mGame;
		ER_Camera& mCamera;

		ID3D11Buffer* mVertexBuffer = nullptr;
		ID3D11Buffer* mIndexBuffer = nullptr;
		ID3D11InputLayout* mInputLayout = nullptr;
		UINT mIndexCount;

		XMFLOAT4X4 mWorldMatrix;
		XMFLOAT4X4 mScaleMatrix;

		bool mIsMovable = false;
		bool mUseCustomColor = false;

		XMFLOAT4 mBottomColor;
		XMFLOAT4 mTopColor;

		bool mDrawSun = true;
		ID3D11PixelShader* mSunPS = nullptr;
		ID3D11PixelShader* mSunOcclusionPS = nullptr;
		ConstantBuffer<SkyCBufferData::SunData> mSunConstantBuffer;	
		
		ID3D11VertexShader* mSkyboxVS = nullptr;
		ID3D11PixelShader* mSkyboxPS = nullptr;
		ConstantBuffer<SkyCBufferData::SkyboxData> mSkyboxConstantBuffer;

		XMFLOAT4 mSunDir;
		XMFLOAT4 mSunColor;
		float mSunExponent;
		float mSunBrightness;
		float mScale;
	};
}