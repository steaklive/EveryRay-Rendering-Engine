#pragma once
#include "Common.h"
#include "DrawableGameComponent.h"
#include "ER_PostProcessingStack.h"
#include "ConstantBuffer.h"
#include "DepthTarget.h"

namespace Library
{
	class Effect;
	class SkyboxMaterial;
	class FullScreenRenderTarget;

	namespace SunCBufferData {
		struct SunData {
			XMMATRIX InvProj;
			XMMATRIX InvView;
			XMVECTOR SunDir;
			XMVECTOR SunColor;
			float SunExponent;
			float SunBrightness;
		};
	}

	class Skybox
	{
	public:
		Skybox(Game& game, Camera& camera, const std::wstring& cubeMapFileName, float scale);
		~Skybox();

		void Initialize();
		void Draw(Camera* aCustomCamera = nullptr);
		void Update(const GameTime& gameTime, Camera* aCustomCamera = nullptr);
		void UpdateSun(const GameTime& gameTime, Camera* aCustomCamera = nullptr);
		void DrawSun(Camera* aCustomCamera = nullptr, ER_GPUTexture* aSky = nullptr, DepthTarget* aSceneDepth = nullptr);

		void SetMovable(bool value) { mIsMovable = value; };
		void SetUseCustomSkyColor(bool value) { mUseCustomColor = value; }
		void SetSkyColors(XMFLOAT4 bottom, XMFLOAT4 top) { 
			mBottomColor = bottom;
			mTopColor = top;
		}
		void SetSunData(bool rendered, XMVECTOR sunDir, XMVECTOR sunColor, float brightness, float exponent) {
			mDrawSun = rendered;
			mSunDir = sunDir;
			mSunColor = sunColor;
			mSunBrightness = brightness;
			mSunExponent = exponent;
		}
		ID3D11ShaderResourceView* GetSunOcclusionOutputTexture() const;
	private:
		Skybox();
		Skybox(const Skybox& rhs);
		Skybox& operator=(const Skybox& rhs);
		XMFLOAT4 CalculateSunPositionOnSkybox(XMFLOAT3 dir, Camera* aCustomCamera = nullptr);
		Game& mGame;
		Camera& mCamera;

		std::wstring mCubeMapFileName;
		Effect* mEffect;
		SkyboxMaterial* mMaterial;
		ID3D11ShaderResourceView* mCubeMapShaderResourceView;
		ID3D11Buffer* mVertexBuffer;
		ID3D11Buffer* mIndexBuffer;
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
		ConstantBuffer<SunCBufferData::SunData> mSunConstantBuffer;

		XMVECTOR mSunDir;
		XMVECTOR mSunColor;
		float mSunExponent;
		float mSunBrightness;
		FullScreenRenderTarget* mSunRenderTarget = nullptr;
		FullScreenRenderTarget* mSunOcclusionRenderTarget = nullptr;
		float mScale;
	};
}