#pragma once

#include "..\Library\DrawableGameComponent.h"

using namespace Library;

namespace Library
{
	class Effect;
	class Keyboard;
	class Light;
	class RenderStateHelper;
}

namespace DirectX
{
	class SpriteBatch;
	class SpriteFont;
}

namespace Rendering
{
	class AmbientLightingMaterial;

	class AmbientLightingDemo : public DrawableGameComponent
	{
		RTTI_DECLARATIONS(AmbientLightingDemo, DrawableGameComponent)

	public:
		AmbientLightingDemo(Game& game, Camera& camera);
		~AmbientLightingDemo();

		virtual void Initialize() override;
		virtual void Update(const GameTime& gameTime) override;
		virtual void Draw(const GameTime& gameTime) override;

	private:
		AmbientLightingDemo();
		AmbientLightingDemo(const AmbientLightingDemo& rhs);
		AmbientLightingDemo& operator=(const AmbientLightingDemo& rhs);

		void UpdateAmbientLight(const GameTime& gameTime);

		static const float AmbientModulationRate;

		Effect* mEffect;
		AmbientLightingMaterial* mMaterial;
		ID3D11ShaderResourceView* mTextureShaderResourceView;
		ID3D11Buffer* mVertexBuffer;
		ID3D11Buffer* mIndexBuffer;
		UINT mIndexCount;

		Keyboard* mKeyboard;
		Light* mAmbientLight;
		XMFLOAT4X4 mWorldMatrix;

		RenderStateHelper* mRenderStateHelper;
		SpriteBatch* mSpriteBatch;
		SpriteFont* mSpriteFont;
		XMFLOAT2 mTextPosition;
	};
}