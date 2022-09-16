#include "stdafx.h"
#include <cstdlib>
#include <stdio.h>
#include <fstream>
#include <iostream>

#include "ER_Scene.h"
#include "ER_Core.h"
#include "ER_CoreException.h"
#include "ER_Utility.h"
#include "ER_Model.h"
#include "ER_Materials.inl"
#include "ER_RenderingObject.h"
#include "ER_MaterialHelper.h"
#include "ER_Illumination.h"
#include "ER_LightProbesManager.h"
#include "ER_FoliageManager.h"
#include "ER_DirectionalLight.h"
#include "ER_Terrain.h"

#if defined(DEBUG) || defined(_DEBUG)  
	#define MULTITHREADED_SCENE_LOAD 0
#else
	#define MULTITHREADED_SCENE_LOAD 1
#endif

namespace EveryRay_Core 
{
	ER_Scene::ER_Scene(ER_Core& pCore, ER_Camera& pCamera, const std::string& path) :
		ER_CoreComponent(pCore), mCamera(pCamera), mScenePath(path)
	{
		Json::Reader reader;
		std::ifstream scene(path.c_str(), std::ifstream::binary);

		if (!reader.parse(scene, root)) {
			throw ER_CoreException(reader.getFormattedErrorMessages().c_str());
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

			// terrain config
			{
				if (root.isMember("terrain_num_tiles")) {
					mHasTerrain = true;
					mTerrainTilesCount = root["terrain_num_tiles"].asInt();
					if (root.isMember("terrain_tile_scale"))
						mTerrainTileScale = root["terrain_tile_scale"].asFloat();
					if (root.isMember("terrain_tile_resolution"))
						mTerrainTileResolution = root["terrain_tile_resolution"].asInt();
					std::string fieldName = "terrain_texture_splat_layer";
					std::wstring result = L"";
					for (int i = 0; i < 4; i ++)
					{
						fieldName += std::to_string(i);
						if (root.isMember(fieldName.c_str()))
							result = ER_Utility::ToWideString(root[fieldName.c_str()].asString());
						else
							result = L"";

						fieldName = "terrain_texture_splat_layer";
						mTerrainSplatLayersTextureNames[i] = result;
					}
				}
				else
					mHasTerrain = false;
			}

			// light probes config
			{
				if (root.isMember("light_probes_volume_bounds_min")) {
					float vec3[3];
					for (Json::Value::ArrayIndex i = 0; i != root["light_probes_volume_bounds_min"].size(); i++)
						vec3[i] = root["light_probes_volume_bounds_min"][i].asFloat();

					mLightProbesVolumeMinBounds = XMFLOAT3(vec3[0], vec3[1], vec3[2]);
				}
				else
					mHasLightProbes = false;

				if (root.isMember("light_probes_volume_bounds_max")) {
					float vec3[3];
					for (Json::Value::ArrayIndex i = 0; i != root["light_probes_volume_bounds_max"].size(); i++)
						vec3[i] = root["light_probes_volume_bounds_max"][i].asFloat();

					mLightProbesVolumeMaxBounds = XMFLOAT3(vec3[0], vec3[1], vec3[2]);
				}
				else
					mHasLightProbes = false;

				if (root.isMember("light_probes_diffuse_distance"))
					mLightProbesDiffuseDistance = root["light_probes_diffuse_distance"].asFloat();
				if (root.isMember("light_probes_specular_distance"))
					mLightProbesSpecularDistance = root["light_probes_specular_distance"].asFloat();
			}

			if (root.isMember("foliage_zones"))
				mHasFoliage = true;

			if (root.isMember("use_volumetric_fog")) {
				mHasVolumetricFog = root["use_volumetric_fog"].asBool();
			}
			else
				mHasVolumetricFog = false;

			// add rendering objects to scene
			unsigned int numRenderingObjects = root["rendering_objects"].size();
			for (Json::Value::ArrayIndex i = 0; i != numRenderingObjects; i++) {
				std::string name = root["rendering_objects"][i]["name"].asString();
				std::string modelPath = root["rendering_objects"][i]["model_path"].asString();
				bool isInstanced = root["rendering_objects"][i]["instanced"].asBool();
				objects.emplace(name, new ER_RenderingObject(name, i, *mCore, mCamera, std::unique_ptr<ER_Model>(new ER_Model(*mCore, ER_Utility::GetFilePath(modelPath), true)), true, isInstanced));
			}

			assert(numRenderingObjects == objects.size());

#if MULTITHREADED_SCENE_LOAD
			int numThreads = std::thread::hardware_concurrency();
#else
			int numThreads = 1;
#endif
			int objectsPerThread = numRenderingObjects / numThreads;
			if (objectsPerThread == 0)
			{
				numThreads = 1;
				objectsPerThread = numRenderingObjects;
			}

			std::vector<std::thread> threads;
			threads.reserve(numThreads);
			
			for (int i = 0; i < numThreads; i++)
			{
				threads.push_back(std::thread([&, numThreads, numRenderingObjects, objectsPerThread, i]
				{
					int endRange = (i < numThreads - 1) ? (i + 1) * objectsPerThread : numRenderingObjects;
					
					for (int j = i * objectsPerThread; j < endRange; j++)
					{
						auto objectI = objects.begin();
						std::advance(objectI, j);
						LoadRenderingObjectData(objectI->second);
					}
				}));
			}
			for (auto& t : threads) t.join();
			
			for (auto& obj : objects)
				LoadRenderingObjectInstancedData(obj.second);
		}
	}

	ER_Scene::~ER_Scene()
	{
		for (auto object : objects)
		{
			object.second->MeshMaterialVariablesUpdateEvent->RemoveAllListeners();
			DeleteObject(object.second);
		}
		objects.clear();

		for (auto& rs : mStandardMaterialsRootSignatures)
		{
			DeleteObject(rs.second);
		}
		mStandardMaterialsRootSignatures.clear();
	}

	void ER_Scene::LoadRenderingObjectData(ER_RenderingObject* aObject)
	{
		if (!aObject)
			return;

		int i = aObject->GetIndexInScene();
		bool isInstanced = aObject->IsInstanced();
		bool hasLODs = false;

		// load flags
		{
			if (root["rendering_objects"][i].isMember("foliageMask"))
				aObject->SetFoliageMask(root["rendering_objects"][i]["foliageMask"].asBool());
			
			if (root["rendering_objects"][i].isMember("use_indirect_global_lightprobe"))
				aObject->SetUseIndirectGlobalLightProbe(root["rendering_objects"][i]["use_indirect_global_lightprobe"].asBool());
			
			if (root["rendering_objects"][i].isMember("use_in_global_lightprobe_rendering"))
				aObject->SetIsUsedForGlobalLightProbeRendering(root["rendering_objects"][i]["use_in_global_lightprobe_rendering"].asBool());
			
			if (root["rendering_objects"][i].isMember("use_parallax_occlusion_mapping"))
				aObject->SetParallaxOcclusionMapping(root["rendering_objects"][i]["use_parallax_occlusion_mapping"].asBool());
			
			if (root["rendering_objects"][i].isMember("use_forward_shading"))
				aObject->SetForwardShading(root["rendering_objects"][i]["use_forward_shading"].asBool());

			if (root["rendering_objects"][i].isMember("use_sss"))
				aObject->SetSeparableSubsurfaceScattering(root["rendering_objects"][i]["use_sss"].asBool());
			
			if (root["rendering_objects"][i].isMember("use_custom_alpha_discard"))
				aObject->SetCustomAlphaDiscard(root["rendering_objects"][i]["use_custom_alpha_discard"].asFloat());

			//terrain
			if (root["rendering_objects"][i].isMember("terrain_placement"))
			{
				aObject->SetTerrainPlacement(root["rendering_objects"][i]["terrain_placement"].asBool());

				if (root["rendering_objects"][i].isMember("terrain_splat_channel"))
					aObject->SetTerrainProceduralPlacementSplatChannel(root["rendering_objects"][i]["terrain_splat_channel"].asInt());

				//procedural flags
				{
					if (root["rendering_objects"][i].isMember("terrain_procedural_instance_scale_min") && root["rendering_objects"][i].isMember("terrain_procedural_instance_scale_max"))
						aObject->SetTerrainProceduralObjectsMinMaxScale(
							root["rendering_objects"][i]["terrain_procedural_instance_scale_min"].asFloat(),
							root["rendering_objects"][i]["terrain_procedural_instance_scale_max"].asFloat());

					if (root["rendering_objects"][i].isMember("terrain_procedural_instance_pitch_min") && root["rendering_objects"][i].isMember("terrain_procedural_instance_pitch_max"))
						aObject->SetTerrainProceduralObjectsMinMaxPitch(
							root["rendering_objects"][i]["terrain_procedural_instance_pitch_min"].asFloat(),
							root["rendering_objects"][i]["terrain_procedural_instance_pitch_max"].asFloat());

					if (root["rendering_objects"][i].isMember("terrain_procedural_instance_roll_min") && root["rendering_objects"][i].isMember("terrain_procedural_instance_roll_max"))
						aObject->SetTerrainProceduralObjectsMinMaxRoll(
							root["rendering_objects"][i]["terrain_procedural_instance_roll_min"].asFloat(),
							root["rendering_objects"][i]["terrain_procedural_instance_roll_max"].asFloat());

					if (root["rendering_objects"][i].isMember("terrain_procedural_instance_yaw_min") && root["rendering_objects"][i].isMember("terrain_procedural_instance_yaw_max"))
						aObject->SetTerrainProceduralObjectsMinMaxYaw(
							root["rendering_objects"][i]["terrain_procedural_instance_yaw_min"].asFloat(),
							root["rendering_objects"][i]["terrain_procedural_instance_yaw_max"].asFloat());

					if (isInstanced && root["rendering_objects"][i].isMember("terrain_procedural_instance_count"))
						aObject->SetTerrainProceduralInstanceCount(root["rendering_objects"][i]["terrain_procedural_instance_count"].asInt());

					if (root["rendering_objects"][i].isMember("terrain_procedural_zone_center_pos"))
					{
						float vec3[3];
						for (Json::Value::ArrayIndex vecI = 0; vecI != root["rendering_objects"][i]["terrain_procedural_zone_center_pos"].size(); vecI++)
							vec3[vecI] = root["rendering_objects"][i]["terrain_procedural_zone_center_pos"][vecI].asFloat();

						XMFLOAT3 centerPos = XMFLOAT3(vec3[0], vec3[1], vec3[2]);
						aObject->SetTerrainProceduralZoneCenterPos(centerPos);
					}

					if (isInstanced && root["rendering_objects"][i].isMember("terrain_procedural_zone_radius"))
						aObject->SetTerrainProceduralZoneRadius(root["rendering_objects"][i]["terrain_procedural_zone_radius"].asFloat());
				}
			}
			
			if (root["rendering_objects"][i].isMember("min_scale"))
				aObject->SetMinScale(root["rendering_objects"][i]["min_scale"].asFloat());
			
			if (root["rendering_objects"][i].isMember("max_scale"))
				aObject->SetMaxScale(root["rendering_objects"][i]["max_scale"].asFloat());
		}

		// load materials
		{
			if (root["rendering_objects"][i].isMember("new_materials")) {
				unsigned int numMaterials = root["rendering_objects"][i]["new_materials"].size();
				for (Json::Value::ArrayIndex matIndex = 0; matIndex != numMaterials; matIndex++) {
					std::string name = root["rendering_objects"][i]["new_materials"][matIndex]["name"].asString();

					MaterialShaderEntries shaderEntries;
					if (root["rendering_objects"][i]["new_materials"][matIndex].isMember("vertexEntry"))
						shaderEntries.vertexEntry = root["rendering_objects"][i]["new_materials"][matIndex]["vertexEntry"].asString();
					if (root["rendering_objects"][i]["new_materials"][matIndex].isMember("geometryEntry"))
						shaderEntries.geometryEntry = root["rendering_objects"][i]["new_materials"][matIndex]["geometryEntry"].asString();
					if (root["rendering_objects"][i]["new_materials"][matIndex].isMember("hullEntry"))
						shaderEntries.hullEntry = root["rendering_objects"][i]["new_materials"][matIndex]["hullEntry"].asString();	
					if (root["rendering_objects"][i]["new_materials"][matIndex].isMember("domainEntry"))
						shaderEntries.domainEntry = root["rendering_objects"][i]["new_materials"][matIndex]["domainEntry"].asString();	
					if (root["rendering_objects"][i]["new_materials"][matIndex].isMember("pixelEntry"))
						shaderEntries.pixelEntry = root["rendering_objects"][i]["new_materials"][matIndex]["pixelEntry"].asString();

					if (isInstanced) //be careful with the instancing support in shaders of the materials! (i.e., maybe the material does not have instancing entry point/support)
						shaderEntries.vertexEntry = shaderEntries.vertexEntry + "_instancing";
					
					if (name == ER_MaterialHelper::gbufferMaterialName)
						aObject->SetInGBuffer(true);
					if (name == ER_MaterialHelper::renderToLightProbeMaterialName)
						aObject->SetInLightProbe(true);

					if (name == ER_MaterialHelper::shadowMapMaterialName)
					{
						for (int cascade = 0; cascade < NUM_SHADOW_CASCADES; cascade++)
						{
							std::string cascadedname = ER_MaterialHelper::shadowMapMaterialName + " " + std::to_string(cascade);
							aObject->LoadMaterial(GetMaterialByName(name, shaderEntries, isInstanced), cascadedname);
						}
					}
					else if (name == ER_MaterialHelper::voxelizationMaterialName)
					{
						aObject->SetInVoxelization(true);
						for (int cascade = 0; cascade < NUM_VOXEL_GI_CASCADES; cascade++)
						{
							const std::string fullname = ER_MaterialHelper::voxelizationMaterialName + "_" + std::to_string(cascade);
							aObject->LoadMaterial(GetMaterialByName(name, shaderEntries, isInstanced), fullname);
						}
					}
					else if (name == ER_MaterialHelper::renderToLightProbeMaterialName)
					{
						std::string originalPSEntry = shaderEntries.pixelEntry;
						for (int cubemapFaceIndex = 0; cubemapFaceIndex < CUBEMAP_FACES_COUNT; cubemapFaceIndex++)
						{
							std::string newName;
							//diffuse
							{
								shaderEntries.pixelEntry = originalPSEntry + "_DiffuseProbes";
								newName = "diffuse_" + ER_MaterialHelper::renderToLightProbeMaterialName + "_" + std::to_string(cubemapFaceIndex);
								aObject->LoadMaterial(GetMaterialByName(name, shaderEntries, isInstanced), newName);
							}
							//specular
							{
								shaderEntries.pixelEntry = originalPSEntry + "_SpecularProbes";
								newName = "specular_" + ER_MaterialHelper::renderToLightProbeMaterialName + "_" + std::to_string(cubemapFaceIndex);
								aObject->LoadMaterial(GetMaterialByName(name, shaderEntries, isInstanced), newName);
							}
						}
					}
					else //other standard materials
						aObject->LoadMaterial(GetMaterialByName(name, shaderEntries, isInstanced), name);
				}
			}

			aObject->LoadRenderBuffers();
		}

		// load custom textures
		{
			if (root["rendering_objects"][i].isMember("textures")) {

				for (Json::Value::ArrayIndex mesh = 0; mesh != root["rendering_objects"][i]["textures"].size(); mesh++) {
					if (root["rendering_objects"][i]["textures"][mesh].isMember("albedo"))
						aObject->mCustomAlbedoTextures[mesh] = root["rendering_objects"][i]["textures"][mesh]["albedo"].asString();
					if (root["rendering_objects"][i]["textures"][mesh].isMember("normal"))
						aObject->mCustomNormalTextures[mesh] = root["rendering_objects"][i]["textures"][mesh]["normal"].asString();
					if (root["rendering_objects"][i]["textures"][mesh].isMember("roughness"))
						aObject->mCustomRoughnessTextures[mesh] = root["rendering_objects"][i]["textures"][mesh]["roughness"].asString();
					if (root["rendering_objects"][i]["textures"][mesh].isMember("metalness"))
						aObject->mCustomMetalnessTextures[mesh] = root["rendering_objects"][i]["textures"][mesh]["metalness"].asString();
					if (root["rendering_objects"][i]["textures"][mesh].isMember("height"))
						aObject->mCustomHeightTextures[mesh] = root["rendering_objects"][i]["textures"][mesh]["height"].asString();
					if (root["rendering_objects"][i]["textures"][mesh].isMember("reflection_mask"))
						aObject->mCustomReflectionMaskTextures[mesh] = root["rendering_objects"][i]["textures"][mesh]["reflection_mask"].asString();

					aObject->LoadCustomMeshTextures(mesh);
				}
			}
		}

		// load world transform
		{
			if (root["rendering_objects"][i].isMember("transform")) {
				if (root["rendering_objects"][i]["transform"].size() != 16)
				{
					aObject->SetTransformationMatrix(XMMatrixIdentity());
				}
				else {
					float matrix[16];
					for (Json::Value::ArrayIndex matC = 0; matC != root["rendering_objects"][i]["transform"].size(); matC++) {
						matrix[matC] = root["rendering_objects"][i]["transform"][matC].asFloat();
					}
					XMFLOAT4X4 worldTransform(matrix);
					aObject->SetTransformationMatrix(XMMatrixTranspose(XMLoadFloat4x4(&worldTransform)));
				}
			}
			else
				aObject->SetTransformationMatrix(XMMatrixIdentity());
		}

		// load lods
		{
			hasLODs = root["rendering_objects"][i].isMember("model_lods");
			if (hasLODs) {
				for (Json::Value::ArrayIndex lod = 1 /* 0 is main model loaded before */; lod != root["rendering_objects"][i]["model_lods"].size(); lod++) {
					std::string path = root["rendering_objects"][i]["model_lods"][lod]["path"].asString();
					aObject->LoadLOD(std::unique_ptr<ER_Model>(new ER_Model(*mCore, ER_Utility::GetFilePath(path), true)));
				}
			}
		}
	}

	// [WARNING] NOT THREAD-SAFE!
	void ER_Scene::LoadRenderingObjectInstancedData(ER_RenderingObject* aObject)
	{
		int i = aObject->GetIndexInScene();
		bool isInstanced = aObject->IsInstanced();
		bool hasLODs = root["rendering_objects"][i].isMember("model_lods");
		if (isInstanced) {
			if (hasLODs)
			{
				for (int lod = 0; lod < root["rendering_objects"][i]["model_lods"].size(); lod++)
				{
					aObject->LoadInstanceBuffers(lod);
					if (aObject->GetTerrainPlacement() && aObject->GetTerrainProceduralInstanceCount() > 0)
					{
						int instanceCount = aObject->GetTerrainProceduralInstanceCount();
						aObject->ResetInstanceData(instanceCount, true, lod);
						for (int i = 0; i < instanceCount; i++)
							aObject->AddInstanceData(XMMatrixIdentity(), lod);
					}
					else
					{
						if (root["rendering_objects"][i].isMember("instances_transforms")) {
							aObject->ResetInstanceData(root["rendering_objects"][i]["instances_transforms"].size(), true, lod);
							for (Json::Value::ArrayIndex instance = 0; instance != root["rendering_objects"][i]["instances_transforms"].size(); instance++) {
								float matrix[16];
								for (Json::Value::ArrayIndex matC = 0; matC != root["rendering_objects"][i]["instances_transforms"][instance]["transform"].size(); matC++) {
									matrix[matC] = root["rendering_objects"][i]["instances_transforms"][instance]["transform"][matC].asFloat();
								}
								XMFLOAT4X4 worldTransform(matrix);
								aObject->AddInstanceData(XMMatrixTranspose(XMLoadFloat4x4(&worldTransform)), lod);
							}
						}
						else {
							aObject->ResetInstanceData(1, true, lod);
							aObject->AddInstanceData(aObject->GetTransformationMatrix(), lod);
						}
					}
					aObject->UpdateInstanceBuffer(aObject->GetInstancesData(), lod);
				}
			}
			else {
				aObject->LoadInstanceBuffers();

				if (aObject->GetTerrainPlacement() && aObject->GetTerrainProceduralInstanceCount() > 0)
				{
					int instanceCount = aObject->GetTerrainProceduralInstanceCount();
					aObject->ResetInstanceData(instanceCount, true);
					for (int i = 0; i < instanceCount; i++)
						aObject->AddInstanceData(XMMatrixIdentity());
				}
				else
				{
					if (root["rendering_objects"][i].isMember("instances_transforms")) {
						aObject->ResetInstanceData(root["rendering_objects"][i]["instances_transforms"].size(), true);
						for (Json::Value::ArrayIndex instance = 0; instance != root["rendering_objects"][i]["instances_transforms"].size(); instance++) {
							float matrix[16];
							for (Json::Value::ArrayIndex matC = 0; matC != root["rendering_objects"][i]["instances_transforms"][instance]["transform"].size(); matC++) {
								matrix[matC] = root["rendering_objects"][i]["instances_transforms"][instance]["transform"][matC].asFloat();
							}
							XMFLOAT4X4 worldTransform(matrix);
							aObject->AddInstanceData(XMMatrixTranspose(XMLoadFloat4x4(&worldTransform)));
						}
					}
					else {
						aObject->ResetInstanceData(1, true);
						aObject->AddInstanceData(aObject->GetTransformationMatrix());
					}
				}
				aObject->UpdateInstanceBuffer(aObject->GetInstancesData());
			}
		}
	}

	void ER_Scene::SaveFoliageZonesTransforms(const std::vector<ER_Foliage*>& foliageZones)
	{
		if (mScenePath.empty())
			throw ER_CoreException("Can't save to scene json file! Empty scene name...");

		if (root.isMember("foliage_zones")) {
			assert(foliageZones.size() == root["foliage_zones"].size());
			float vec3[3];
			for (Json::Value::ArrayIndex iz = 0; iz != root["foliage_zones"].size(); iz++)
			{
				Json::Value content(Json::arrayValue);
				vec3[0] = foliageZones[iz]->GetDistributionCenter().x;
				vec3[1] = foliageZones[iz]->GetDistributionCenter().y;
				vec3[2] = foliageZones[iz]->GetDistributionCenter().z;
				for (Json::Value::ArrayIndex i = 0; i < 3; i++)
					content.append(vec3[i]);
				root["foliage_zones"][iz]["position"] = content;
			}
		}

		Json::StreamWriterBuilder builder;
		std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());

		std::ofstream file_id;
		file_id.open(mScenePath.c_str());
		writer->write(root, &file_id);
	}

	void ER_Scene::SaveRenderingObjectsTransforms()
	{
		if (mScenePath.empty())
			throw ER_CoreException("Can't save to scene json file! Empty scene name...");

		// store world transform
		for (Json::Value::ArrayIndex i = 0; i != root["rendering_objects"].size(); i++) {
			Json::Value content(Json::arrayValue);
			if (root["rendering_objects"][i].isMember("transform")) {
				XMFLOAT4X4 mat = objects[root["rendering_objects"][i]["name"].asString()]->GetTransformationMatrix4X4();
				XMMATRIX matXM  = XMMatrixTranspose(XMLoadFloat4x4(&mat));
				XMStoreFloat4x4(&mat, matXM);
				float matF[16];
				ER_MatrixHelper::GetFloatArray(mat, matF);
				for (int i = 0; i < 16; i++)
					content.append(matF[i]);
				root["rendering_objects"][i]["transform"] = content;
			}
			else
			{
				float matF[16];
				XMFLOAT4X4 mat;
				XMStoreFloat4x4(&mat, XMMatrixTranspose(XMMatrixIdentity()));
				ER_MatrixHelper::GetFloatArray(mat, matF);

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
							throw ER_CoreException("Can't save instances transforms to scene json file! RenderObject's instance count is not equal to the number of instance transforms in scene file.");

						for (Json::Value::ArrayIndex instance = 0; instance != root["rendering_objects"][i]["instances_transforms"].size(); instance++) {
							Json::Value contentInstanceTransform(Json::arrayValue);

							XMFLOAT4X4 mat = objects[root["rendering_objects"][i]["name"].asString()]->GetInstancesData()[instance].World;
							XMMATRIX matXM = XMMatrixTranspose(XMLoadFloat4x4(&mat));
							XMStoreFloat4x4(&mat, matXM);
							float matF[16];
							ER_MatrixHelper::GetFloatArray(mat, matF);
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

	// We cant do reflection in C++, that is why we check every materials name and create a material out of it (and root-signature if needed)
	ER_Material* ER_Scene::GetMaterialByName(const std::string& matName, const MaterialShaderEntries& entries, bool instanced)
	{
		ER_Core* core = GetCore();
		assert(core);
		ER_RHI* rhi = core->GetRHI();

		ER_Material* material = nullptr;

		if (matName == "BasicColorMaterial")
		{
			material = new ER_BasicColorMaterial(*core, entries, HAS_VERTEX_SHADER | HAS_PIXEL_SHADER /*TODO instanced support*/);
			// This material is standard, that is why we create a root-signature here and send it to callbacks later
			ER_RHI_GPURootSignature* rs = rhi->CreateRootSignature(1, 0);
			if (rs)
			{
				rs->InitDescriptorTable(rhi, BASICCOLOR_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_ALL);
				rs->Finalize(rhi, "BasicColorMaterial Pass Root Signature", true);
			}
			mStandardMaterialsRootSignatures.emplace(matName, rs);
		}
		else if (matName == "ShadowMapMaterial")
			material = new ER_ShadowMapMaterial(*core, entries, HAS_VERTEX_SHADER | HAS_PIXEL_SHADER, instanced);
		else if (matName == "GBufferMaterial")
			material = new ER_GBufferMaterial(*core, entries, HAS_VERTEX_SHADER | HAS_PIXEL_SHADER, instanced);
		else if (matName == "RenderToLightProbeMaterial")
			material = new ER_RenderToLightProbeMaterial(*core, entries, HAS_VERTEX_SHADER | HAS_PIXEL_SHADER, instanced);
		else if (matName == "VoxelizationMaterial")
			material = new ER_VoxelizationMaterial(*core, entries, HAS_VERTEX_SHADER | HAS_GEOMETRY_SHADER | HAS_PIXEL_SHADER, instanced);
		else
			material = nullptr;

		return material;
	}

	void ER_Scene::LoadFoliageZones(std::vector<ER_Foliage*>& foliageZones, ER_DirectionalLight& light)
	{
		Json::Reader reader;
		std::ifstream scene(mScenePath.c_str(), std::ifstream::binary);
		ER_Core* core = GetCore();
		assert(core);

		if (!reader.parse(scene, root)) {
			throw ER_CoreException(reader.getFormattedErrorMessages().c_str());
		}
		else {
			if (root.isMember("foliage_zones")) {
				for (Json::Value::ArrayIndex i = 0; i != root["foliage_zones"].size(); i++)
				{
					float vec3[3];
					for (Json::Value::ArrayIndex ia = 0; ia != root["foliage_zones"][i]["position"].size(); ia++)
						vec3[ia] = root["foliage_zones"][i]["position"][ia].asFloat();

					bool placedOnTerrain = false;
					if (root["foliage_zones"][i].isMember("placed_on_terrain"))
						placedOnTerrain = root["foliage_zones"][i]["placed_on_terrain"].asBool();
					
					TerrainSplatChannels terrainChannel = TerrainSplatChannels::NONE;
					if (root["foliage_zones"][i].isMember("placed_splat_channel"))
						terrainChannel = (TerrainSplatChannels)(root["foliage_zones"][i]["placed_splat_channel"].asInt());

					foliageZones.push_back(new ER_Foliage(*core, mCamera, light,
						root["foliage_zones"][i]["patch_count"].asInt(),
						ER_Utility::GetFilePath(root["foliage_zones"][i]["texture_path"].asString()),
						root["foliage_zones"][i]["average_scale"].asFloat(),
						root["foliage_zones"][i]["distribution_radius"].asFloat(),
						XMFLOAT3(vec3[0], vec3[1], vec3[2]),
						(FoliageBillboardType)root["foliage_zones"][i]["type"].asInt(), placedOnTerrain, terrainChannel));
				}
			}
			else
				mHasFoliage = false;
		}
	}

	ER_RHI_GPURootSignature* ER_Scene::GetStandardMaterialRootSignature(const std::string& materialName)
	{
		auto it = mStandardMaterialsRootSignatures.find(materialName);
		if (it != mStandardMaterialsRootSignatures.end())
		{
			return it->second;
		}
		else
			return nullptr;
	}
	}