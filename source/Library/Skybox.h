#pragma once
#include "Common.h"
#include "DrawableGameComponent.h"
#include "PostProcessingStack.h"
#include "ConstantBuffer.h"

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

	class Skybox : public DrawableGameComponent
	{
		RTTI_DECLARATIONS(Skybox, DrawableGameComponent)

	public:
		Skybox(Game& game, Camera& camera, const std::wstring& cubeMapFileName, float scale);
		~Skybox();

		virtual void Initialize() override;
		virtual void Update(const GameTime& gameTime) override;
		virtual void Draw(const GameTime& gameTime) override;
		void DrawSun(const GameTime& gametime, Rendering::PostProcessingStack* postprocess = nullptr);

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
		ID3D11ShaderResourceView* GetSunOutputTexture();
	private:
		Skybox();
		Skybox(const Skybox& rhs);
		Skybox& operator=(const Skybox& rhs);

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
		ConstantBuffer<SunCBufferData::SunData> mSunConstantBuffer;

		XMVECTOR mSunDir;
		XMVECTOR mSunColor;
		float mSunExponent;
		float mSunBrightness;
		FullScreenRenderTarget* mSunRenderTarget = nullptr;

	};
}