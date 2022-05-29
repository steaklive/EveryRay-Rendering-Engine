#pragma once

#include "..\Library\DrawableGameComponent.h"
#include "..\Library\ER_Sandbox.h"
#include "..\Library\Frustum.h"
#include "..\Library\ER_RenderingObject.h"

using namespace Library;

#define PAD16(n) (((n)+15)/16*16)

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
	class GBuffer;
	class ShadowMapper;
	class Terrain;
	class Foliage;
	class Scene;
	class Editor;
	class VolumetricClouds;
}

namespace Rendering
{
	class RenderingObject;
	class PostProcessingStack;

	class TerrainDemo : public DrawableGameComponent, public DemoLevel
	{
		RTTI_DECLARATIONS(TerrainDemo, DrawableGameComponent)

	public:
		TerrainDemo(Game& game, Camera& camera, Editor& editor);
		~TerrainDemo();

		virtual void Create() override;
		virtual void Destroy() override;
		virtual bool IsComponent() override;
		virtual void UpdateLevel(const GameTime& gameTime) override;
		virtual void DrawLevel(const GameTime& gameTime) override;

	private:
		TerrainDemo();
		TerrainDemo(const TerrainDemo& rhs);
		TerrainDemo& operator=(const TerrainDemo& rhs);

		void UpdateStandardLightingPBRMaterialVariables(const std::string& objectName, int meshIndex);
		void UpdateDeferredPrepassMaterialVariables(const std::string& objectName, int meshIndex);
		void UpdateShadow0MaterialVariables(const std::string & objectName, int meshIndex);
		void UpdateShadow1MaterialVariables(const std::string & objectName, int meshIndex);
		void UpdateShadow2MaterialVariables(const std::string & objectName, int meshIndex);
		void DistributeVegetationZonesPositionsAcrossTerrainGrid(RenderingObject* object, int count);
		void DistributeAcrossVegetationZones(std::vector<InstancedData>& data, int count, float radius, XMFLOAT3 center, float minScale, float maxScale);
		void PlaceFoliageOnTerrainTile(int tileIndex);
		void PlaceInstanceObjectOnTerrain(RenderingObject* object, int tileIndex, int numInstancesPerZone, int splatChannel = -1);
		void PlaceObjectsOnTerrain(int tileIndex, XMFLOAT4* objectsPositions, int objectsCount, XMFLOAT4* terrainVertices, int terrainVertexCount, int splatChannel = -1);
		void UpdateImGui();
		void Initialize();
		void GenerateFoliageInVegetationZones(int zonesCount);
		void GenerateObjectsInVegetationZones(int zonesCount, std::string nameInScene);

		XMFLOAT4X4 mWorldMatrix;
		RenderStateHelper* mRenderStateHelper;
		Skybox* mSkybox;
		Grid* mGrid;
		ShadowMapper* mShadowMapper;
		DirectionalLight* mDirectionalLight;
		GBuffer* mGBuffer;
		PostProcessingStack* mPostProcessingStack;
		Terrain* mTerrain;
		VolumetricClouds* mVolumetricClouds;
		Scene* mScene;
		std::vector<std::map<std::string, Foliage*>> mFoliageZonesCollections;
		std::vector<XMFLOAT3> mVegetationZonesCenters;

		ID3D11ShaderResourceView* mIrradianceDiffuseTextureSRV;
		ID3D11ShaderResourceView* mIrradianceSpecularTextureSRV;
		ID3D11ShaderResourceView* mIntegrationMapTextureSRV;
		std::unique_ptr<IBLRadianceMap> mIBLRadianceMap;

		int mVegetationZonesCount = 64;
		float mFoliageZoneGizmoSphereScale = 10.0f;
		float mFoliageDynamicLODToCameraDistance = 650.0f;

		bool mRenderFoliage = true;
		bool mRenderVegetationZonesCenters = false;

		float mWindStrength = 1.0f;
		float mWindFrequency = 1.0f;
		float mWindGustDistance = 1.0f;
	};
}