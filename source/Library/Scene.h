#pragma once
#include "Common.h"
#include "GameComponent.h"
#include "Camera.h"
#include "Material.h"
#include "ModelMaterial.h"
#include "Effect.h"

#include "..\JsonCpp\include\json\json.h"

namespace Rendering
{
	class RenderingObject;
}

namespace Library
{
	class Scene : public GameComponent
	{
	public:
		Scene(Game& pGame, Camera& pCamera, std::string path);
		~Scene();

		void SaveRenderingObjectsTransforms();
		std::map<std::string, Rendering::RenderingObject*> objects;
		std::string skyboxPath;
		XMFLOAT3 cameraPosition;
		XMFLOAT3 cameraDirection;
		XMFLOAT3 sunDirection;
		XMFLOAT3 sunColor;
		XMFLOAT3 ambientColor;

		Material* GetMaterial(const std::string& materialName);
	private:
		Json::Value root;

		std::tuple<Material*, Effect*, std::string> CreateMaterialData(const std::string& materialName, const std::string& effectName, const std::string& techniqueName);
		Camera& mCamera;
		std::string mScenePath;
	};
}