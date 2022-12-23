#pragma once
#include "Common.h"
#include "ER_CoreComponent.h"
#include "ER_Camera.h"
#include "ER_ModelMaterial.h"
#include "ER_Material.h"

#include "..\JsonCpp\include\json\json.h"

namespace EveryRay_Core
{
	class ER_RenderingObject;
	class ER_DirectionalLight;
	class ER_Foliage;
	using ER_SceneObject = std::pair<std::string, ER_RenderingObject*>;

	class ER_Scene : public ER_CoreComponent
	{
	public:
		ER_Scene(ER_Core& pCore, ER_Camera& pCamera, const std::string& path);
		~ER_Scene();

		void SaveRenderingObjectsTransforms();
		ER_RenderingObject* FindRenderingObjectByName(const std::string& aName);
		std::vector<ER_SceneObject> objects;

		ER_Material* GetMaterialByName(const std::string& matName, const MaterialShaderEntries& entries, bool instanced);
		ER_RHI_GPURootSignature* GetStandardMaterialRootSignature(const std::string& materialName);
		
		ER_Camera& GetCamera() { return mCamera; }
		const XMFLOAT3& GetCameraPos() { return mCameraPosition; }
		const XMFLOAT3& GetCameraDir() { return mCameraDirection; }

		const XMFLOAT3& GetSunDir() { return mSunDirection; }
		const XMFLOAT3& GetSunColor() { return mSunColor; }

		bool HasLightProbesSupport() { return mHasLightProbes; }
		const XMFLOAT3& GetLightProbesVolumeMinBounds() const { return mLightProbesVolumeMinBounds; }
		const XMFLOAT3& GetLightProbesVolumeMaxBounds() const { return mLightProbesVolumeMaxBounds; }
		float GetLightProbesDiffuseDistance() { return mLightProbesDiffuseDistance; }
		float GetLightProbesSpecularDistance() { return mLightProbesSpecularDistance; }
		const XMFLOAT3& GetGlobalLightProbeCameraPos() { return mGlobalLightProbeCameraPos; }

		bool HasFoliage() { return mHasFoliage; }
		void LoadFoliageZones(std::vector<ER_Foliage*>& foliageZones, ER_DirectionalLight& light);
		void SaveFoliageZonesTransforms(const std::vector<ER_Foliage*>& foliageZones);

		bool HasTerrain() { return mHasTerrain; }
		int GetTerrainTilesCount() { return mTerrainTilesCount; }
		int GetTerrainTileResolution() { return mTerrainTileResolution; }
		float GetTerrainTileScale() { return mTerrainTileScale; }
		const std::wstring& GetTerrainSplatLayerTextureName(int index) { return mTerrainSplatLayersTextureNames[index]; }

		bool HasVolumetricFog() { return mHasVolumetricFog; }
	private:
		void LoadRenderingObjectData(ER_RenderingObject* aObject);
		void LoadRenderingObjectInstancedData(ER_RenderingObject* aObject);

		std::map<std::string, ER_RHI_GPURootSignature*> mStandardMaterialsRootSignatures;

		ER_Camera& mCamera;
		XMFLOAT3 mCameraPosition;
		XMFLOAT3 mCameraDirection;

		XMFLOAT3 mSunDirection; //in degrees
		XMFLOAT3 mSunColor;

		Json::Value mSceneJsonRoot;
		std::string mScenePath;
		
		bool mHasVolumetricFog = false;
		bool mHasFoliage = false;

		bool mHasTerrain = false;
		int mTerrainTilesCount = 0;
		int mTerrainTileResolution = 0;
		float mTerrainTileScale = 1.0f;
		std::wstring mTerrainSplatLayersTextureNames[4];

		bool mHasLightProbes = true;
		XMFLOAT3 mLightProbesVolumeMinBounds = { 0,0,0 };
		XMFLOAT3 mLightProbesVolumeMaxBounds = { 0,0,0 };
		float mLightProbesDiffuseDistance = -1.0f;
		float mLightProbesSpecularDistance = -1.0f;
		XMFLOAT3 mGlobalLightProbeCameraPos = { 0,0,0 };
	};
}