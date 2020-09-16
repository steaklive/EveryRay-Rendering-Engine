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
}

namespace Rendering
{
	class RenderingObject;
	class PostProcessingStack;

	class TerrainDemo : public DrawableGameComponent, public DemoLevel
	{
		RTTI_DECLARATIONS(TerrainDemo, DrawableGameComponent)

	public:
		TerrainDemo(Game& game, Camera& camera);
		~TerrainDemo();

		virtual void Initialize() override;
		virtual void Update(const GameTime& gameTime) override;
		virtual void Draw(const GameTime& gameTime) override;

		virtual void Create() override;
		virtual void Destroy() override;
		virtual bool IsComponent() override;

	private:
		TerrainDemo();
		TerrainDemo(const TerrainDemo& rhs);
		TerrainDemo& operator=(const TerrainDemo& rhs);

		void UpdateStandardLightingPBRMaterialVariables(const std::string& objectName, int meshIndex);
		void UpdateDeferredPrepassMaterialVariables(const std::string& objectName, int meshIndex);
		void UpdateShadow0MaterialVariables(const std::string & objectName, int meshIndex);
		void UpdateShadow1MaterialVariables(const std::string & objectName, int meshIndex);
		void UpdateShadow2MaterialVariables(const std::string & objectName, int meshIndex);
		void UpdateImGui();

		std::map<std::string, RenderingObject*> mRenderingObjects;

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
		Terrain* mTerrain;

		ID3D11ShaderResourceView* mIrradianceTextureSRV;
		ID3D11ShaderResourceView* mRadianceTextureSRV;
		ID3D11ShaderResourceView* mIntegrationMapTextureSRV;
		std::unique_ptr<IBLRadianceMap> mIBLRadianceMap;
	};
}