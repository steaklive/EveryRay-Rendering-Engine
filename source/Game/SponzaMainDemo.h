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
	class Grid;
	class IBLRadianceMap;
	class Frustum;
	class FullScreenRenderTarget;
	class FullScreenQuad;
	class GBuffer;
	class ShadowMapper;
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

		void UpdateStandardLightingPBRMaterialVariables(const std::string& objectName, int meshIndex);
		void UpdateDeferredPrepassMaterialVariables(const std::string& objectName, int meshIndex);
		void UpdateSSRMaterialVariables(const std::string & objectName, int meshIndex);
		void UpdateShadowMaterialVariables(const std::string & objectName, int meshIndex);
		void UpdateImGui();

		//void CheckMouseIntersections();

		Keyboard* mKeyboard;
		XMFLOAT4X4 mWorldMatrix;

		std::map<std::string, RenderingObject*> mRenderingObjects;

		FullScreenQuad* mSSRQuad;

		DirectionalLight* mDirectionalLight;
		ShadowMapper* mShadowMapper;

		Skybox* mSkybox;
		Grid* mGrid;

		ID3D11ShaderResourceView* mIrradianceTextureSRV;
		ID3D11ShaderResourceView* mRadianceTextureSRV;
		ID3D11ShaderResourceView* mIntegrationMapTextureSRV;
		std::unique_ptr<IBLRadianceMap> mIBLRadianceMap;

		GBuffer* mGBuffer;

		PostProcessingStack* mPostProcessingStack;
		RenderStateHelper* mRenderStateHelper;
	};
}