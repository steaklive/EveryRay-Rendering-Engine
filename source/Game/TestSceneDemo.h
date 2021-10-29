#pragma once

#include "..\Library\DrawableGameComponent.h"
#include "..\Library\DemoLevel.h"

using namespace Library;

namespace Library
{
	class IBLRadianceMap;
}

namespace Rendering
{
	class RenderingObject;

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