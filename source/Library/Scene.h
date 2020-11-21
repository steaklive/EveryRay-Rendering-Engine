#pragma once
#include "Common.h"
#include "GameComponent.h"
#include "Camera.h"
#include "Material.h"
#include "ModelMaterial.h"
#include "Effect.h"

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

		std::map<std::string, Rendering::RenderingObject*> objects;
	private:
		std::tuple<Material*, Effect*, std::string> GetMaterialData(std::string materialName, std::string effectName, std::string techniqueName);
		Camera& mCamera;
	};
}