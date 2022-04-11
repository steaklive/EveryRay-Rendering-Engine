#pragma once
#include "Common.h"
#include "GameComponent.h"
#include "ConstantBuffer.h"
#include "DepthTarget.h"
#include "ER_GPUTexture.h"

namespace Library
{
	class FullScreenRenderTarget;
	class Camera;
	class GameTime;

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

	class ER_Skybox : public GameComponent
	{
	public:
		ER_Skybox(Game& game, Camera& camera, float scale);
		~ER_Skybox();

		void Initialize();
		void Draw(Camera* aCustomCamera = nullptr);
		void Update(Camera* aCustomCamera = nullptr);
		void UpdateSun(const GameTime& gameTime, Camera* aCustomCamera = nullptr);
		void DrawSun(Camera* aCustomCamera = nullptr, ER_GPUTexture* aSky = nullptr, DepthTarget* aSceneDepth = nullptr);

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

		XMFLOAT4 CalculateSunPositionOnSkybox(XMFLOAT3 dir, Camera* aCustomCamera = nullptr);
		
		Game& mGame;
		Camera& mCamera;

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
		FullScreenRenderTarget* mSunRenderTarget = nullptr;
		FullScreenRenderTarget* mSunOcclusionRenderTarget = nullptr;
		float mScale;
	};
}