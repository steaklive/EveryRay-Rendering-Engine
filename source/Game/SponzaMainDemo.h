#pragma once

#include "..\Library\DrawableGameComponent.h"
#include "..\Library\DemoLevel.h"
#include "..\Library\Frustum.h"

using namespace Library;

namespace Library
{
	class Effect;
	class Keyboard;
	class Light;
	class RenderStateHelper;
	class DepthMap;
	class Projector;
	class DirectionalLight;
	class ProxyModel;
	class Skybox;
	class Frustum;
	class FullScreenRenderTarget;
}

namespace DirectX
{
	class SpriteBatch;
	class SpriteFont;
}

namespace Rendering
{
	class RenderingObject;
	class PostProcessingStack;

	class SponzaMainDemo : public DrawableGameComponent, public DemoLevel
	{
		RTTI_DECLARATIONS(SponzaMainDemo, DrawableGameComponent)

	public:
		SponzaMainDemo(Game& game, Camera& camera);
		~SponzaMainDemo();

		virtual void Initialize() override;
		virtual void Update(const GameTime& gameTime) override;
		virtual void Draw(const GameTime& gameTime) override;

		virtual void Create() override;
		virtual void Destroy() override;
		virtual bool IsComponent() override;

	private:
		SponzaMainDemo();
		SponzaMainDemo(const SponzaMainDemo& rhs);
		SponzaMainDemo& operator=(const SponzaMainDemo& rhs);

		void UpdateStandardLightingMaterialVariables(int meshIndex);
		void UpdateDepthMapMaterialVariables(int meshIndex);
		void UpdateDirectionalLightAndProjector(const GameTime & gameTime);
		void UpdateImGui();
		XMMATRIX GetProjectionMatrixFromFrustum(Frustum & cameraFrustum, DirectionalLight& light);

		static const float AmbientModulationRate;
		static const XMFLOAT2 LightRotationRate;

		Keyboard* mKeyboard;
		XMFLOAT4X4 mWorldMatrix;

		RenderingObject* mSponzaLightingRenderingObject;
		RenderingObject* mSponzaShadowRenderingObject;

		Projector* mShadowProjector;
		DepthMap* mShadowMap;

		ProxyModel* mProxyModel;
		DirectionalLight* mDirectionalLight;
		Frustum mLightFrustum;
		XMFLOAT3 mLightFrustumCenter;

		ID3D11RasterizerState* mShadowRasterizerState;
		RenderStateHelper* mRenderStateHelper;

		Skybox* mSkybox;

		PostProcessingStack* mPostProcessingStack;

		float mSunColor[3] = { 1.0f, 1.0f, 1.0f };
		float mAmbientColor[3] = { 0.1f, 0.1f, 0.1f };

	};
}