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

		ER_Material* GetMaterialByName(const std::string& matName, const MaterialShaderEntries& entries, bool instanced, int layerIndex = -1);
		ER_RHI_GPURootSignature* GetStandardMaterialRootSignature(const std::string& materialName);
		
		void LoadPointLightsData();
		void SavePointLightsData();

		void LoadFoliageZones(std::vector<ER_Foliage*>& foliageZones, ER_DirectionalLight& light);
		void SaveFoliageZonesTransforms(const std::vector<ER_Foliage*>& foliageZones);

		void LoadPostProcessingVolumes();
		void SavePostProcessingVolumes();

		template<typename T>
		T GetValueFromSceneRoot(const std::string& aName);
		bool IsValueInSceneRoot(const std::string& aName);

	private:
		void CreateStandardMaterialsRootSignatures();
		void LoadRenderingObjectData(ER_RenderingObject* aObject);
		void LoadRenderingObjectInstancedData(ER_RenderingObject* aObject);

		void ShowNoValueFoundMessage(const std::string& aName);

		std::map<std::string, ER_RHI_GPURootSignature*> mStandardMaterialsRootSignatures;

		Json::Value mSceneJsonRoot;
		std::string mScenePath;

		ER_Camera& mCamera;
	};
}