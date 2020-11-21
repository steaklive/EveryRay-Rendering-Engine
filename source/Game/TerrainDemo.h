#pragma once

#include "..\Library\DrawableGameComponent.h"
#include "..\Library\DemoLevel.h"
#include "..\Library\Frustum.h"

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
	class FullScreenQuad;
	class GBuffer;
	class ShadowMapper;
	class Terrain;
	class Foliage;
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
		void DistributeFoliageZonesAcrossTerrainGrid(RenderingObject* object, int count);
		void DistributeAcrossTerrainGrid(RenderingObject* object, int count);
		void PlaceFoliageOnTerrainTile(int tileIndex);
		void PlaceObjectsOnTerrain(int tileIndex, XMFLOAT4* objectsPositions, int objectsCount, XMFLOAT4* terrainVertices, int terrainVertexCount, int splatChannel = -1);
		void UpdateImGui();
		void Initialize();
		void GenerateFoliageZones(int count);

		std::map<std::string, RenderingObject*> mRenderingObjects;

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
		std::vector<std::map<std::string, Foliage*>> mFoliageZonesCollections;
		std::vector<XMFLOAT3> mFoliageZonesCenters;

		ID3D11ShaderResourceView* mIrradianceTextureSRV;
		ID3D11ShaderResourceView* mRadianceTextureSRV;
		ID3D11ShaderResourceView* mIntegrationMapTextureSRV;
		std::unique_ptr<IBLRadianceMap> mIBLRadianceMap;

		bool mRenderTerrainWireframe = false;
		bool mRenderTessellatedTerrain = false;
		bool mRenderNonTessellatedTerrain = true;
		int mStaticTessellationFactor = 4;
		float mTessellatedTerrainHeightScale = 328.0f;
		int mTessellationFactorDynamic = 64;
		bool mDynamicTessellation = true;
		float mCameraDistanceFactor = 0.015;

		int mFoliageZonesCount = 64;
		float mFoliageZoneGizmoSphereScale = 10.0f;
		float mFoliageDynamicLODToCameraDistance = 650.0f;

		bool mRenderFoliage = true;
		bool mRenderFoliageZonesCenters = false;

		float mWindStrength = 1.0f;
		float mWindFrequency = 1.0f;
		float mWindGustDistance = 1.0f;
	};
}