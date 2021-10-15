#pragma once

#include "..\Library\DrawableGameComponent.h"
#include "..\Library\DemoLevel.h"
#include "..\Library\ConstantBuffer.h"
#include "..\Library\Frustum.h"
#include "..\Library\CustomRenderTarget.h"

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
	class FoliageSystem;
	class Scene;
	class VolumetricClouds;
	class Illumination;
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

	class TestSceneDemo : public DrawableGameComponent, public DemoLevel
	{
		RTTI_DECLARATIONS(TestSceneDemo, DrawableGameComponent)

	public:
		TestSceneDemo(Game& game, Camera& camera, Editor& editor);
		~TestSceneDemo();

		virtual void Create() override;
		virtual void Destroy() override;
		virtual bool IsComponent() override;
		virtual void UpdateLevel(const GameTime& gameTime) override;
		virtual void DrawLevel(const GameTime& gameTime) override;

	private:
		TestSceneDemo();
		TestSceneDemo(const TestSceneDemo& rhs);
		TestSceneDemo& operator=(const TestSceneDemo& rhs);

		void UpdateStandardLightingPBRMaterialVariables(const std::string& objectName, int meshIndex);
		void UpdateDeferredPrepassMaterialVariables(const std::string& objectName, int meshIndex);
		void UpdateShadow0MaterialVariables(const std::string & objectName, int meshIndex);
		void UpdateShadow1MaterialVariables(const std::string & objectName, int meshIndex);
		void UpdateShadow2MaterialVariables(const std::string & objectName, int meshIndex); 
		void UpdateImGui();
		void Initialize();

		Scene* mScene;
		Keyboard* mKeyboard;
		XMFLOAT4X4 mWorldMatrix;
		RenderStateHelper* mRenderStateHelper;
		Skybox* mSkybox;
		Grid* mGrid;
		ShadowMapper* mShadowMapper;
		FullScreenQuad* mSSRQuad;
		DirectionalLight* mDirectionalLight;
		GBuffer* mGBuffer;
		PostProcessingStack* mPostProcessingStack;
		FoliageSystem* mFoliageSystem;
		VolumetricClouds* mVolumetricClouds;
		Illumination* mGI;

		ID3D11ShaderResourceView* mIrradianceDiffuseTextureSRV;
		ID3D11ShaderResourceView* mIrradianceSpecularTextureSRV;
		ID3D11ShaderResourceView* mIntegrationMapTextureSRV;
		std::unique_ptr<IBLRadianceMap> mIBLRadianceMap;

		float mWindStrength = 1.0f;
		float mWindFrequency = 1.0f;
		float mWindGustDistance = 1.0f;
		//Terrain* mTerrain;
	};
}