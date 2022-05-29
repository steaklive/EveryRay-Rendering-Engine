#pragma once

#include "..\Library\DrawableGameComponent.h"
#include "..\Library\ER_Sandbox.h"
#include "..\Library\Frustum.h"

using namespace Library;

namespace Library
{
	class Effect;
	class Keyboard;
	class Light;
	class RenderStateHelper;
	class DepthMap;
	class DirectionalLight;
	class Skybox;
	class Grid;
	class IBLRadianceMap;
	class Frustum;
	class FullScreenRenderTarget;
	class FullScreenQuad;
	class GBuffer;
	class ShadowMapper;
	class Terrain;
	class Foliage;
	class Scene;
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

	class ParallaxMappingDemo : public DrawableGameComponent, public DemoLevel
	{
		RTTI_DECLARATIONS(ParallaxMappingDemo, DrawableGameComponent)

	public:
		ParallaxMappingDemo(Game& game, Camera& camera);
		~ParallaxMappingDemo();

		virtual void Create() override;
		virtual void Destroy() override;

	private:
		ParallaxMappingDemo();
		ParallaxMappingDemo(const ParallaxMappingDemo& rhs);
		ParallaxMappingDemo& operator=(const ParallaxMappingDemo& rhs);

		void UpdateParallaxMaterialVariables(const std::string& objectName, int meshIndex);
		void UpdateImGui();
		void Initialize();

		Scene* mScene;
		Keyboard* mKeyboard;
		XMFLOAT4X4 mWorldMatrix;
		RenderStateHelper* mRenderStateHelper;
		Skybox* mSkybox;
		Grid* mGrid;
		GBuffer* mGBuffer;
		PostProcessingStack* mPostProcessingStack;

		ID3D11ShaderResourceView* mHeightMap;
		ID3D11ShaderResourceView* mEnvMap;

		float mLightPos[3] = { 10.0f, 3.0f, -6.0f };
		float mParallaxScale = 0.15f;
		bool mParallaxUseSoftShadows;
		bool mDemonstrateAO = false;
		bool mDemonstrateNormal = false;

		bool mUseAO = false;
		float mMetalness = 0.5f;
		float mRoughness = 0.5f;
	};
}