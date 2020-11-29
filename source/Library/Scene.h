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
	private:
		Json::Value root;

		std::tuple<Material*, Effect*, std::string> GetMaterialData(std::string materialName, std::string effectName, std::string techniqueName);
		Camera& mCamera;
		std::string mScenePath;
	};
}