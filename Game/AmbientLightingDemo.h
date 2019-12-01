#pragma once

#include "..\Library\DrawableGameComponent.h"
#include "..\Library\DemoLevel.h"
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
	class RenderingObject;

	class AmbientLightingDemo : public DrawableGameComponent, public DemoLevel
	{
		RTTI_DECLARATIONS(AmbientLightingDemo, DrawableGameComponent)

	public:
		AmbientLightingDemo(Game& game, Camera& camera);
		~AmbientLightingDemo();

		virtual void Initialize() override;
		virtual void Update(const GameTime& gameTime) override;
		virtual void Draw(const GameTime& gameTime) override;


		virtual void Create() override;
		virtual void Destroy() override;
		virtual bool IsComponent() override;

	private:
		AmbientLightingDemo();
		AmbientLightingDemo(const AmbientLightingDemo& rhs);
		AmbientLightingDemo& operator=(const AmbientLightingDemo& rhs);

		void UpdateAmbientLight(const GameTime& gameTime);
		void UpdateAmbientMaterialVariables(int meshIndex);

		static const float AmbientModulationRate;

		//Effect* mEffect;
		//AmbientLightingMaterial* mMaterial;
		ID3D11ShaderResourceView* mTextureShaderResourceView;
		//ID3D11Buffer* mVertexBuffer;
		//ID3D11Buffer* mIndexBuffer;
		//UINT mIndexCount;

		Keyboard* mKeyboard;
		Light* mAmbientLight;
		XMFLOAT4X4 mWorldMatrix;

		RenderStateHelper* mRenderStateHelper;
		SpriteBatch* mSpriteBatch;
		SpriteFont* mSpriteFont;
		XMFLOAT2 mTextPosition;

		RenderingObject* mObject;
	};
}