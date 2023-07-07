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
		{
			std::wstring msg = L"[ER Logger][ER_Scene] Started loading scene: " + ER_Utility::ToWideString(path) + L". This might take several minutes... \n";
			ER_OUTPUT_LOG(msg.c_str());
		}

		CreateStandardMaterialsRootSignatures();

		Json::Reader reader;
		std::ifstream scene(path.c_str(), std::ifstream::binary);

		if (!reader.parse(scene, mSceneJsonRoot)) {
			throw ER_CoreException(reader.getFormattedErrorMessages().c_str());
		}
		else {

			if (mSceneJsonRoot.isMember("camera_position")) {
				float vec3[3];
				for (Json::Value::ArrayIndex i = 0; i != mSceneJsonRoot["camera_position"].size(); i++)
					vec3[i] = mSceneJsonRoot["camera_position"][i].asFloat();

				mCameraPosition = XMFLOAT3(vec3[0], vec3[1], vec3[2]);
			}

			if (mSceneJsonRoot.isMember("camera_direction")) {
				float vec3[3];
				for (Json::Value::ArrayIndex i = 0; i != mSceneJsonRoot["camera_direction"].size(); i++)
					vec3[i] = mSceneJsonRoot["camera_direction"][i].asFloat();

				mCameraDirection = XMFLOAT3(vec3[0], vec3[1], vec3[2]);
			}

			if (mSceneJsonRoot.isMember("sun_direction")) {
				float vec3[3] = { 0.0, 0.0, 0.0 };
				for (Json::Value::ArrayIndex i = 0; i != mSceneJsonRoot["sun_direction"].size(); i++)
					vec3[i] = mSceneJsonRoot["sun_direction"][i].asFloat();

				mSunDirection = XMFLOAT3(vec3[0], vec3[1], vec3[2]);
			}

			if (mSceneJsonRoot.isMember("sun_color")) {
				float vec3[3];
				for (Json::Value::ArrayIndex i = 0; i != mSceneJsonRoot["sun_color"].size(); i++)
					vec3[i] = mSceneJsonRoot["sun_color"][i].asFloat();

				mSunColor = XMFLOAT3(vec3[0], vec3[1], vec3[2]);
			}

			// terrain config
			{
				if (mSceneJsonRoot.isMember("terrain_num_tiles")) {
					mHasTerrain = true;
					mTerrainTilesCount = mSceneJsonRoot["terrain_num_tiles"].asInt();
					if (mSceneJsonRoot.isMember("terrain_tile_scale"))
						mTerrainTileScale = mSceneJsonRoot["terrain_tile_scale"].asFloat();
					if (mSceneJsonRoot.isMember("terrain_tile_resolution"))
						mTerrainTileResolution = mSceneJsonRoot["terrain_tile_resolution"].asInt();
					std::string fieldName = "terrain_texture_splat_layer";
					std::wstring result = L"";

					for (int i = 0; i < 4; i++)
					{
						fieldName += std::to_string(i);
						if (mSceneJsonRoot.isMember(fieldName.c_str()))
							result = ER_Utility::ToWideString(mSceneJsonRoot[fieldName.c_str()].asString());
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
				if (mSceneJsonRoot.isMember("light_probes_volume_bounds_min")) {
					float vec3[3];
					for (Json::Value::ArrayIndex i = 0; i != mSceneJsonRoot["light_probes_volume_bounds_min"].size(); i++)
						vec3[i] = mSceneJsonRoot["light_probes_volume_bounds_min"][i].asFloat();

					mLightProbesVolumeMinBounds = XMFLOAT3(vec3[0], vec3[1], vec3[2]);
				}
				else
					mHasLightProbes = false;

				if (mSceneJsonRoot.isMember("light_probes_volume_bounds_max")) {
					float vec3[3];
					for (Json::Value::ArrayIndex i = 0; i != mSceneJsonRoot["light_probes_volume_bounds_max"].size(); i++)
						vec3[i] = mSceneJsonRoot["light_probes_volume_bounds_max"][i].asFloat();

					mLightProbesVolumeMaxBounds = XMFLOAT3(vec3[0], vec3[1], vec3[2]);
				}
				else
					mHasLightProbes = false;

				if (mSceneJsonRoot.isMember("light_probes_diffuse_distance"))
					mLightProbesDiffuseDistance = mSceneJsonRoot["light_probes_diffuse_distance"].asFloat();
				if (mSceneJsonRoot.isMember("light_probes_specular_distance"))
					mLightProbesSpecularDistance = mSceneJsonRoot["light_probes_specular_distance"].asFloat();

				if (mSceneJsonRoot.isMember("light_probe_global_cam_position")) {
					float vec3[3];
					for (Json::Value::ArrayIndex i = 0; i != mSceneJsonRoot["light_probe_global_cam_position"].size(); i++)
						vec3[i] = mSceneJsonRoot["light_probe_global_cam_position"][i].asFloat();

					mGlobalLightProbeCameraPos = XMFLOAT3(vec3[0], vec3[1], vec3[2]);
				}
			}

			if (mSceneJsonRoot.isMember("foliage_zones"))
				mHasFoliage = true;

			if (mSceneJsonRoot.isMember("use_volumetric_fog")) {
				mHasVolumetricFog = mSceneJsonRoot["use_volumetric_fog"].asBool();
			}
			else
				mHasVolumetricFog = false;

			// add rendering objects to scene
			unsigned int numRenderingObjects = mSceneJsonRoot["rendering_objects"].size();
			for (Json::Value::ArrayIndex i = 0; i != numRenderingObjects; i++) {
				objects.emplace_back(
					mSceneJsonRoot["rendering_objects"][i]["name"].asString(), 
					new ER_RenderingObject(mSceneJsonRoot["rendering_objects"][i]["name"].asString(), i, *mCore, mCamera, 
						std::unique_ptr<ER_Model>(new ER_Model(*mCore, ER_Utility::GetFilePath(mSceneJsonRoot["rendering_objects"][i]["model_path"].asString()), true)),
						true, mSceneJsonRoot["rendering_objects"][i]["instanced"].asBool())
				);
			}
			std::partition(objects.begin(), objects.end(), [](const ER_SceneObject& obj) {	return obj.second->IsInstanced(); });
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

		{
			std::wstring msg = L"[ER Logger][ER_Scene] Finished loading scene: " + ER_Utility::ToWideString(path) + L" Enjoy! \n";
			ER_OUTPUT_LOG(msg.c_str());
		}
	}

	ER_Scene::~ER_Scene()
	{
		for (auto& object : objects)
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

	void ER_Scene::CreateStandardMaterialsRootSignatures()
	{
		ER_Core* core = GetCore();
		assert(core);
		ER_RHI* rhi = core->GetRHI();

		{
			ER_RHI_GPURootSignature* rs = rhi->CreateRootSignature(1, 0);
			if (rs)
			{
				rs->InitDescriptorTable(rhi, BASICCOLOR_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 1 }, ER_RHI_SHADER_VISIBILITY_ALL);
				rs->Finalize(rhi, "ER_RHI_GPURootSignature: BasicColorMaterial Pass", true);
			}
			mStandardMaterialsRootSignatures.emplace(ER_MaterialHelper::basicColorMaterialName, rs);
		}

		{
			ER_RHI_GPURootSignature* rs = rhi->CreateRootSignature(2, 2);
			if (rs)
			{
				rs->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_PIXEL);
				rs->InitStaticSampler(rhi, 1, ER_RHI_SAMPLER_STATE::ER_SHADOW_SS, ER_RHI_SHADER_VISIBILITY_PIXEL);
				rs->InitDescriptorTable(rhi, SNOW_MAT_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 7 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				rs->InitDescriptorTable(rhi, SNOW_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 2 }, ER_RHI_SHADER_VISIBILITY_ALL);
				rs->Finalize(rhi, "ER_RHI_GPURootSignature: SimpleSnowMaterial Pass", true);
			}
			mStandardMaterialsRootSignatures.emplace(ER_MaterialHelper::snowMaterialName, rs);
		}

		{
			ER_RHI_GPURootSignature* rs = rhi->CreateRootSignature(1, 0);
			if (rs)
			{
				rs->InitDescriptorTable(rhi, FRESNEL_OUTLINE_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 2 }, ER_RHI_SHADER_VISIBILITY_ALL);
				rs->Finalize(rhi, "ER_RHI_GPURootSignature: FresnelOutlineMaterial Pass", true);
			}
			mStandardMaterialsRootSignatures.emplace(ER_MaterialHelper::fresnelOutlineMaterialName, rs);
		}

		{
			ER_RHI_GPURootSignature* rs = rhi->CreateRootSignature(2, 2);
			if (rs)
			{
				rs->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SHADER_VISIBILITY_PIXEL);
				rs->InitStaticSampler(rhi, 1, ER_RHI_SAMPLER_STATE::ER_SHADOW_SS, ER_RHI_SHADER_VISIBILITY_PIXEL);
				rs->InitDescriptorTable(rhi, FUR_SHELL_MAT_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 6 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				rs->InitDescriptorTable(rhi, FUR_SHELL_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 2 }, ER_RHI_SHADER_VISIBILITY_ALL);
				rs->Finalize(rhi, "ER_RHI_GPURootSignature: FurShellMaterial Pass", true);
			}
			mStandardMaterialsRootSignatures.emplace(ER_MaterialHelper::furShellMaterialName, rs);
		}
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
			if (mSceneJsonRoot["rendering_objects"][i].isMember("foliageMask"))
				aObject->SetIsMarkedAsFoliage(mSceneJsonRoot["rendering_objects"][i]["foliageMask"].asBool());
			
			if (mSceneJsonRoot["rendering_objects"][i].isMember("use_indirect_global_lightprobe"))
				aObject->SetUseIndirectGlobalLightProbe(mSceneJsonRoot["rendering_objects"][i]["use_indirect_global_lightprobe"].asBool());
			
			if (mSceneJsonRoot["rendering_objects"][i].isMember("use_in_global_lightprobe_rendering"))
				aObject->SetIsUsedForGlobalLightProbeRendering(mSceneJsonRoot["rendering_objects"][i]["use_in_global_lightprobe_rendering"].asBool());
			
			if (mSceneJsonRoot["rendering_objects"][i].isMember("use_parallax_occlusion_mapping"))
				aObject->SetParallaxOcclusionMapping(mSceneJsonRoot["rendering_objects"][i]["use_parallax_occlusion_mapping"].asBool());
			
			if (mSceneJsonRoot["rendering_objects"][i].isMember("use_forward_shading"))
				aObject->SetForwardShading(mSceneJsonRoot["rendering_objects"][i]["use_forward_shading"].asBool());

			if (mSceneJsonRoot["rendering_objects"][i].isMember("use_reflection"))
				aObject->SetReflective(mSceneJsonRoot["rendering_objects"][i]["use_reflection"].asBool());

			if (mSceneJsonRoot["rendering_objects"][i].isMember("use_sss"))
				aObject->SetSeparableSubsurfaceScattering(mSceneJsonRoot["rendering_objects"][i]["use_sss"].asBool());
			
			if (mSceneJsonRoot["rendering_objects"][i].isMember("use_custom_alpha_discard"))
				aObject->SetCustomAlphaDiscard(mSceneJsonRoot["rendering_objects"][i]["use_custom_alpha_discard"].asFloat());

			if (mSceneJsonRoot["rendering_objects"][i].isMember("use_transparency"))
				aObject->SetTransparency(mSceneJsonRoot["rendering_objects"][i]["use_transparency"].asBool());

			if (mSceneJsonRoot["rendering_objects"][i].isMember("use_indirect_rendering"))
				aObject->SetUseIndirectRendering(mSceneJsonRoot["rendering_objects"][i]["use_indirect_rendering"].asBool());

			if (mSceneJsonRoot["rendering_objects"][i].isMember("skip_indirect_specular"))
				aObject->SetSkipIndirectSpecular(mSceneJsonRoot["rendering_objects"][i]["skip_indirect_specular"].asBool());

			if (mSceneJsonRoot["rendering_objects"][i].isMember("index_of_refraction"))
				aObject->SetIOR(mSceneJsonRoot["rendering_objects"][i]["index_of_refraction"].asFloat());

			if (mSceneJsonRoot["rendering_objects"][i].isMember("custom_roughness"))
				aObject->SetCustomRoughness(mSceneJsonRoot["rendering_objects"][i]["custom_roughness"].asFloat());

			if (mSceneJsonRoot["rendering_objects"][i].isMember("custom_metalness"))
				aObject->SetCustomMetalness(mSceneJsonRoot["rendering_objects"][i]["custom_metalness"].asFloat());

			//fur
			if (mSceneJsonRoot["rendering_objects"][i].isMember("fur_layers_count"))
				aObject->SetFurLayersCount(mSceneJsonRoot["rendering_objects"][i]["fur_layers_count"].asInt());
			if (mSceneJsonRoot["rendering_objects"][i].isMember("fur_color"))
			{
				float vec3[3];
				for (Json::Value::ArrayIndex vecI = 0; vecI != mSceneJsonRoot["rendering_objects"][i]["fur_color"].size(); vecI++)
					vec3[vecI] = mSceneJsonRoot["rendering_objects"][i]["fur_color"][vecI].asFloat();

				aObject->SetFurColor(vec3[0], vec3[1], vec3[2]);
			}
			if (mSceneJsonRoot["rendering_objects"][i].isMember("fur_color_interpolation"))
				aObject->SetFurColorInterpolation(mSceneJsonRoot["rendering_objects"][i]["fur_color_interpolation"].asFloat());
			if (mSceneJsonRoot["rendering_objects"][i].isMember("fur_length"))
				aObject->SetFurLength(mSceneJsonRoot["rendering_objects"][i]["fur_length"].asFloat());
			if (mSceneJsonRoot["rendering_objects"][i].isMember("fur_cutoff"))
				aObject->SetFurCutoff(mSceneJsonRoot["rendering_objects"][i]["fur_cutoff"].asFloat());
			if (mSceneJsonRoot["rendering_objects"][i].isMember("fur_cutoff_end"))
				aObject->SetFurCutoffEnd(mSceneJsonRoot["rendering_objects"][i]["fur_cutoff_end"].asFloat());
			if (mSceneJsonRoot["rendering_objects"][i].isMember("fur_wind_frequency"))
				aObject->SetFurWindFrequency(mSceneJsonRoot["rendering_objects"][i]["fur_wind_frequency"].asFloat());
			if (mSceneJsonRoot["rendering_objects"][i].isMember("fur_gravity_strength"))
				aObject->SetFurGravityStrength(mSceneJsonRoot["rendering_objects"][i]["fur_gravity_strength"].asFloat());
			if (mSceneJsonRoot["rendering_objects"][i].isMember("fur_uv_scale"))
				aObject->SetFurUVScale(mSceneJsonRoot["rendering_objects"][i]["fur_uv_scale"].asFloat());

			//terrain
			if (mSceneJsonRoot["rendering_objects"][i].isMember("terrain_placement"))
			{
				aObject->SetTerrainPlacement(mSceneJsonRoot["rendering_objects"][i]["terrain_placement"].asBool());

				if (mSceneJsonRoot["rendering_objects"][i].isMember("terrain_splat_channel"))
					aObject->SetTerrainProceduralPlacementSplatChannel(mSceneJsonRoot["rendering_objects"][i]["terrain_splat_channel"].asInt());

				//procedural flags
				{
					if (mSceneJsonRoot["rendering_objects"][i].isMember("terrain_procedural_instance_scale_min") && mSceneJsonRoot["rendering_objects"][i].isMember("terrain_procedural_instance_scale_max"))
						aObject->SetTerrainProceduralObjectsMinMaxScale(
							mSceneJsonRoot["rendering_objects"][i]["terrain_procedural_instance_scale_min"].asFloat(),
							mSceneJsonRoot["rendering_objects"][i]["terrain_procedural_instance_scale_max"].asFloat());

					if (mSceneJsonRoot["rendering_objects"][i].isMember("terrain_procedural_instance_pitch_min") && mSceneJsonRoot["rendering_objects"][i].isMember("terrain_procedural_instance_pitch_max"))
						aObject->SetTerrainProceduralObjectsMinMaxPitch(
							mSceneJsonRoot["rendering_objects"][i]["terrain_procedural_instance_pitch_min"].asFloat(),
							mSceneJsonRoot["rendering_objects"][i]["terrain_procedural_instance_pitch_max"].asFloat());

					if (mSceneJsonRoot["rendering_objects"][i].isMember("terrain_procedural_instance_roll_min") && mSceneJsonRoot["rendering_objects"][i].isMember("terrain_procedural_instance_roll_max"))
						aObject->SetTerrainProceduralObjectsMinMaxRoll(
							mSceneJsonRoot["rendering_objects"][i]["terrain_procedural_instance_roll_min"].asFloat(),
							mSceneJsonRoot["rendering_objects"][i]["terrain_procedural_instance_roll_max"].asFloat());

					if (mSceneJsonRoot["rendering_objects"][i].isMember("terrain_procedural_instance_yaw_min") && mSceneJsonRoot["rendering_objects"][i].isMember("terrain_procedural_instance_yaw_max"))
						aObject->SetTerrainProceduralObjectsMinMaxYaw(
							mSceneJsonRoot["rendering_objects"][i]["terrain_procedural_instance_yaw_min"].asFloat(),
							mSceneJsonRoot["rendering_objects"][i]["terrain_procedural_instance_yaw_max"].asFloat());

					if (isInstanced && mSceneJsonRoot["rendering_objects"][i].isMember("terrain_procedural_instance_count"))
						aObject->SetTerrainProceduralInstanceCount(mSceneJsonRoot["rendering_objects"][i]["terrain_procedural_instance_count"].asInt());

					if (mSceneJsonRoot["rendering_objects"][i].isMember("terrain_procedural_zone_center_pos"))
					{
						float vec3[3];
						for (Json::Value::ArrayIndex vecI = 0; vecI != mSceneJsonRoot["rendering_objects"][i]["terrain_procedural_zone_center_pos"].size(); vecI++)
							vec3[vecI] = mSceneJsonRoot["rendering_objects"][i]["terrain_procedural_zone_center_pos"][vecI].asFloat();

						XMFLOAT3 centerPos = XMFLOAT3(vec3[0], vec3[1], vec3[2]);
						aObject->SetTerrainProceduralZoneCenterPos(centerPos);
					}

					if (isInstanced && mSceneJsonRoot["rendering_objects"][i].isMember("terrain_procedural_zone_radius"))
						aObject->SetTerrainProceduralZoneRadius(mSceneJsonRoot["rendering_objects"][i]["terrain_procedural_zone_radius"].asFloat());
				}
			}
			
			if (mSceneJsonRoot["rendering_objects"][i].isMember("min_scale"))
				aObject->SetMinScale(mSceneJsonRoot["rendering_objects"][i]["min_scale"].asFloat());
			
			if (mSceneJsonRoot["rendering_objects"][i].isMember("max_scale"))
				aObject->SetMaxScale(mSceneJsonRoot["rendering_objects"][i]["max_scale"].asFloat());
		}

		// load materials
		{
			if (mSceneJsonRoot["rendering_objects"][i].isMember("new_materials")) {
				unsigned int numMaterials = mSceneJsonRoot["rendering_objects"][i]["new_materials"].size();
				for (Json::Value::ArrayIndex matIndex = 0; matIndex != numMaterials; matIndex++) {
					std::string name = mSceneJsonRoot["rendering_objects"][i]["new_materials"][matIndex]["name"].asString();

					MaterialShaderEntries shaderEntries;
					if (mSceneJsonRoot["rendering_objects"][i]["new_materials"][matIndex].isMember("vertexEntry"))
						shaderEntries.vertexEntry = mSceneJsonRoot["rendering_objects"][i]["new_materials"][matIndex]["vertexEntry"].asString();
					if (mSceneJsonRoot["rendering_objects"][i]["new_materials"][matIndex].isMember("geometryEntry"))
						shaderEntries.geometryEntry = mSceneJsonRoot["rendering_objects"][i]["new_materials"][matIndex]["geometryEntry"].asString();
					if (mSceneJsonRoot["rendering_objects"][i]["new_materials"][matIndex].isMember("hullEntry"))
						shaderEntries.hullEntry = mSceneJsonRoot["rendering_objects"][i]["new_materials"][matIndex]["hullEntry"].asString();	
					if (mSceneJsonRoot["rendering_objects"][i]["new_materials"][matIndex].isMember("domainEntry"))
						shaderEntries.domainEntry = mSceneJsonRoot["rendering_objects"][i]["new_materials"][matIndex]["domainEntry"].asString();	
					if (mSceneJsonRoot["rendering_objects"][i]["new_materials"][matIndex].isMember("pixelEntry"))
						shaderEntries.pixelEntry = mSceneJsonRoot["rendering_objects"][i]["new_materials"][matIndex]["pixelEntry"].asString();

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
					else if (name == ER_MaterialHelper::furShellMaterialName)
					{
						ER_RHI_GPURootSignature* rs = mStandardMaterialsRootSignatures.at(name);
						int layerCount = aObject->GetFurLayersCount();
						if (layerCount > 0)
						{
							for (int layer = 0; layer < layerCount; layer++)
							{
								const std::string fullname = ER_MaterialHelper::furShellMaterialName + "_" + std::to_string(layer);
								aObject->LoadMaterial(GetMaterialByName(name, shaderEntries, isInstanced, layer), fullname);
								if (rs)
									mStandardMaterialsRootSignatures.emplace(fullname, rs);
							}
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

		// load extra materials data
		if (mSceneJsonRoot["rendering_objects"][i].isMember("snow_albedo"))
			aObject->mSnowAlbedoTexturePath = mSceneJsonRoot["rendering_objects"][i]["snow_albedo"].asString();
		if (mSceneJsonRoot["rendering_objects"][i].isMember("snow_normal"))
			aObject->mSnowNormalTexturePath = mSceneJsonRoot["rendering_objects"][i]["snow_normal"].asString();
		if (mSceneJsonRoot["rendering_objects"][i].isMember("snow_roughness"))
			aObject->mSnowRoughnessTexturePath = mSceneJsonRoot["rendering_objects"][i]["snow_roughness"].asString();
		
		if (mSceneJsonRoot["rendering_objects"][i].isMember("fresnel_outline_color"))
		{
			float vec3[3];
			for (Json::Value::ArrayIndex vecI = 0; vecI != mSceneJsonRoot["rendering_objects"][i]["fresnel_outline_color"].size(); vecI++)
				vec3[vecI] = mSceneJsonRoot["rendering_objects"][i]["fresnel_outline_color"][vecI].asFloat();

			XMFLOAT3 color = XMFLOAT3(vec3[0], vec3[1], vec3[2]);
			aObject->SetFresnelOutlineColor(color);
		}

		if (mSceneJsonRoot["rendering_objects"][i].isMember("fur_height"))
			aObject->mFurHeightTexturePath = mSceneJsonRoot["rendering_objects"][i]["fur_height"].asString();

		// load custom textures
		{
			if (mSceneJsonRoot["rendering_objects"][i].isMember("textures")) {

				for (Json::Value::ArrayIndex mesh = 0; mesh != mSceneJsonRoot["rendering_objects"][i]["textures"].size(); mesh++) {
					if (mSceneJsonRoot["rendering_objects"][i]["textures"][mesh].isMember("albedo"))
						aObject->mCustomAlbedoTextures[mesh] = mSceneJsonRoot["rendering_objects"][i]["textures"][mesh]["albedo"].asString();
					if (mSceneJsonRoot["rendering_objects"][i]["textures"][mesh].isMember("normal"))
						aObject->mCustomNormalTextures[mesh] = mSceneJsonRoot["rendering_objects"][i]["textures"][mesh]["normal"].asString();
					if (mSceneJsonRoot["rendering_objects"][i]["textures"][mesh].isMember("roughness"))
						aObject->mCustomRoughnessTextures[mesh] = mSceneJsonRoot["rendering_objects"][i]["textures"][mesh]["roughness"].asString();
					if (mSceneJsonRoot["rendering_objects"][i]["textures"][mesh].isMember("metalness"))
						aObject->mCustomMetalnessTextures[mesh] = mSceneJsonRoot["rendering_objects"][i]["textures"][mesh]["metalness"].asString();
					if (mSceneJsonRoot["rendering_objects"][i]["textures"][mesh].isMember("height"))
						aObject->mCustomHeightTextures[mesh] = mSceneJsonRoot["rendering_objects"][i]["textures"][mesh]["height"].asString();
					if (mSceneJsonRoot["rendering_objects"][i]["textures"][mesh].isMember("reflection_mask"))
						aObject->mCustomReflectionMaskTextures[mesh] = mSceneJsonRoot["rendering_objects"][i]["textures"][mesh]["reflection_mask"].asString();

					aObject->LoadCustomMeshTextures(mesh);
				}
			}
			aObject->LoadCustomMaterialTextures();
		}

		// load world transform
		{
			if (mSceneJsonRoot["rendering_objects"][i].isMember("transform")) {
				if (mSceneJsonRoot["rendering_objects"][i]["transform"].size() != 16)
				{
					aObject->SetTransformationMatrix(XMMatrixIdentity());
				}
				else {
					float matrix[16];
					for (Json::Value::ArrayIndex matC = 0; matC != mSceneJsonRoot["rendering_objects"][i]["transform"].size(); matC++) {
						matrix[matC] = mSceneJsonRoot["rendering_objects"][i]["transform"][matC].asFloat();
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
			hasLODs = mSceneJsonRoot["rendering_objects"][i].isMember("model_lods");
			if (hasLODs) {
				for (Json::Value::ArrayIndex lod = 1 /* 0 is main model loaded before */; lod != mSceneJsonRoot["rendering_objects"][i]["model_lods"].size(); lod++) {
					std::string path = mSceneJsonRoot["rendering_objects"][i]["model_lods"][lod]["path"].asString();
					aObject->LoadLOD(std::unique_ptr<ER_Model>(new ER_Model(*mCore, ER_Utility::GetFilePath(path), true)));
				}
			}
		}

		std::wstring msg = L"[ER Logger][ER_Scene] Loaded rendering object into scene: " + ER_Utility::ToWideString(aObject->GetName()) + L'\n';
		ER_OUTPUT_LOG(msg.c_str());
	}

	// [WARNING] NOT THREAD-SAFE!
	void ER_Scene::LoadRenderingObjectInstancedData(ER_RenderingObject* aObject)
	{
		int i = aObject->GetIndexInScene();
		bool isInstanced = aObject->IsInstanced();
		if (!isInstanced)
			return;

		bool hasLODs = mSceneJsonRoot["rendering_objects"][i].isMember("model_lods");
		if (hasLODs)
		{
			for (int lod = 0; lod < mSceneJsonRoot["rendering_objects"][i]["model_lods"].size(); lod++)
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
					if (mSceneJsonRoot["rendering_objects"][i].isMember("instances_transforms")) {
						aObject->ResetInstanceData(mSceneJsonRoot["rendering_objects"][i]["instances_transforms"].size(), true, lod);
						for (Json::Value::ArrayIndex instance = 0; instance != mSceneJsonRoot["rendering_objects"][i]["instances_transforms"].size(); instance++) {
							float matrix[16];
							for (Json::Value::ArrayIndex matC = 0; matC != mSceneJsonRoot["rendering_objects"][i]["instances_transforms"][instance]["transform"].size(); matC++) {
								matrix[matC] = mSceneJsonRoot["rendering_objects"][i]["instances_transforms"][instance]["transform"][matC].asFloat();
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
				if (mSceneJsonRoot["rendering_objects"][i].isMember("instances_transforms")) {
					aObject->ResetInstanceData(mSceneJsonRoot["rendering_objects"][i]["instances_transforms"].size(), true);
					for (Json::Value::ArrayIndex instance = 0; instance != mSceneJsonRoot["rendering_objects"][i]["instances_transforms"].size(); instance++) {
						float matrix[16];
						for (Json::Value::ArrayIndex matC = 0; matC != mSceneJsonRoot["rendering_objects"][i]["instances_transforms"][instance]["transform"].size(); matC++) {
							matrix[matC] = mSceneJsonRoot["rendering_objects"][i]["instances_transforms"][instance]["transform"][matC].asFloat();
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

	void ER_Scene::SaveFoliageZonesTransforms(const std::vector<ER_Foliage*>& foliageZones)
	{
		if (mScenePath.empty())
			throw ER_CoreException("Can't save to scene json file! Empty scene name...");

		if (mSceneJsonRoot.isMember("foliage_zones")) {
			assert(foliageZones.size() == mSceneJsonRoot["foliage_zones"].size());
			float vec3[3];
			for (Json::Value::ArrayIndex iz = 0; iz != mSceneJsonRoot["foliage_zones"].size(); iz++)
			{
				Json::Value content(Json::arrayValue);
				vec3[0] = foliageZones[iz]->GetDistributionCenter().x;
				vec3[1] = foliageZones[iz]->GetDistributionCenter().y;
				vec3[2] = foliageZones[iz]->GetDistributionCenter().z;
				for (Json::Value::ArrayIndex i = 0; i < 3; i++)
					content.append(vec3[i]);
				mSceneJsonRoot["foliage_zones"][iz]["position"] = content;
			}
		}

		Json::StreamWriterBuilder builder;
		std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());

		std::ofstream file_id;
		file_id.open(mScenePath.c_str());
		writer->write(mSceneJsonRoot, &file_id);
	}

	void ER_Scene::SaveRenderingObjectsTransforms()
	{
		if (mScenePath.empty())
			throw ER_CoreException("Can't save to scene json file! Empty scene name...");

		// store world transform
		for (Json::Value::ArrayIndex i = 0; i != mSceneJsonRoot["rendering_objects"].size(); i++) {
			Json::Value content(Json::arrayValue);
			if (mSceneJsonRoot["rendering_objects"][i].isMember("transform")) {
				ER_RenderingObject* rObj = FindRenderingObjectByName(mSceneJsonRoot["rendering_objects"][i]["name"].asString());
				if (rObj)
				{
					XMFLOAT4X4 mat = rObj->GetTransformationMatrix4X4();
					XMMATRIX matXM = XMMatrixTranspose(XMLoadFloat4x4(&mat));
					XMStoreFloat4x4(&mat, matXM);
					float matF[16];
					ER_MatrixHelper::GetFloatArray(mat, matF);
					for (int i = 0; i < 16; i++)
						content.append(matF[i]);
					mSceneJsonRoot["rendering_objects"][i]["transform"] = content;
				}
			}
			else
			{
				float matF[16];
				XMFLOAT4X4 mat;
				XMStoreFloat4x4(&mat, XMMatrixTranspose(XMMatrixIdentity()));
				ER_MatrixHelper::GetFloatArray(mat, matF);

				for (int i = 0; i < 16; i++)
					content.append(matF[i]);
				mSceneJsonRoot["rendering_objects"][i]["transform"] = content;
			}

			if (mSceneJsonRoot["rendering_objects"][i].isMember("instanced")) 
			{
				bool isInstanced = mSceneJsonRoot["rendering_objects"][i]["instanced"].asBool();
				if (isInstanced)
				{
					if (mSceneJsonRoot["rendering_objects"][i].isMember("instances_transforms")) {

						ER_RenderingObject* rObj = FindRenderingObjectByName(mSceneJsonRoot["rendering_objects"][i]["name"].asString());
						if (!rObj || mSceneJsonRoot["rendering_objects"][i]["instances_transforms"].size() != rObj->GetInstanceCount())
							throw ER_CoreException("Can't save instances transforms to scene json file! RenderObject's instance count is not equal to the number of instance transforms in scene file.");

						for (Json::Value::ArrayIndex instance = 0; instance != mSceneJsonRoot["rendering_objects"][i]["instances_transforms"].size(); instance++) {
							Json::Value contentInstanceTransform(Json::arrayValue);

							XMFLOAT4X4 mat = rObj->GetInstancesData()[instance].World;
							XMMATRIX matXM = XMMatrixTranspose(XMLoadFloat4x4(&mat));
							XMStoreFloat4x4(&mat, matXM);
							float matF[16];
							ER_MatrixHelper::GetFloatArray(mat, matF);
							for (int i = 0; i < 16; i++)
								contentInstanceTransform.append(matF[i]);

							mSceneJsonRoot["rendering_objects"][i]["instances_transforms"][instance]["transform"] = contentInstanceTransform;
						}
					}
				}
			}
		}

		Json::StreamWriterBuilder builder;
		std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());

		std::ofstream file_id;
		file_id.open(mScenePath.c_str());
		writer->write(mSceneJsonRoot, &file_id);
	}

	// We cant do reflection in C++, that is why we check every materials name and create a material out of it (and root-signature if needed)
	// "layerIndex" is used when we need to render multiple layers/copies of the material and keep track of each index
	ER_Material* ER_Scene::GetMaterialByName(const std::string& matName, const MaterialShaderEntries& entries, bool instanced, int layerIndex)
	{
		ER_Core* core = GetCore();
		assert(core);
		ER_RHI* rhi = core->GetRHI();

		ER_Material* material = nullptr;

		if (matName == "BasicColorMaterial")
			material = new ER_BasicColorMaterial(*core, entries, HAS_VERTEX_SHADER | HAS_PIXEL_SHADER /*TODO instanced support*/);
		else if (matName == "ShadowMapMaterial")
			material = new ER_ShadowMapMaterial(*core, entries, HAS_VERTEX_SHADER | HAS_PIXEL_SHADER, instanced);
		else if (matName == "GBufferMaterial")
			material = new ER_GBufferMaterial(*core, entries, HAS_VERTEX_SHADER | HAS_PIXEL_SHADER, instanced);
		else if (matName == "RenderToLightProbeMaterial")
			material = new ER_RenderToLightProbeMaterial(*core, entries, HAS_VERTEX_SHADER | HAS_PIXEL_SHADER, instanced);
		else if (matName == "VoxelizationMaterial")
			material = new ER_VoxelizationMaterial(*core, entries, HAS_VERTEX_SHADER | HAS_GEOMETRY_SHADER | HAS_PIXEL_SHADER, instanced);
		else if (matName == "SnowMaterial")
			material = new ER_SimpleSnowMaterial(*core, entries, HAS_VERTEX_SHADER | HAS_PIXEL_SHADER, instanced);
		else if (matName == "FresnelOutlineMaterial")
			material = new ER_FresnelOutlineMaterial(*core, entries, HAS_VERTEX_SHADER | HAS_PIXEL_SHADER, instanced);
		else if (matName == "FurShellMaterial")
			material = new ER_FurShellMaterial(*core, entries, HAS_VERTEX_SHADER | HAS_PIXEL_SHADER, instanced, layerIndex);
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

		if (!reader.parse(scene, mSceneJsonRoot)) {
			throw ER_CoreException(reader.getFormattedErrorMessages().c_str());
		}
		else {
			if (mSceneJsonRoot.isMember("foliage_zones")) {
				for (Json::Value::ArrayIndex i = 0; i != mSceneJsonRoot["foliage_zones"].size(); i++)
				{
					float vec3[3];
					for (Json::Value::ArrayIndex ia = 0; ia != mSceneJsonRoot["foliage_zones"][i]["position"].size(); ia++)
						vec3[ia] = mSceneJsonRoot["foliage_zones"][i]["position"][ia].asFloat();

					bool placedOnTerrain = false;
					if (mSceneJsonRoot["foliage_zones"][i].isMember("placed_on_terrain"))
						placedOnTerrain = mSceneJsonRoot["foliage_zones"][i]["placed_on_terrain"].asBool();
					
					TerrainSplatChannels terrainChannel = TerrainSplatChannels::NONE;
					if (mSceneJsonRoot["foliage_zones"][i].isMember("placed_splat_channel"))
						terrainChannel = (TerrainSplatChannels)(mSceneJsonRoot["foliage_zones"][i]["placed_splat_channel"].asInt());

					float placedHeightDelta = 0.0f;
					if (mSceneJsonRoot["foliage_zones"][i].isMember("placed_height_delta"))
						placedHeightDelta = mSceneJsonRoot["foliage_zones"][i]["placed_height_delta"].asFloat();

					foliageZones.push_back(new ER_Foliage(*core, mCamera, light,
						mSceneJsonRoot["foliage_zones"][i]["patch_count"].asInt(),
						ER_Utility::GetFilePath(mSceneJsonRoot["foliage_zones"][i]["texture_path"].asString()),
						mSceneJsonRoot["foliage_zones"][i]["average_scale"].asFloat(),
						mSceneJsonRoot["foliage_zones"][i]["distribution_radius"].asFloat(),
						XMFLOAT3(vec3[0], vec3[1], vec3[2]),
						(FoliageBillboardType)mSceneJsonRoot["foliage_zones"][i]["type"].asInt(), placedOnTerrain, terrainChannel, placedHeightDelta));
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

	ER_RenderingObject* ER_Scene::FindRenderingObjectByName(const std::string& aName)
	{
		for (auto& sceneObj : objects)
		{
			if (sceneObj.first == aName)
				return sceneObj.second;
			else
				continue;
		}

		return nullptr;
	}
}