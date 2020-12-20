#include "stdafx.h"
#include <cstdlib>
#include <stdio.h>
#include <fstream>
#include <iostream>

#include "Scene.h"
#include "GameException.h"
#include "Utility.h"
#include "Model.h"
#include "Material.h"
#include "RenderingObject.h"
#include "MaterialHelper.h"
#include "DepthMapMaterial.h"
#include "StandardLightingMaterial.h"
#include "DeferredMaterial.h"
#include "ParallaxMappingTestMaterial.h"

namespace Library 
{
	Scene::Scene(Game& pGame, Camera& pCamera, std::string path) :
		GameComponent(pGame), mCamera(pCamera), mScenePath(path)
	{
		Json::Reader reader;
		std::ifstream scene(path.c_str(), std::ifstream::binary);

		if (!reader.parse(scene, root)) {
			throw GameException(reader.getFormattedErrorMessages().c_str());
		}
		else {

			if (root.isMember("camera_position")) {
				float vec3[3];
				for (Json::Value::ArrayIndex i = 0; i != root["camera_position"].size(); i++)
					vec3[i] = root["camera_position"][i].asFloat();

				cameraPosition = XMFLOAT3(vec3[0], vec3[1], vec3[2]);
			}

			if (root.isMember("camera_direction")) {
				float vec3[3];
				for (Json::Value::ArrayIndex i = 0; i != root["camera_direction"].size(); i++)
					vec3[i] = root["camera_direction"][i].asFloat();

				cameraDirection = XMFLOAT3(vec3[0], vec3[1], vec3[2]);
			}

			if (root.isMember("sun_direction")) {
				float vec3[3];
				for (Json::Value::ArrayIndex i = 0; i != root["sun_direction"].size(); i++)
					vec3[i] = root["sun_direction"][i].asFloat();

				sunDirection = XMFLOAT3(vec3[0], vec3[1], vec3[2]);
			}

			if (root.isMember("sun_color")) {
				float vec3[3];
				for (Json::Value::ArrayIndex i = 0; i != root["sun_color"].size(); i++)
					vec3[i] = root["sun_color"][i].asFloat();

				sunColor = XMFLOAT3(vec3[0], vec3[1], vec3[2]);
			}

			if (root.isMember("ambient_color")) {
				float vec3[3];
				for (Json::Value::ArrayIndex i = 0; i != root["ambient_color"].size(); i++)
					vec3[i] = root["ambient_color"][i].asFloat();

				ambientColor = XMFLOAT3(vec3[0], vec3[1], vec3[2]);
			}

			if (root.isMember("skybox_path")) {
				skyboxPath = root["skybox_path"].asString();
			}

			for (Json::Value::ArrayIndex i = 0; i != root["rendering_objects"].size(); i++) {
				std::string name = root["rendering_objects"][i]["name"].asString();
				std::string modelPath = root["rendering_objects"][i]["model_path"].asString();
				bool isInstanced = root["rendering_objects"][i]["instanced"].asBool();

				objects.insert(
					std::pair<std::string, Rendering::RenderingObject*>(
						name,
						new Rendering::RenderingObject(name, *mGame, mCamera, std::unique_ptr<Model>(new Model(*mGame, Utility::GetFilePath(modelPath), true)), true, isInstanced)
					)
				);

				auto it = objects.end();
				--it;

				if (root["rendering_objects"][i].isMember("placed_on_terrain")) {
					it->second->SetPlacedOnTerrain(root["rendering_objects"][i]["placed_on_terrain"].asBool());

					if (isInstanced && root["rendering_objects"][i].isMember("num_instances_per_vegetation_zone"))
						it->second->SetNumInstancesPerVegetationZone(root["rendering_objects"][i]["num_instances_per_vegetation_zone"].asInt());
				}

				// load materials
				if (root["rendering_objects"][i].isMember("materials")) {

					for (Json::Value::ArrayIndex mat = 0; mat != root["rendering_objects"][i]["materials"].size(); mat++) {
						auto materialData = GetMaterialData(
							root["rendering_objects"][i]["materials"][mat]["name"].asString(),
							root["rendering_objects"][i]["materials"][mat]["effect"].asString(),
							root["rendering_objects"][i]["materials"][mat]["technique"].asString()
						);
						
						if (std::get<2>(materialData) == MaterialHelper::shadowMapMaterialName) {
							for (int cascade = 0; cascade < MAX_NUM_OF_CASCADES; cascade++)
							{
								const std::string name = MaterialHelper::shadowMapMaterialName + " " + std::to_string(cascade);
								it->second->LoadMaterial(new DepthMapMaterial(), std::get<1>(materialData), name);
								std::map<std::string, Material*>::iterator iter = it->second->GetMaterials().find(name);

								if (iter != it->second->GetMaterials().end())
								{
									it->second->GetMaterials()[name]->SetCurrentTechnique(
										it->second->GetMaterials()[name]->GetEffect()->TechniquesByName().at(root["rendering_objects"][i]["materials"][mat]["technique"].asString())
									);
								}
							}
						} 
						else if (std::get<0>(materialData))
						{
							it->second->LoadMaterial(std::get<0>(materialData), std::get<1>(materialData), std::get<2>(materialData));
							it->second->GetMaterials()[std::get<2>(materialData)]->SetCurrentTechnique(
								it->second->GetMaterials()[std::get<2>(materialData)]->GetEffect()->TechniquesByName().at(root["rendering_objects"][i]["materials"][mat]["technique"].asString())
							);
						}
					}
					it->second->LoadRenderBuffers();
				}

				// load world transform
				if (root["rendering_objects"][i].isMember("transform")) {
					if (root["rendering_objects"][i]["transform"].size() != 16)
					{
						it->second->SetTransformationMatrix(XMMatrixIdentity());
					} 
					else {
						float matrix[16];
						for (Json::Value::ArrayIndex matC = 0; matC != root["rendering_objects"][i]["transform"].size(); matC++) {
							matrix[matC] = root["rendering_objects"][i]["transform"][matC].asFloat();
						}
						XMFLOAT4X4 worldTransform(matrix);
						it->second->SetTransformationMatrix(XMMatrixTranspose(XMLoadFloat4x4(&worldTransform)));
					}
				}
				else
					it->second->SetTransformationMatrix(XMMatrixIdentity());

				// load instanced data
				if (isInstanced) {
					it->second->LoadInstanceBuffers();
					if (root["rendering_objects"][i].isMember("instances_transforms")) {
						it->second->ResetInstanceData(root["rendering_objects"][i]["instances_transforms"].size(), true);
						for (Json::Value::ArrayIndex instance = 0; instance != root["rendering_objects"][i]["instances_transforms"].size(); instance++) {
							float matrix[16];
							for (Json::Value::ArrayIndex matC = 0; matC != root["rendering_objects"][i]["instances_transforms"][instance]["transform"].size(); matC++) {
								matrix[matC] = root["rendering_objects"][i]["instances_transforms"][instance]["transform"][matC].asFloat();
							}
							XMFLOAT4X4 worldTransform(matrix);
							it->second->AddInstanceData(XMMatrixTranspose(XMLoadFloat4x4(&worldTransform)));
						}
					}
					else {
						it->second->ResetInstanceData(1, true);
						it->second->AddInstanceData(it->second->GetTransformationMatrix());
					}
					it->second->UpdateInstanceBuffer(it->second->GetInstancesData());
				}
			}
		}
	}

	Scene::~Scene()
	{
		for (auto object : objects)
		{
			object.second->MeshMaterialVariablesUpdateEvent->RemoverAllListeners();
			DeleteObject(object.second);
		}
		objects.clear();
	}

	void Scene::SaveRenderingObjectsTransforms()
	{
		if (mScenePath.empty())
			throw GameException("Can't save to scene json file! Empty scene name...");

		// store world transform
		for (Json::Value::ArrayIndex i = 0; i != root["rendering_objects"].size(); i++) {
			Json::Value content(Json::arrayValue);
			if (root["rendering_objects"][i].isMember("transform")) {
				XMFLOAT4X4 mat = objects[root["rendering_objects"][i]["name"].asString()]->GetTransformationMatrix4X4();
				XMMATRIX matXM  = XMMatrixTranspose(XMLoadFloat4x4(&mat));
				XMStoreFloat4x4(&mat, matXM);
				float matF[16];
				MatrixHelper::GetFloatArray(mat, matF);
				for (int i = 0; i < 16; i++)
					content.append(matF[i]);
				root["rendering_objects"][i]["transform"] = content;
			}
			else
			{
				float matF[16];
				XMFLOAT4X4 mat;
				XMStoreFloat4x4(&mat, XMMatrixTranspose(XMMatrixIdentity()));
				MatrixHelper::GetFloatArray(mat, matF);

				for (int i = 0; i < 16; i++)
					content.append(matF[i]);
				root["rendering_objects"][i]["transform"] = content;
			}

			if (root["rendering_objects"][i].isMember("instanced")) 
			{
				bool isInstanced = root["rendering_objects"][i]["instanced"].asBool();
				if (isInstanced)
				{
					if (root["rendering_objects"][i].isMember("instances_transforms")) {

						if (root["rendering_objects"][i]["instances_transforms"].size() != objects[root["rendering_objects"][i]["name"].asString()]->GetInstanceCount())
							throw GameException("Can't save instances transforms to scene json file! RenderObject's instance count is not equal to the number of instance transforms in scene file.");

						for (Json::Value::ArrayIndex instance = 0; instance != root["rendering_objects"][i]["instances_transforms"].size(); instance++) {
							Json::Value contentInstanceTransform(Json::arrayValue);

							XMFLOAT4X4 mat = objects[root["rendering_objects"][i]["name"].asString()]->GetInstancesData()[instance].World;
							XMMATRIX matXM = XMMatrixTranspose(XMLoadFloat4x4(&mat));
							XMStoreFloat4x4(&mat, matXM);
							float matF[16];
							MatrixHelper::GetFloatArray(mat, matF);
							for (int i = 0; i < 16; i++)
								contentInstanceTransform.append(matF[i]);

							root["rendering_objects"][i]["instances_transforms"][instance]["transform"] = contentInstanceTransform;
						}
					}
				}
			}
		}

		Json::StreamWriterBuilder builder;
		std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());

		std::ofstream file_id;
		file_id.open(mScenePath.c_str());
		writer->write(root, &file_id);
	}

	std::tuple<Material*, Effect*, std::string> Scene::GetMaterialData(std::string matName, std::string effectName, std::string techniqueName) {
		Material* material = nullptr;

		Effect* effect = new Effect(*mGame);
		effect->CompileFromFile(Utility::GetFilePath(Utility::ToWideString(effectName)));

		std::string materialName = "";

		// cant do reflection in C++
		if (matName == "DepthMapMaterial") {
			materialName = MaterialHelper::shadowMapMaterialName;
			//material = new DepthMapMaterial(); //processed later as we need cascades
		}
		else if (matName == "DeferredMaterial") {
			materialName = MaterialHelper::deferredPrepassMaterialName;
			material = new DeferredMaterial();
		}
		else if (matName == "StandardLightingMaterial") {
			materialName = MaterialHelper::lightingMaterialName;
			material = new Rendering::StandardLightingMaterial();
		}
		else if (matName == "ParallaxMappingTestMaterial") {
			materialName = MaterialHelper::parallaxMaterialName;
			material = new Rendering::ParallaxMappingTestMaterial();
		}
		else
			material = nullptr;

		return std::tuple<Material*, Effect*, std::string>(material, effect, materialName);
	}
}