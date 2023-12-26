
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
#include "ER_PointLight.h"
#include "ER_Terrain.h"
#include "ER_PostProcessingStack.h"

#if defined(DEBUG) || defined(_DEBUG)  
	#define MULTITHREADED_SCENE_LOAD 0
#else
	#define MULTITHREADED_SCENE_LOAD 1
#endif

namespace EveryRay_Core 
{
	template<>
	int ER_Scene::GetValueFromSceneRoot(const std::string& aName)
	{
		int value = 0;
		if (mSceneJsonRoot.isMember(aName.c_str()))
			value = mSceneJsonRoot[aName.c_str()].asInt();
		else
			ShowNoValueFoundMessage(aName);

		return value;
	}

	template<>
	UINT ER_Scene::GetValueFromSceneRoot(const std::string& aName)
	{
		UINT value = 0;
		if (mSceneJsonRoot.isMember(aName.c_str()))
			value = mSceneJsonRoot[aName.c_str()].asUInt();
		else
			ShowNoValueFoundMessage(aName);

		return value;
	}

	template<>
	bool ER_Scene::GetValueFromSceneRoot(const std::string& aName)
	{
		bool value = false;
		if (mSceneJsonRoot.isMember(aName.c_str()))
			value = mSceneJsonRoot[aName.c_str()].asBool();
		else
			ShowNoValueFoundMessage(aName);

		return value;
	}

	template<>
	float ER_Scene::GetValueFromSceneRoot(const std::string& aName)
	{
		float value = 0.0f;
		if (mSceneJsonRoot.isMember(aName.c_str()))
			value = mSceneJsonRoot[aName.c_str()].asFloat();
		else
			ShowNoValueFoundMessage(aName);

		return value;
	}

	template<>
	XMFLOAT3 ER_Scene::GetValueFromSceneRoot(const std::string& aName)
	{
		XMFLOAT3 value = XMFLOAT3(0.0f, 0.0f, 0.0f);
		if (mSceneJsonRoot.isMember(aName.c_str()))
		{
			float vec3[3] = { 0.0, 0.0, 0.0 };
			for (Json::Value::ArrayIndex i = 0; i != mSceneJsonRoot[aName.c_str()].size(); i++)
				vec3[i] = mSceneJsonRoot[aName.c_str()][i].asFloat();

			value = XMFLOAT3(vec3[0], vec3[1], vec3[2]);
		}
		else
			ShowNoValueFoundMessage(aName);

		return value;
	}

	template<>
	XMFLOAT4 ER_Scene::GetValueFromSceneRoot(const std::string& aName)
	{
		XMFLOAT4 value = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		if (mSceneJsonRoot.isMember(aName.c_str()))
		{
			float vec4[4] = { 0.0, 0.0, 0.0, 0.0 };
			for (Json::Value::ArrayIndex i = 0; i != mSceneJsonRoot[aName.c_str()].size(); i++)
				vec4[i] = mSceneJsonRoot[aName.c_str()][i].asFloat();

			value = XMFLOAT4(vec4[0], vec4[1], vec4[2], vec4[3]);
		}
		else
			ShowNoValueFoundMessage(aName);

		return value;
	}

	template<>
	std::string ER_Scene::GetValueFromSceneRoot(const std::string& aName)
	{
		std::string value = "";
		if (mSceneJsonRoot.isMember(aName.c_str()))
			value = mSceneJsonRoot[aName.c_str()].asString();
		else
			ShowNoValueFoundMessage(aName);

		return value;
	}

	bool ER_Scene::IsValueInSceneRoot(const std::string& aName)
	{
		return mSceneJsonRoot.isMember(aName.c_str());
	}

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

			// load camera
			{
				mCamera.SetPosition(GetValueFromSceneRoot<XMFLOAT3>("camera_position"));

				if (IsValueInSceneRoot("camera_direction"))
					mCamera.SetDirection(GetValueFromSceneRoot<XMFLOAT3>("camera_direction"));

				if (IsValueInSceneRoot("camera_plane_far"))
					mCamera.SetFarPlaneDistance(GetValueFromSceneRoot<float>("camera_plane_far"));

				if (IsValueInSceneRoot("camera_plane_near"))
					mCamera.SetNearPlaneDistance(GetValueFromSceneRoot<float>("camera_plane_near"));
			}

			// add rendering objects to scene
			unsigned int numRenderingObjects = mSceneJsonRoot["rendering_objects"].size();
			for (Json::Value::ArrayIndex i = 0; i != numRenderingObjects; i++) {
				objects.emplace_back(
					mSceneJsonRoot["rendering_objects"][i]["name"].asString(), 
					new ER_RenderingObject(mSceneJsonRoot["rendering_objects"][i]["name"].asString(), i, *mCore, mCamera, 
						ER_Utility::GetFilePath(mSceneJsonRoot["rendering_objects"][i]["model_path"].asString()),
						true, mSceneJsonRoot["rendering_objects"][i]["instanced"].asBool())
				);
			}
			std::partition(objects.begin(), objects.end(), [](const ER_SceneObject& obj) {	return obj.second->IsInstanced(); });
			assert(numRenderingObjects == objects.size());

#if MULTITHREADED_SCENE_LOAD && !ER_PLATFORM_WIN64_DX12
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
				rs->InitDescriptorTable(rhi, SNOW_MAT_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, 
					{ ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { LIGHTING_SRV_INDEX_MAX_RESERVED_FOR_TEXTURES + NUM_SHADOW_CASCADES + 1 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
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
				rs->InitDescriptorTable(rhi, FUR_SHELL_MAT_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, 
					{ ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { LIGHTING_SRV_INDEX_MAX_RESERVED_FOR_TEXTURES + NUM_SHADOW_CASCADES + 1 }, ER_RHI_SHADER_VISIBILITY_PIXEL);
				rs->InitDescriptorTable(rhi, FUR_SHELL_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 2 }, ER_RHI_SHADER_VISIBILITY_ALL);
				rs->Finalize(rhi, "ER_RHI_GPURootSignature: FurShellMaterial Pass", true);
			}
			mStandardMaterialsRootSignatures.emplace(ER_MaterialHelper::furShellMaterialName, rs);
		}
	}

	void ER_Scene::LoadRenderingObjectData(ER_RenderingObject* aObject)
	{
		if (!aObject || !aObject->IsLoaded())
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

			if (mSceneJsonRoot["rendering_objects"][i].isMember("use_gpu_indirect_rendering"))
				aObject->SetGPUIndirectlyRendered(mSceneJsonRoot["rendering_objects"][i]["use_gpu_indirect_rendering"].asBool());

			if (mSceneJsonRoot["rendering_objects"][i].isMember("skip_indirect_specular"))
				aObject->SetSkipIndirectSpecular(mSceneJsonRoot["rendering_objects"][i]["skip_indirect_specular"].asBool());

			if (mSceneJsonRoot["rendering_objects"][i].isMember("index_of_refraction"))
				aObject->SetIOR(mSceneJsonRoot["rendering_objects"][i]["index_of_refraction"].asFloat());

			if (mSceneJsonRoot["rendering_objects"][i].isMember("custom_roughness"))
				aObject->SetCustomRoughness(mSceneJsonRoot["rendering_objects"][i]["custom_roughness"].asFloat());

			if (mSceneJsonRoot["rendering_objects"][i].isMember("custom_metalness"))
				aObject->SetCustomMetalness(mSceneJsonRoot["rendering_objects"][i]["custom_metalness"].asFloat());

			if (mSceneJsonRoot["rendering_objects"][i].isMember("use_triplanar_mapping"))
				aObject->SetTriplanarMapping(mSceneJsonRoot["rendering_objects"][i]["use_triplanar_mapping"].asBool());
			//if (mSceneJsonRoot["rendering_objects"][i].isMember("triplanar_mapping_sharpness"))
			//	aObject->SetTriplanarMappedSharpness(mSceneJsonRoot["rendering_objects"][i]["triplanar_mapping_sharpness"].asFloat());

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

				if (mSceneJsonRoot["rendering_objects"][i].isMember("terrain_height_delta"))
					aObject->SetTerrainProceduralPlacementHeightDelta(mSceneJsonRoot["rendering_objects"][i]["terrain_height_delta"].asFloat());

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

		// load textures
		{
			const int meshCount = aObject->GetMeshCount();

			const bool containsCustomTextures = mSceneJsonRoot["rendering_objects"][i].isMember("textures");
			const int maxCustomTextures = mSceneJsonRoot["rendering_objects"][i]["textures"].size();

			for (int meshIndex = 0; meshIndex < meshCount; meshIndex++)
			{
				if (containsCustomTextures && meshIndex < maxCustomTextures)
				{
					if (mSceneJsonRoot["rendering_objects"][i]["textures"][meshIndex].isMember("albedo"))
						aObject->mCustomAlbedoTextures[meshIndex] = mSceneJsonRoot["rendering_objects"][i]["textures"][meshIndex]["albedo"].asString();
					if (mSceneJsonRoot["rendering_objects"][i]["textures"][meshIndex].isMember("normal"))
						aObject->mCustomNormalTextures[meshIndex] = mSceneJsonRoot["rendering_objects"][i]["textures"][meshIndex]["normal"].asString();
					if (mSceneJsonRoot["rendering_objects"][i]["textures"][meshIndex].isMember("roughness"))
						aObject->mCustomRoughnessTextures[meshIndex] = mSceneJsonRoot["rendering_objects"][i]["textures"][meshIndex]["roughness"].asString();
					if (mSceneJsonRoot["rendering_objects"][i]["textures"][meshIndex].isMember("metalness"))
						aObject->mCustomMetalnessTextures[meshIndex] = mSceneJsonRoot["rendering_objects"][i]["textures"][meshIndex]["metalness"].asString();
					if (mSceneJsonRoot["rendering_objects"][i]["textures"][meshIndex].isMember("height"))
						aObject->mCustomHeightTextures[meshIndex] = mSceneJsonRoot["rendering_objects"][i]["textures"][meshIndex]["height"].asString();
					if (mSceneJsonRoot["rendering_objects"][i]["textures"][meshIndex].isMember("reflection_mask"))
						aObject->mCustomReflectionMaskTextures[meshIndex] = mSceneJsonRoot["rendering_objects"][i]["textures"][meshIndex]["reflection_mask"].asString();

					aObject->LoadCustomMeshTextures(meshIndex);
				}
				aObject->LoadAssignedMeshTextures(meshIndex);
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
					aObject->AddLOD(ER_Utility::GetFilePath(path));
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
			for (int lod = 0; lod < static_cast<int>(mSceneJsonRoot["rendering_objects"][i]["model_lods"].size()); lod++)
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
	// TODO: add functionality for storing flags, etc. (currently only transforms can be saved to json)
	void ER_Scene::SaveRenderingObjectsData()
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
					ER_MatrixHelper::SetFloatArray(mat, matF);
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
				ER_MatrixHelper::SetFloatArray(mat, matF);

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
							ER_MatrixHelper::SetFloatArray(mat, matF);
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

	void ER_Scene::LoadFoliageZonesData(std::vector<ER_Foliage*>& foliageZones, ER_DirectionalLight& light)
	{
		Json::Reader reader;
		std::ifstream scene(mScenePath.c_str(), std::ifstream::binary);
		ER_Core* core = GetCore();
		assert(core);

		if (!reader.parse(scene, mSceneJsonRoot)) {
			throw ER_CoreException(reader.getFormattedErrorMessages().c_str());
		}
		else {
			if (mSceneJsonRoot.isMember("foliage_zones")) 
			{
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
		}
	}
	// TODO: add functionality for storing flags, etc. (currently only transforms can be saved to json)
	void ER_Scene::SaveFoliageZonesData(const std::vector<ER_Foliage*>& foliageZones)
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

	void ER_Scene::LoadPostProcessingVolumesData()
	{
		ER_Core* core = GetCore();
		assert(core);

		ER_PostProcessingStack* pp = core->GetLevel()->mPostProcessingStack;

		Json::Reader reader;
		std::ifstream scene(mScenePath.c_str(), std::ifstream::binary);

		if (!reader.parse(scene, mSceneJsonRoot)) {
			throw ER_CoreException(reader.getFormattedErrorMessages().c_str());
		}
		else
		{
			if (mSceneJsonRoot.isMember("posteffects_volumes")) 
			{
				XMFLOAT4X4 transform = 
				{
					1.f, 0.f, 0.f, 0.f,
					0.f, 1.f, 0.f, 0.f,
					0.f, 0.f, 1.f, 0.f,
					0.f, 0.f, 0.f, 1.f
				};
				int size = mSceneJsonRoot["posteffects_volumes"].size();
				pp->ReservePostEffectsVolumes(size);
				for (Json::Value::ArrayIndex i = 0; i != size; i++)
				{
					if (mSceneJsonRoot["posteffects_volumes"][i].isMember("volume_transform"))
					{
						if (mSceneJsonRoot["posteffects_volumes"][i]["volume_transform"].size() == 16)
						{
							float matrix[16];
							for (Json::Value::ArrayIndex matC = 0; matC != mSceneJsonRoot["posteffects_volumes"][i]["volume_transform"].size(); matC++)
								matrix[matC] = mSceneJsonRoot["posteffects_volumes"][i]["volume_transform"][matC].asFloat();

							XMFLOAT4X4 worldTransform(matrix);
							XMMATRIX transformM = XMMatrixTranspose(XMLoadFloat4x4(&worldTransform));

							XMStoreFloat4x4(&transform, transformM);
						}
					}

					PostEffectsVolumeValues values = {};

					if (mSceneJsonRoot["posteffects_volumes"][i].isMember("posteffects_linearfog_enabled"))
						values.linearFogEnable = mSceneJsonRoot["posteffects_volumes"][i]["posteffects_linearfog_enabled"].asBool();
					if (mSceneJsonRoot["posteffects_volumes"][i].isMember("posteffects_linearfog_density"))
						values.linearFogDensity = mSceneJsonRoot["posteffects_volumes"][i]["posteffects_linearfog_density"].asFloat();
					if (mSceneJsonRoot["posteffects_volumes"][i].isMember("posteffects_linearfog_color"))
					{
						float vec3[3];
						for (Json::Value::ArrayIndex j = 0; j != mSceneJsonRoot["posteffects_volumes"][i]["posteffects_linearfog_color"].size(); j++)
							vec3[j] = mSceneJsonRoot["posteffects_volumes"][i]["posteffects_linearfog_color"][j].asFloat();

						values.linearFogColor[0] = vec3[0];
						values.linearFogColor[1] = vec3[1];
						values.linearFogColor[2] = vec3[2];
					}

					if (mSceneJsonRoot["posteffects_volumes"][i].isMember("posteffects_tonemapping_enabled"))
						values.tonemappingEnable = mSceneJsonRoot["posteffects_volumes"][i]["posteffects_tonemapping_enabled"].asBool();

					if (mSceneJsonRoot["posteffects_volumes"][i].isMember("posteffects_sss_enabled"))
						values.sssEnable = mSceneJsonRoot["posteffects_volumes"][i]["posteffects_sss_enabled"].asBool();

					if (mSceneJsonRoot["posteffects_volumes"][i].isMember("posteffects_ssr_enabled"))
						values.ssrEnable = mSceneJsonRoot["posteffects_volumes"][i]["posteffects_ssr_enabled"].asBool();
					if (mSceneJsonRoot["posteffects_volumes"][i].isMember("posteffects_ssr_maxthickness"))
						values.ssrMaxThickness = mSceneJsonRoot["posteffects_volumes"][i]["posteffects_ssr_maxthickness"].asFloat();
					if (mSceneJsonRoot["posteffects_volumes"][i].isMember("posteffects_ssr_stepsize"))
						values.ssrStepSize = mSceneJsonRoot["posteffects_volumes"][i]["posteffects_ssr_stepsize"].asFloat();

					if (mSceneJsonRoot["posteffects_volumes"][i].isMember("posteffects_vignette_enabled"))
						values.vignetteEnable = mSceneJsonRoot["posteffects_volumes"][i]["posteffects_vignette_enabled"].asBool();
					if (mSceneJsonRoot["posteffects_volumes"][i].isMember("posteffects_vignette_softness"))
						values.vignetteSoftness = mSceneJsonRoot["posteffects_volumes"][i]["posteffects_vignette_softness"].asFloat();
					if (mSceneJsonRoot["posteffects_volumes"][i].isMember("posteffects_vignette_radius"))
						values.vignetteRadius = mSceneJsonRoot["posteffects_volumes"][i]["posteffects_vignette_radius"].asFloat();

					if (mSceneJsonRoot["posteffects_volumes"][i].isMember("posteffects_colorgrading_enabled"))
						values.colorGradingEnable = mSceneJsonRoot["posteffects_volumes"][i]["posteffects_colorgrading_enabled"].asBool();
					if (mSceneJsonRoot["posteffects_volumes"][i].isMember("posteffects_colorgrading_lut"))
						values.colorGradingLUTIndex = mSceneJsonRoot["posteffects_volumes"][i]["posteffects_colorgrading_lut"].asUInt();

					std::string name = "";
					if (mSceneJsonRoot["posteffects_volumes"][i].isMember("volume_name"))
						name = mSceneJsonRoot["posteffects_volumes"][i]["volume_name"].asString();

					pp->AddPostEffectsVolume(transform, values, name);
				}
			}
		}

		// set default flags

		if (mSceneJsonRoot.isMember("posteffects_aa_enabled"))
			pp->SetUseAntiAliasing(mSceneJsonRoot["posteffects_aa_enabled"].asBool());

		if (mSceneJsonRoot.isMember("posteffects_ssr_default"))
			pp->SetUseSSR(mSceneJsonRoot["posteffects_ssr_default"].asBool(), true);

		if (mSceneJsonRoot.isMember("posteffects_sss_default"))
			pp->SetUseSSS(mSceneJsonRoot["posteffects_sss_default"].asBool(), true);

		if (mSceneJsonRoot.isMember("posteffects_vignette_default"))
			pp->SetUseVignette(mSceneJsonRoot["posteffects_vignette_default"].asBool(), true);

		if (mSceneJsonRoot.isMember("posteffects_tonmapping_default"))
			pp->SetUseTonemapping(mSceneJsonRoot["posteffects_tonmapping_default"].asBool(), true);

		if (mSceneJsonRoot.isMember("posteffects_linearfog_default"))
			pp->SetUseLinearFog(mSceneJsonRoot["posteffects_linearfog_default"].asBool(), true);

		if (mSceneJsonRoot.isMember("posteffects_colorgrading_default"))
			pp->SetUseColorGrading(mSceneJsonRoot["posteffects_colorgrading_default"].asBool(), true);
	}
	void ER_Scene::SavePostProcessingVolumesData()
	{
		if (mScenePath.empty())
			throw ER_CoreException("Can't save to scene json file! Empty scene name...");

		ER_Core* core = GetCore();
		assert(core);

		ER_PostProcessingStack* pp = core->GetLevel()->mPostProcessingStack;

		if (mSceneJsonRoot.isMember("posteffects_volumes"))
		{
			assert(pp->GetPostEffectsVolumesCount() == mSceneJsonRoot["posteffects_volumes"].size());
			for (Json::Value::ArrayIndex i = 0; i != mSceneJsonRoot["posteffects_volumes"].size(); i++)
			{
				Json::Value content(Json::arrayValue);
				if (mSceneJsonRoot["posteffects_volumes"][i].isMember("volume_transform"))
				{
					const PostEffectsVolume& volume = pp->GetPostEffectsVolume(i);
					
					XMFLOAT4X4 mat = volume.GetTransform();
					XMMATRIX matXM = XMMatrixTranspose(XMLoadFloat4x4(&mat));
					XMStoreFloat4x4(&mat, matXM);
					float matF[16];
					ER_MatrixHelper::SetFloatArray(mat, matF);
					for (int i = 0; i < 16; i++)
						content.append(matF[i]);
					mSceneJsonRoot["posteffects_volumes"][i]["volume_transform"] = content;
				}
				else
				{
					float matF[16];
					XMFLOAT4X4 mat;
					XMStoreFloat4x4(&mat, XMMatrixTranspose(XMMatrixIdentity()));
					ER_MatrixHelper::SetFloatArray(mat, matF);

					for (int i = 0; i < 16; i++)
						content.append(matF[i]);
					mSceneJsonRoot["posteffects_volumes"][i]["volume_transform"] = content;
				}
			}
		}

		Json::StreamWriterBuilder builder;
		std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());

		std::ofstream file_id;
		file_id.open(mScenePath.c_str());
		writer->write(mSceneJsonRoot, &file_id);
	}

	void ER_Scene::LoadPointLightsData()
	{
		std::vector<ER_PointLight*>& lights = GetCore()->GetLevel()->mPointLights;

		unsigned int numLights = mSceneJsonRoot["point_lights"].size();
		for (Json::Value::ArrayIndex i = 0; i != numLights; i++)
		{
			float position[3] = { 0.0, 0.0, 0.0 };
			if (mSceneJsonRoot["point_lights"][i].isMember("position"))
			{
				for (Json::Value::ArrayIndex j = 0; j != mSceneJsonRoot["point_lights"][i]["position"].size(); j++)
					position[j] = mSceneJsonRoot["point_lights"][i]["position"][j].asFloat();

				//light.SetPosition(XMFLOAT3(position[0], position[1], position[2]));
			}

			float radius = 0.0;
			if (mSceneJsonRoot["point_lights"][i].isMember("radius"))
				radius = mSceneJsonRoot["point_lights"][i]["radius"].asFloat();
			//light.SetRadius(mSceneJsonRoot["point_lights"][i]["radius"].asFloat());

			if (static_cast<int>(lights.size()) < MAX_NUM_POINT_LIGHTS)
				lights.push_back(new ER_PointLight(*GetCore(), XMFLOAT3(position[0], position[1], position[2]), radius));
			else
				throw ER_CoreException("Exceeded the max number of point lights when loading the scene!");

			if (mSceneJsonRoot["point_lights"][i].isMember("color"))
			{
				float vec4[4];
				for (Json::Value::ArrayIndex j = 0; j != mSceneJsonRoot["point_lights"][i]["color"].size(); j++)
					vec4[j] = mSceneJsonRoot["point_lights"][i]["color"][j].asFloat();

				lights.back()->SetColor(XMFLOAT4(vec4[0], vec4[1], vec4[2], vec4[3]));
			}

		}
	}
	void ER_Scene::SavePointLightsData()
	{
		if (mScenePath.empty())
			throw ER_CoreException("Can't save to scene json file! Empty scene name...");

		std::vector<ER_PointLight*>& lights = GetCore()->GetLevel()->mPointLights;
		
		// store world transform
		for (Json::Value::ArrayIndex i = 0; i != mSceneJsonRoot["point_lights"].size(); i++) 
		{
			ER_PointLight* light = lights[i];

			if (mSceneJsonRoot["point_lights"][i].isMember("position")) 
			{
				Json::Value content(Json::arrayValue);
				if (light)
				{
					content.append(light->GetPosition().x);
					content.append(light->GetPosition().y);
					content.append(light->GetPosition().z);

					mSceneJsonRoot["point_lights"][i]["position"] = content;
				}
			}

			if (mSceneJsonRoot["point_lights"][i].isMember("color"))
			{
				Json::Value content(Json::arrayValue);
				if (light)
				{
					content.append(light->GetColor().x);
					content.append(light->GetColor().y);
					content.append(light->GetColor().z);
					content.append(light->GetColor().w);

					mSceneJsonRoot["point_lights"][i]["color"] = content;
				}
			}

			if (mSceneJsonRoot["point_lights"][i].isMember("radius"))
			{
				if (light)
					mSceneJsonRoot["point_lights"][i]["radius"] = light->mRadius;
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

	void ER_Scene::ShowNoValueFoundMessage(const std::string& aName)
	{
		std::wstring msg = L"[ER Logger][ER_Scene] Could not load a requested value: " + ER_Utility::ToWideString(aName) + L"\n";
		ER_OUTPUT_LOG(msg.c_str());
	}
}