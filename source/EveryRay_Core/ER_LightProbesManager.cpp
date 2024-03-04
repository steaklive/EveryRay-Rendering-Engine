#include "ER_LightProbesManager.h"
#include "ER_Core.h"
#include "ER_CoreTime.h"
#include "ER_CoreException.h"
#include "ER_Utility.h"
#include "ER_DirectionalLight.h"
#include "ER_MatrixHelper.h"
#include "ER_MaterialHelper.h"
#include "ER_VectorHelper.h"
#include "ER_RenderingObject.h"
#include "ER_Model.h"
#include "ER_Scene.h"
#include "ER_RenderableAABB.h"
#include "ER_QuadRenderer.h"
#include "ER_DebugLightProbeMaterial.h"
#include "ER_MaterialsCallbacks.h"
#include "ER_Terrain.h"

namespace EveryRay_Core
{
	ER_LightProbesManager::ER_LightProbesManager(ER_Core& game, ER_Camera& camera, ER_Scene* scene, ER_DirectionalLight* light, ER_ShadowMapper* shadowMapper)
		: mMainCamera(camera)
	{
		if (!scene)
			throw ER_CoreException("No scene to load light probes for!");

		ER_RHI* rhi = game.GetRHI();

		mTempDiffuseCubemapFacesRT = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Temp Diffuse Cubemap RT");
		mTempDiffuseCubemapFacesRT->CreateGPUTextureResource(rhi, DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE_SIZE, 1, ER_FORMAT_R16G16B16A16_FLOAT,
			ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, 1, -1, CUBEMAP_FACES_COUNT, true);

		mTempDiffuseCubemapFacesConvolutedRT = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Temp Diffuse Cubemap Convoluted RT");
		mTempDiffuseCubemapFacesConvolutedRT->CreateGPUTextureResource(rhi, DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE_SIZE, 1, ER_FORMAT_R16G16B16A16_FLOAT,
			ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, 1, -1, CUBEMAP_FACES_COUNT, true);
		
		mTempSpecularCubemapFacesRT = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Temp Specular Cubemap RT");
		mTempSpecularCubemapFacesRT->CreateGPUTextureResource(rhi, SPECULAR_PROBE_SIZE, SPECULAR_PROBE_SIZE, 1, ER_FORMAT_R8G8B8A8_UNORM,
			ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, SPECULAR_PROBE_MIP_COUNT, -1, CUBEMAP_FACES_COUNT, true);

		mTempSpecularCubemapFacesConvolutedRT = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Temp Specular Cubemap Convoluted RT");
		mTempSpecularCubemapFacesConvolutedRT->CreateGPUTextureResource(rhi, SPECULAR_PROBE_SIZE, SPECULAR_PROBE_SIZE, 1, ER_FORMAT_R8G8B8A8_UNORM,
			ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, SPECULAR_PROBE_MIP_COUNT, -1, CUBEMAP_FACES_COUNT, true);

		for (int i = 0; i < CUBEMAP_FACES_COUNT; i++)
		{
			mTempDiffuseCubemapDepthBuffers[i] = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Temp Cubemap Diffuse Depth Buffers");
			mTempDiffuseCubemapDepthBuffers[i]->CreateGPUTextureResource(rhi, DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE_SIZE, 1u, ER_FORMAT_D24_UNORM_S8_UINT,ER_BIND_SHADER_RESOURCE | ER_BIND_DEPTH_STENCIL);
			
			mTempSpecularCubemapDepthBuffers[i] = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Temp Cubemap Specular Depth Buffers");
			mTempSpecularCubemapDepthBuffers[i]->CreateGPUTextureResource(rhi, SPECULAR_PROBE_SIZE, SPECULAR_PROBE_SIZE, 1u, ER_FORMAT_D24_UNORM_S8_UINT, ER_BIND_SHADER_RESOURCE | ER_BIND_DEPTH_STENCIL);
		}

		mQuadRenderer = (ER_QuadRenderer*)game.GetServices().FindService(ER_QuadRenderer::TypeIdClass());
		assert(mQuadRenderer);
		mConvolutionPS = rhi->CreateGPUShader();
		mConvolutionPS->CompileShader(rhi, "content\\shaders\\IBL\\ProbeConvolution.hlsl", "PSMain", ER_PIXEL);

		mIntegrationMapTextureSRV = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Integration Map BRDF");
		mIntegrationMapTextureSRV->CreateGPUTextureResource(rhi, ER_Utility::GetFilePath(L"content\\textures\\IntegrationMapBrdf.dds"), true);

		if (!scene->IsValueInSceneRoot("light_probes_volume_bounds_min") && !scene->IsValueInSceneRoot("light_probes_volume_bounds_max"))
		{
			mEnabled = false;

			SetupGlobalDiffuseProbe(game, camera, scene, light, shadowMapper);
			SetupGlobalSpecularProbe(game, camera, scene, light, shadowMapper);
			return;
		}

		mSceneProbesMinBounds = scene->GetValueFromSceneRoot<XMFLOAT3>("light_probes_volume_bounds_min");
		mSceneProbesMaxBounds = scene->GetValueFromSceneRoot<XMFLOAT3>("light_probes_volume_bounds_max");
		mIsPlacedOnTerrain = scene->GetValueFromSceneRoot<bool>("light_probes_place_on_terrain");
		if (mIsPlacedOnTerrain)
		{
			mTerrainPlacementHeightDeltaDiffuseProbes = scene->GetValueFromSceneRoot<float>("light_probes_diffuse_on_terrain_height_delta");
			mTerrainPlacementHeightDeltaSpecularProbes = scene->GetValueFromSceneRoot<float>("light_probes_specular_on_terrain_height_delta");

			mCurrentProbeCountPerCell = PROBE_COUNT_PER_CELL_2D;
			mIs2DCellsGrid = true; // for now we only support 2D grid of probes that are placed on terrain (y value is ignored for cells but not for probes)
		}

		mDistanceBetweenDiffuseProbes = scene->IsValueInSceneRoot("light_probes_diffuse_distance") ? scene->GetValueFromSceneRoot<float>("light_probes_diffuse_distance") : -1.0f;
		if (mDistanceBetweenDiffuseProbes > 0)
			SetupDiffuseProbes(game, camera, scene, light, shadowMapper);
		else
			SetupGlobalDiffuseProbe(game, camera, scene, light, shadowMapper);
		
		mDistanceBetweenSpecularProbes = scene->IsValueInSceneRoot("light_probes_specular_distance") ? scene->GetValueFromSceneRoot<float>("light_probes_specular_distance") : -1.0f;
		if (mDistanceBetweenSpecularProbes > 0)
		{
			mSpecularProbesVolumeSize = MAX_CUBEMAPS_IN_VOLUME_PER_AXIS * mDistanceBetweenSpecularProbes * 0.5f;
			mMaxSpecularProbesInVolumeCount = MAX_CUBEMAPS_IN_VOLUME_PER_AXIS * MAX_CUBEMAPS_IN_VOLUME_PER_AXIS * MAX_CUBEMAPS_IN_VOLUME_PER_AXIS;

			SetupSpecularProbes(game, camera, scene, light, shadowMapper);
		}
		else
			SetupGlobalSpecularProbe(game, camera, scene, light, shadowMapper);
	}

	ER_LightProbesManager::~ER_LightProbesManager()
	{
		DeleteObject(mGlobalDiffuseProbe);
		DeleteObject(mGlobalSpecularProbe);
		//DeletePointerCollection(mDiffuseProbes);
		//DeletePointerCollection(mSpecularProbes);

		DeleteObject(mConvolutionPS);

		DeleteObject(mTempDiffuseCubemapFacesRT);
		DeleteObject(mTempDiffuseCubemapFacesConvolutedRT);
		DeleteObject(mTempSpecularCubemapFacesRT);
		DeleteObject(mTempSpecularCubemapFacesConvolutedRT);
		for (int i = 0; i < CUBEMAP_FACES_COUNT; i++)
		{	
			DeleteObject(mTempDiffuseCubemapDepthBuffers[i]);
			DeleteObject(mTempSpecularCubemapDepthBuffers[i]);
		}
		DeleteObject(mIntegrationMapTextureSRV);

		DeleteObject(mDiffuseProbesSphericalHarmonicsGPUBuffer);

		DeleteObject(mDiffuseProbesPositionsGPUBuffer);
		DeleteObject(mSpecularProbesPositionsGPUBuffer);
		if (mIsPlacedOnTerrain)
		{
			DeleteObject(mDiffuseInputPositionsOnTerrainBuffer);
			DeleteObject(mDiffuseOutputPositionsOnTerrainBuffer);
			DeleteObject(mSpecularInputPositionsOnTerrainBuffer);
			DeleteObject(mSpecularOutputPositionsOnTerrainBuffer);
		}

		DeleteObject(mDiffuseProbesCellsIndicesGPUBuffer);

		DeleteObject(mSpecularCubemapArrayRT);
		DeleteObject(mSpecularProbesCellsIndicesGPUBuffer);
		DeleteObjects(mSpecularProbesTexArrayIndicesCPUBuffer);
		DeleteObject(mSpecularProbesTexArrayIndicesGPUBuffer);
	}

	void ER_LightProbesManager::SetupGlobalDiffuseProbe(ER_Core& game, ER_Camera& camera, ER_Scene* scene, ER_DirectionalLight* light, ER_ShadowMapper* shadowMapper)
	{
		mGlobalDiffuseProbe = new ER_LightProbe(game, light, shadowMapper, DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE, -1);
		mGlobalDiffuseProbe->SetPosition(scene->GetValueFromSceneRoot<XMFLOAT3>("light_probe_global_cam_position"));
		mGlobalDiffuseProbe->SetShaderInfoForConvolution(mConvolutionPS);
	}
	void ER_LightProbesManager::SetupGlobalSpecularProbe(ER_Core& game, ER_Camera& camera, ER_Scene* scene, ER_DirectionalLight* light, ER_ShadowMapper*shadowMapper)
	{
		mGlobalSpecularProbe = new ER_LightProbe(game, light, shadowMapper, SPECULAR_PROBE_SIZE, SPECULAR_PROBE, -1);
		mGlobalSpecularProbe->SetPosition(scene->GetValueFromSceneRoot<XMFLOAT3>("light_probe_global_cam_position"));
		mGlobalSpecularProbe->SetShaderInfoForConvolution(mConvolutionPS);
	}

	void ER_LightProbesManager::SetupDiffuseProbes(ER_Core& core, ER_Camera& camera, ER_Scene* scene, ER_DirectionalLight* light, ER_ShadowMapper* shadowMapper)
	{
		ER_RHI* rhi = core.GetRHI();

		ER_MaterialSystems materialSystems;
		materialSystems.mCamera = &camera;
		materialSystems.mDirectionalLight = light;
		materialSystems.mShadowMapper = shadowMapper;
		materialSystems.mProbesManager = this;

		XMFLOAT3& minBounds = mSceneProbesMinBounds;
		XMFLOAT3& maxBounds = mSceneProbesMaxBounds;

		SetupGlobalDiffuseProbe(core, camera, scene, light, shadowMapper);

		mDiffuseProbesCountX = (maxBounds.x - minBounds.x) / mDistanceBetweenDiffuseProbes + 1;
		if (!mIs2DCellsGrid)
			mDiffuseProbesCountY = (maxBounds.y - minBounds.y) / mDistanceBetweenDiffuseProbes + 1;
		else
			mDiffuseProbesCountY = 1;
		mDiffuseProbesCountZ = (maxBounds.z - minBounds.z) / mDistanceBetweenDiffuseProbes + 1;
		mDiffuseProbesCountTotal = mDiffuseProbesCountX * mDiffuseProbesCountY * mDiffuseProbesCountZ;
		assert(mDiffuseProbesCountTotal);
		mDiffuseProbes.reserve(mDiffuseProbesCountTotal);

		// cells setup
		mDiffuseProbesCellsCountX = (mDiffuseProbesCountX - 1);
		mDiffuseProbesCellsCountZ = (mDiffuseProbesCountZ - 1);
		if (!mIs2DCellsGrid)
		{
			mDiffuseProbesCellsCountY = (mDiffuseProbesCountY - 1);
			mDiffuseProbesCellsCountTotal = mDiffuseProbesCellsCountX * mDiffuseProbesCellsCountY * mDiffuseProbesCellsCountZ;
		}
		else
		{
			mDiffuseProbesCellsCountY = 1;
			mDiffuseProbesCellsCountTotal = mDiffuseProbesCellsCountX * mDiffuseProbesCellsCountZ;
		}
		mDiffuseProbesCells.resize(mDiffuseProbesCellsCountTotal, {});

		assert(mDiffuseProbesCellsCountTotal);

		float probeCellPositionOffset = static_cast<float>(mDistanceBetweenDiffuseProbes) / 2.0f;
		mDiffuseProbesCellBounds = {
			XMFLOAT3(-probeCellPositionOffset, mIs2DCellsGrid ? 0.0f : -probeCellPositionOffset, -probeCellPositionOffset),
			XMFLOAT3(probeCellPositionOffset, mIs2DCellsGrid ? 0.0f : probeCellPositionOffset, probeCellPositionOffset) };
		{
			for (int cellsY = 0; cellsY < mDiffuseProbesCellsCountY; cellsY++)
			{
				for (int cellsX = 0; cellsX < mDiffuseProbesCellsCountX; cellsX++)
				{
					for (int cellsZ = 0; cellsZ < mDiffuseProbesCellsCountZ; cellsZ++)
					{
						XMFLOAT3 pos = XMFLOAT3(
							minBounds.x + probeCellPositionOffset + cellsX * mDistanceBetweenDiffuseProbes,
							minBounds.y + mIs2DCellsGrid ? 0.0f : probeCellPositionOffset + cellsY * mDistanceBetweenDiffuseProbes,
							minBounds.z + probeCellPositionOffset + cellsZ * mDistanceBetweenDiffuseProbes);

						int index = cellsY * (mDiffuseProbesCellsCountX * mDiffuseProbesCellsCountZ) + cellsX * mDiffuseProbesCellsCountZ + cellsZ;
						mDiffuseProbesCells[index].index = index;
						mDiffuseProbesCells[index].position = pos;
					}
				}
			}
		}

		// simple 3D grid distribution of probes or 2D grid (i.e. when placeable on terrain)
		for (int i = 0; i < mDiffuseProbesCountTotal; i++)
			mDiffuseProbes.emplace_back(core, light, shadowMapper, DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE, i);

		for (int probesY = 0; probesY < mDiffuseProbesCountY; probesY++)
		{
			for (int probesX = 0; probesX < mDiffuseProbesCountX; probesX++)
			{
				for (int probesZ = 0; probesZ < mDiffuseProbesCountZ; probesZ++)
				{
					XMFLOAT3 pos = XMFLOAT3(
						minBounds.x + probesX * mDistanceBetweenDiffuseProbes,
						minBounds.y + probesY * mDistanceBetweenDiffuseProbes,
						minBounds.z + probesZ * mDistanceBetweenDiffuseProbes);
					int index = probesY * (mDiffuseProbesCountX * mDiffuseProbesCountZ) + probesX * mDiffuseProbesCountZ + probesZ;
					mDiffuseProbes[index].SetPosition(pos);
					mDiffuseProbes[index].SetShaderInfoForConvolution(mConvolutionPS);
					AddProbeToCells(mDiffuseProbes[index], DIFFUSE_PROBE, minBounds, maxBounds);
				}
			}
		}

		// all probes positions GPU buffer
		{
			XMFLOAT4* diffuseProbesPositionsCPUBuffer = new XMFLOAT4[mDiffuseProbesCountTotal];
			for (int probeIndex = 0; probeIndex < mDiffuseProbesCountTotal; probeIndex++)
				diffuseProbesPositionsCPUBuffer[probeIndex] = XMFLOAT4(mDiffuseProbes[probeIndex].GetPosition().x, mDiffuseProbes[probeIndex].GetPosition().y, mDiffuseProbes[probeIndex].GetPosition().z, 1.0);

			mDiffuseProbesPositionsGPUBuffer = rhi->CreateGPUBuffer("ER_RHI_GPUBuffer: diffuse probes positions buffer");
			mDiffuseProbesPositionsGPUBuffer->CreateGPUBufferResource(rhi, diffuseProbesPositionsCPUBuffer, mDiffuseProbesCountTotal, sizeof(XMFLOAT4), mIsPlacedOnTerrain, ER_BIND_SHADER_RESOURCE, 0, ER_RESOURCE_MISC_BUFFER_STRUCTURED);

			if (mIsPlacedOnTerrain)
			{
				mDiffuseInputPositionsOnTerrainBuffer = rhi->CreateGPUBuffer("ER_RHI_GPUBuffer: On-terrain placement input positions buffer (diffuse probes)");
				mDiffuseInputPositionsOnTerrainBuffer->CreateGPUBufferResource(rhi, diffuseProbesPositionsCPUBuffer, mDiffuseProbesCountTotal, sizeof(XMFLOAT4), false , ER_BIND_UNORDERED_ACCESS, 0, ER_RESOURCE_MISC_BUFFER_STRUCTURED);

				mDiffuseOutputPositionsOnTerrainBuffer = rhi->CreateGPUBuffer("ER_RHI_GPUBuffer: On-terrain placement output positions buffer (diffuse probes)");
				mDiffuseOutputPositionsOnTerrainBuffer->CreateGPUBufferResource(rhi, diffuseProbesPositionsCPUBuffer, mDiffuseProbesCountTotal, sizeof(XMFLOAT4), false, ER_BIND_NONE, 0x10000L | 0x20000L /*legacy from DX11*/, ER_RESOURCE_MISC_BUFFER_STRUCTURED); //should be STAGING

				PlaceProbesOnTerrain(core, mDiffuseOutputPositionsOnTerrainBuffer, mDiffuseInputPositionsOnTerrainBuffer, diffuseProbesPositionsCPUBuffer, mDiffuseProbesCountTotal, mTerrainPlacementHeightDeltaDiffuseProbes);

				rhi->UpdateBuffer(mDiffuseProbesPositionsGPUBuffer, (void*)diffuseProbesPositionsCPUBuffer, mDiffuseProbesCountTotal * sizeof(XMFLOAT4));

				for (int i = 0; i < mDiffuseProbesCountTotal; i++)
					mDiffuseProbes[i].SetPosition(XMFLOAT3(diffuseProbesPositionsCPUBuffer[i].x, diffuseProbesPositionsCPUBuffer[i].y, diffuseProbesPositionsCPUBuffer[i].z));

				DeleteObject(mDiffuseInputPositionsOnTerrainBuffer);
				DeleteObject(mDiffuseOutputPositionsOnTerrainBuffer);
			}

			DeleteObjects(diffuseProbesPositionsCPUBuffer);
		}

		// probe cell's indices GPU buffer, tex. array indices GPU/CPU buffers
		int* diffuseProbeCellsIndicesCPUBuffer = new int[mDiffuseProbesCellsCountTotal * mCurrentProbeCountPerCell];
		for (int probeIndex = 0; probeIndex < mDiffuseProbesCellsCountTotal; probeIndex++)
		{
			for (int indices = 0; indices < mCurrentProbeCountPerCell; indices++)
			{
				if (indices >= mDiffuseProbesCells[probeIndex].lightProbeIndices.size())
					diffuseProbeCellsIndicesCPUBuffer[probeIndex * mCurrentProbeCountPerCell + indices] = -1;
				else
					diffuseProbeCellsIndicesCPUBuffer[probeIndex * mCurrentProbeCountPerCell + indices] = mDiffuseProbesCells[probeIndex].lightProbeIndices[indices];
			}
		}
		mDiffuseProbesCellsIndicesGPUBuffer = rhi->CreateGPUBuffer("ER_RHI_GPUBuffer: diffuse probes cells indices buffer");
		mDiffuseProbesCellsIndicesGPUBuffer->CreateGPUBufferResource(rhi, diffuseProbeCellsIndicesCPUBuffer, mDiffuseProbesCellsCountTotal * mCurrentProbeCountPerCell, sizeof(int), false, ER_BIND_SHADER_RESOURCE, 0, ER_RESOURCE_MISC_BUFFER_STRUCTURED);
		DeleteObjects(diffuseProbeCellsIndicesCPUBuffer);
		
		std::string name = "Debug diffuse lightprobes ";
		scene->objects.emplace_back(name, new ER_RenderingObject(name, scene->objects.size(), core, camera,
			ER_Utility::GetFilePath("content\\models\\sphere_lowpoly.fbx"), false, true));

		MaterialShaderEntries shaderEntries;
		shaderEntries.vertexEntry += "_instancing";

		mDiffuseProbeRenderingObject = scene->objects[static_cast<int>(scene->objects.size()) - 1].second;
		mDiffuseProbeRenderingObject->LoadMaterial(new ER_DebugLightProbeMaterial(core, shaderEntries, HAS_VERTEX_SHADER | HAS_PIXEL_SHADER, true), ER_MaterialHelper::debugLightProbeMaterialName);
		mDiffuseProbeRenderingObject->LoadRenderBuffers();
		mDiffuseProbeRenderingObject->LoadInstanceBuffers();
		mDiffuseProbeRenderingObject->ResetInstanceData(mDiffuseProbesCountTotal, true);
		for (int i = 0; i < mDiffuseProbesCountTotal; i++) {
			const XMFLOAT3& pos = mDiffuseProbes[i].GetPosition();
			XMMATRIX worldT = XMMatrixTranslation(pos.x, pos.y, pos.z);
			mDiffuseProbeRenderingObject->AddInstanceData(worldT);
		}
		mDiffuseProbeRenderingObject->UpdateInstanceBuffer(mDiffuseProbeRenderingObject->GetInstancesData());
		std::partition(scene->objects.begin(), scene->objects.end(), [](const ER_SceneObject& obj) {	return obj.second->IsInstanced(); });
	}

	void ER_LightProbesManager::SetupSpecularProbes(ER_Core& game, ER_Camera& camera, ER_Scene* scene, ER_DirectionalLight* light, ER_ShadowMapper* shadowMapper)
	{
		ER_RHI* rhi = game.GetRHI();

		ER_MaterialSystems materialSystems;
		materialSystems.mCamera = &camera;
		materialSystems.mDirectionalLight = light;
		materialSystems.mShadowMapper = shadowMapper;
		materialSystems.mProbesManager = this;

		XMFLOAT3& minBounds = mSceneProbesMinBounds;
		XMFLOAT3& maxBounds = mSceneProbesMaxBounds;
		
		SetupGlobalSpecularProbe(game, camera, scene, light, shadowMapper);

		mSpecularProbesCountX = (maxBounds.x - minBounds.x) / mDistanceBetweenSpecularProbes + 1;
		if (!mIs2DCellsGrid)
			mSpecularProbesCountY = (maxBounds.y - minBounds.y) / mDistanceBetweenSpecularProbes + 1;
		else
			mSpecularProbesCountY = 1;
		mSpecularProbesCountZ = (maxBounds.z - minBounds.z) / mDistanceBetweenSpecularProbes + 1;
		mSpecularProbesCountTotal = mSpecularProbesCountX * mSpecularProbesCountY * mSpecularProbesCountZ;
		assert(mSpecularProbesCountTotal);
		mSpecularProbes.reserve(mSpecularProbesCountTotal);

		// cells setup
		mSpecularProbesCellsCountX = (mSpecularProbesCountX - 1);
		mSpecularProbesCellsCountZ = (mSpecularProbesCountZ - 1);
		if (!mIs2DCellsGrid)
		{
			mSpecularProbesCellsCountY = (mSpecularProbesCountY - 1);
			mSpecularProbesCellsCountTotal = mSpecularProbesCellsCountX * mSpecularProbesCellsCountY * mSpecularProbesCellsCountZ;
		}
		else
		{
			mSpecularProbesCellsCountY = 1;
			mSpecularProbesCellsCountTotal = mSpecularProbesCellsCountX * mSpecularProbesCellsCountZ;
		}
		mSpecularProbesCells.resize(mSpecularProbesCellsCountTotal, {});
		assert(mSpecularProbesCellsCountTotal);

		float probeCellPositionOffset = static_cast<float>(mDistanceBetweenSpecularProbes) / 2.0f;
		mSpecularProbesCellBounds = {
			XMFLOAT3(-probeCellPositionOffset, mIs2DCellsGrid ? 0.0f : -probeCellPositionOffset, -probeCellPositionOffset),
			XMFLOAT3(probeCellPositionOffset, mIs2DCellsGrid ? 0.0f : probeCellPositionOffset, probeCellPositionOffset) };
		
		for (int cellsY = 0; cellsY < mSpecularProbesCellsCountY; cellsY++)
		{
			for (int cellsX = 0; cellsX < mSpecularProbesCellsCountX; cellsX++)
			{
				for (int cellsZ = 0; cellsZ < mSpecularProbesCellsCountZ; cellsZ++)
				{
					XMFLOAT3 pos = XMFLOAT3(
						minBounds.x + probeCellPositionOffset + cellsX * mDistanceBetweenSpecularProbes,
						minBounds.y + mIs2DCellsGrid ? 0.0f : probeCellPositionOffset + cellsY * mDistanceBetweenSpecularProbes,
						minBounds.z + probeCellPositionOffset + cellsZ * mDistanceBetweenSpecularProbes);

					int index = cellsY * (mSpecularProbesCellsCountX * mSpecularProbesCellsCountZ) + cellsX * mSpecularProbesCellsCountZ + cellsZ;
					mSpecularProbesCells[index].index = index;
					mSpecularProbesCells[index].position = pos;
				}
			}
		}

		// simple 3D grid distribution of probes or 2D grid (i.e. when placeable on terrain)
		for (size_t i = 0; i < mSpecularProbesCountTotal; i++)
			mSpecularProbes.emplace_back(game, light, shadowMapper, SPECULAR_PROBE_SIZE, SPECULAR_PROBE, i);

		for (int probesY = 0; probesY < mSpecularProbesCountY; probesY++)
		{
			for (int probesX = 0; probesX < mSpecularProbesCountX; probesX++)
			{
				for (int probesZ = 0; probesZ < mSpecularProbesCountZ; probesZ++)
				{
					XMFLOAT3 pos = XMFLOAT3(
						minBounds.x + probesX * mDistanceBetweenSpecularProbes,
						minBounds.y + probesY * mDistanceBetweenSpecularProbes,
						minBounds.z + probesZ * mDistanceBetweenSpecularProbes);
					int index = probesY * (mSpecularProbesCountX * mSpecularProbesCountZ) + probesX * mSpecularProbesCountZ + probesZ;
					//mSpecularProbes[index]->SetIndex(index);
					mSpecularProbes[index].SetPosition(pos);
					mSpecularProbes[index].SetShaderInfoForConvolution(mConvolutionPS);
					AddProbeToCells(mSpecularProbes[index], SPECULAR_PROBE, minBounds, maxBounds);
				}
			}
		}

		// all probes positions GPU buffer
		{
			XMFLOAT4* specularProbesPositionsCPUBuffer = new XMFLOAT4[mSpecularProbesCountTotal];
		
			for (int probeIndex = 0; probeIndex < mSpecularProbesCountTotal; probeIndex++)
				specularProbesPositionsCPUBuffer[probeIndex] = XMFLOAT4(mSpecularProbes[probeIndex].GetPosition().x,mSpecularProbes[probeIndex].GetPosition().y,mSpecularProbes[probeIndex].GetPosition().z, 1.0);
			mSpecularProbesPositionsGPUBuffer = rhi->CreateGPUBuffer("ER_RHI_GPUBuffer: specular probes positions buffer");
			mSpecularProbesPositionsGPUBuffer->CreateGPUBufferResource(rhi, specularProbesPositionsCPUBuffer, mSpecularProbesCountTotal, sizeof(XMFLOAT4), mIsPlacedOnTerrain, ER_BIND_SHADER_RESOURCE, 0, ER_RESOURCE_MISC_BUFFER_STRUCTURED);
			
			if (mIsPlacedOnTerrain)
			{
				mSpecularInputPositionsOnTerrainBuffer = rhi->CreateGPUBuffer("ER_RHI_GPUBuffer: On-terrain placement input positions buffer (specular probes)");
				mSpecularInputPositionsOnTerrainBuffer->CreateGPUBufferResource(rhi, specularProbesPositionsCPUBuffer, mSpecularProbesCountTotal, sizeof(XMFLOAT4), false, ER_BIND_UNORDERED_ACCESS, 0, ER_RESOURCE_MISC_BUFFER_STRUCTURED);

				mSpecularOutputPositionsOnTerrainBuffer = rhi->CreateGPUBuffer("ER_RHI_GPUBuffer: On-terrain placement output positions buffer (specular probes)");
				mSpecularOutputPositionsOnTerrainBuffer->CreateGPUBufferResource(rhi, specularProbesPositionsCPUBuffer, mSpecularProbesCountTotal, sizeof(XMFLOAT4), false, ER_BIND_NONE, 0x10000L | 0x20000L /*legacy from DX11*/, ER_RESOURCE_MISC_BUFFER_STRUCTURED); //should be STAGING

				PlaceProbesOnTerrain(game, mSpecularOutputPositionsOnTerrainBuffer, mSpecularInputPositionsOnTerrainBuffer, specularProbesPositionsCPUBuffer, mSpecularProbesCountTotal, mTerrainPlacementHeightDeltaSpecularProbes);

				rhi->UpdateBuffer(mSpecularProbesPositionsGPUBuffer, (void*)specularProbesPositionsCPUBuffer, mSpecularProbesCountTotal * sizeof(XMFLOAT4));

				for (int i = 0; i < mSpecularProbesCountTotal; i++)
					mSpecularProbes[i].SetPosition(XMFLOAT3(specularProbesPositionsCPUBuffer[i].x, specularProbesPositionsCPUBuffer[i].y, specularProbesPositionsCPUBuffer[i].z));

				DeleteObject(mSpecularInputPositionsOnTerrainBuffer);
				DeleteObject(mSpecularOutputPositionsOnTerrainBuffer);
			}

			DeleteObjects(specularProbesPositionsCPUBuffer);
		}

		// probe cell's indices GPU buffer, tex. array indices GPU/CPU buffers
		int* specularProbeCellsIndicesCPUBuffer = new int[mSpecularProbesCellsCountTotal * mCurrentProbeCountPerCell];
		for (int probeIndex = 0; probeIndex < mSpecularProbesCellsCountTotal; probeIndex++)
		{
			for (int indices = 0; indices < mCurrentProbeCountPerCell; indices++)
			{
				if (indices >= mSpecularProbesCells[probeIndex].lightProbeIndices.size())
					specularProbeCellsIndicesCPUBuffer[probeIndex * mCurrentProbeCountPerCell + indices] = -1;
				else
					specularProbeCellsIndicesCPUBuffer[probeIndex * mCurrentProbeCountPerCell + indices] = mSpecularProbesCells[probeIndex].lightProbeIndices[indices];
			}
		}
		mSpecularProbesCellsIndicesGPUBuffer = rhi->CreateGPUBuffer("ER_RHI_GPUBuffer: specular probes cells indices buffer");
		mSpecularProbesCellsIndicesGPUBuffer->CreateGPUBufferResource(rhi, specularProbeCellsIndicesCPUBuffer,	mSpecularProbesCellsCountTotal * mCurrentProbeCountPerCell, sizeof(int), false, ER_BIND_SHADER_RESOURCE, 0, ER_RESOURCE_MISC_BUFFER_STRUCTURED);
		DeleteObjects(specularProbeCellsIndicesCPUBuffer);

		mSpecularProbesTexArrayIndicesCPUBuffer = new int[mSpecularProbesCountTotal];
		mSpecularProbesTexArrayIndicesGPUBuffer = rhi->CreateGPUBuffer("ER_RHI_GPUBuffer: specular probes texture array indices buffer");
		mSpecularProbesTexArrayIndicesGPUBuffer->CreateGPUBufferResource(rhi, mSpecularProbesTexArrayIndicesCPUBuffer, mSpecularProbesCountTotal, sizeof(int), true, ER_BIND_SHADER_RESOURCE, 0, ER_RESOURCE_MISC_BUFFER_STRUCTURED);

		std::string name = "Debug specular lightprobes ";
		scene->objects.emplace_back(name, new ER_RenderingObject(name, scene->objects.size(), game, camera,
			ER_Utility::GetFilePath("content\\models\\sphere_lowpoly.fbx"), false, true));
		
		MaterialShaderEntries shaderEntries;
		shaderEntries.vertexEntry += "_instancing";

		mSpecularProbeRenderingObject = scene->objects[static_cast<int>(scene->objects.size()) - 1].second;
		mSpecularProbeRenderingObject->LoadMaterial(new ER_DebugLightProbeMaterial(game, shaderEntries, HAS_VERTEX_SHADER | HAS_PIXEL_SHADER, true), ER_MaterialHelper::debugLightProbeMaterialName);
		mSpecularProbeRenderingObject->LoadRenderBuffers();
		mSpecularProbeRenderingObject->LoadInstanceBuffers();
		mSpecularProbeRenderingObject->ResetInstanceData(mSpecularProbesCountTotal, true);
		for (int i = 0; i < mSpecularProbesCountTotal; i++) {
			const XMFLOAT3& pos = mSpecularProbes[i].GetPosition();
			XMMATRIX worldT = XMMatrixTranslation(pos.x, pos.y, pos.z);
			mSpecularProbeRenderingObject->AddInstanceData(worldT);
		}
		mSpecularProbeRenderingObject->UpdateInstanceBuffer(mSpecularProbeRenderingObject->GetInstancesData());
		std::partition(scene->objects.begin(), scene->objects.end(), [](const ER_SceneObject& obj) {	return obj.second->IsInstanced(); });

		mSpecularCubemapArrayRT = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Specular Cubemap Array RT");
		mSpecularCubemapArrayRT->CreateGPUTextureResource(rhi, SPECULAR_PROBE_SIZE, SPECULAR_PROBE_SIZE, 1, ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_SHADER_RESOURCE, SPECULAR_PROBE_MIP_COUNT, -1, CUBEMAP_FACES_COUNT, true, mMaxSpecularProbesInVolumeCount);
	}

	void ER_LightProbesManager::AddProbeToCells(ER_LightProbe& aProbe, ER_ProbeType aType, const XMFLOAT3& minBounds, const XMFLOAT3& maxBounds)
	{
		int index = aProbe.GetIndex();
		if (aType == DIFFUSE_PROBE)
		{
			// brute force O(cells * probes)
			for (auto& cell : mDiffuseProbesCells)
			{
				if (IsProbeInCell(aProbe, cell, mDiffuseProbesCellBounds))
					cell.lightProbeIndices.push_back(index);

				if (cell.lightProbeIndices.size() > mCurrentProbeCountPerCell)
					throw ER_CoreException("Too many diffuse probes per cell!");
			}
		}
		else
		{
			// brute force O(cells * probes)
			for (auto& cell : mSpecularProbesCells)
			{
				if (IsProbeInCell(aProbe, cell, mSpecularProbesCellBounds))
					cell.lightProbeIndices.push_back(index);

				if (cell.lightProbeIndices.size() > mCurrentProbeCountPerCell)
					throw ER_CoreException("Too many specular probes per cell!");
			}
		}
	}

	// Fast uniform-grid searching approach (WARNING: can not do multiple indices per pos. (i.e., when pos. is on the edge of several cells))
	int ER_LightProbesManager::GetCellIndex(const XMFLOAT3& pos, ER_ProbeType aType)
	{
		int finalIndex = -1;

		if (aType == DIFFUSE_PROBE)
		{
			int xIndex = floor((pos.x - mSceneProbesMinBounds.x) / static_cast<float>(mDistanceBetweenDiffuseProbes));
			int yIndex = mIs2DCellsGrid ? 0 : floor((pos.y - mSceneProbesMinBounds.y) / static_cast<float>(mDistanceBetweenDiffuseProbes));
			int zIndex = floor((pos.z - mSceneProbesMinBounds.z) / static_cast<float>(mDistanceBetweenDiffuseProbes));

			if (xIndex < 0 || xIndex > mDiffuseProbesCellsCountX)
				return -1;
			if (yIndex < 0 || yIndex > mDiffuseProbesCellsCountY)
				return -1;
			if (zIndex < 0 || zIndex > mDiffuseProbesCellsCountZ)
				return -1;

			//little hacky way to prevent from out-of-bounds
			if (xIndex == mDiffuseProbesCellsCountX)
				xIndex = mDiffuseProbesCellsCountX - 1;
			if (yIndex == mDiffuseProbesCellsCountY)
				yIndex = mDiffuseProbesCellsCountY - 1;
			if (zIndex == mDiffuseProbesCellsCountZ)
				zIndex = mDiffuseProbesCellsCountZ - 1;

			finalIndex = yIndex * (mDiffuseProbesCellsCountX * mDiffuseProbesCellsCountZ) + xIndex * mDiffuseProbesCellsCountZ + zIndex;

			if (finalIndex >= mDiffuseProbesCellsCountTotal)
				return -1;
		}
		else
		{
			int xIndex = floor((pos.x - mSceneProbesMinBounds.x) / static_cast<float>(mDistanceBetweenSpecularProbes));
			int yIndex = mIs2DCellsGrid ? 0 : floor((pos.y - mSceneProbesMinBounds.y) / static_cast<float>(mDistanceBetweenSpecularProbes));
			int zIndex = floor((pos.z - mSceneProbesMinBounds.z) / static_cast<float>(mDistanceBetweenSpecularProbes));

			if (xIndex < 0 || xIndex > mSpecularProbesCellsCountX)
				return -1;
			if (yIndex < 0 || yIndex > mSpecularProbesCellsCountY)
				return -1;
			if (zIndex < 0 || zIndex > mSpecularProbesCellsCountZ)
				return -1;

			//little hacky way to prevent from out-of-bounds
			if (xIndex == mSpecularProbesCellsCountX)
				xIndex = mSpecularProbesCellsCountX - 1;
			if (yIndex == mSpecularProbesCellsCountY)
				yIndex = mSpecularProbesCellsCountY - 1;
			if (zIndex == mSpecularProbesCellsCountZ)
				zIndex = mSpecularProbesCellsCountZ - 1;

			finalIndex = yIndex * (mSpecularProbesCellsCountX * mSpecularProbesCellsCountZ) + xIndex * mSpecularProbesCellsCountZ + zIndex;

			if (finalIndex >= mSpecularProbesCellsCountTotal)
				return -1;

		}

		return finalIndex;
	}

	XMFLOAT4 ER_LightProbesManager::GetProbesCellsCount(ER_ProbeType aType)
	{
		if (aType == DIFFUSE_PROBE)
			return XMFLOAT4(mDiffuseProbesCellsCountX, mDiffuseProbesCellsCountY, mDiffuseProbesCellsCountZ, mDiffuseProbesCellsCountTotal);
		else
			return XMFLOAT4(mSpecularProbesCellsCountX, mSpecularProbesCellsCountY, mSpecularProbesCellsCountZ, mSpecularProbesCellsCountTotal);
	}

	bool ER_LightProbesManager::IsProbeInCell(ER_LightProbe& aProbe, ER_LightProbeCell& aCell, ER_AABB& aCellBounds)
	{
		XMFLOAT3 pos = aProbe.GetPosition();
		float epsilon = 0.00001f;

		XMFLOAT3 maxBounds = XMFLOAT3(
			aCellBounds.second.x + aCell.position.x,
			aCellBounds.second.y + aCell.position.y,
			aCellBounds.second.z + aCell.position.z);

		XMFLOAT3 minBounds = XMFLOAT3(
			aCellBounds.first.x + aCell.position.x,
			aCellBounds.first.y + aCell.position.y,
			aCellBounds.first.z + aCell.position.z);

		if (mIs2DCellsGrid)
		{
			return	
				(pos.x <= (maxBounds.x + epsilon) && pos.x >= (minBounds.x - epsilon)) &&
				(pos.z <= (maxBounds.z + epsilon) && pos.z >= (minBounds.z - epsilon));
		}
		else
		{
			return	
				(pos.x <= (maxBounds.x + epsilon) && pos.x >= (minBounds.x - epsilon)) &&
				(pos.y <= (maxBounds.y + epsilon) && pos.y >= (minBounds.y - epsilon)) &&
				(pos.z <= (maxBounds.z + epsilon) && pos.z >= (minBounds.z - epsilon));
		}
	}

	void ER_LightProbesManager::ComputeOrLoadGlobalProbes(ER_Core& game, ProbesRenderingObjectsInfo& aObjects)
	{
		// DIFFUSE_PROBE
		{
			if (mGlobalDiffuseProbeReady)
				return;

			std::wstring diffuseProbesPath = mLevelPath + L"diffuse_probes\\";
			if (!mGlobalDiffuseProbe->LoadProbeFromDisk(game, diffuseProbesPath))
			{
				if (game.GetRHI()->GetAPI() == ER_GRAPHICS_API::DX11)
					mGlobalDiffuseProbe->Compute(game, mTempDiffuseCubemapFacesRT, mTempDiffuseCubemapFacesConvolutedRT, mTempDiffuseCubemapDepthBuffers, diffuseProbesPath, aObjects, mQuadRenderer);
				else
					throw ER_CoreException("ER_LightProbesManager: Computing & saving the probes is only possible on DX11 at the moment");
			}

			mGlobalDiffuseProbeReady = true;
		}
		// SPECULAR_PROBE
		{
			if (mGlobalSpecularProbeReady)
				return;

			std::wstring specularProbesPath = mLevelPath + L"specular_probes\\";

			if (!mGlobalSpecularProbe->LoadProbeFromDisk(game, specularProbesPath))
			{
				if (game.GetRHI()->GetAPI() == ER_GRAPHICS_API::DX11)
					mGlobalSpecularProbe->Compute(game, mTempSpecularCubemapFacesRT, mTempSpecularCubemapFacesConvolutedRT, mTempSpecularCubemapDepthBuffers, specularProbesPath, aObjects, mQuadRenderer);
				else
					throw ER_CoreException("ER_LightProbesManager: Computing & saving the probes is only possible on DX11 at the moment");
			}
			//else
			//	throw ER_CoreException("ER_LightProbesManager: Could not load or compute a global specular probe");

			mGlobalSpecularProbeReady = true;
		}
	}

	void ER_LightProbesManager::ComputeOrLoadLocalProbes(ER_Core& game, ProbesRenderingObjectsInfo& aObjects)
	{
		ER_RHI* rhi = game.GetRHI();

		int numThreads = std::thread::hardware_concurrency();
		if (game.GetRHI()->GetAPI() != ER_GRAPHICS_API::DX11)
			numThreads = 1; //TODO fix this on DX12 (need to support multiple command lists)

		assert(numThreads > 0);

		if (mDistanceBetweenDiffuseProbes <= 0.0)
			mDiffuseProbesReady = true;

		if (!mDiffuseProbesReady && mDistanceBetweenDiffuseProbes > 0)
		{
			std::wstring diffuseProbesPath = mLevelPath + L"diffuse_probes\\";
			
			std::vector<std::thread> threads;
			threads.reserve(numThreads);

			int probesPerThread = mDiffuseProbes.size() / numThreads;

			for (int i = 0; i < numThreads; i++)
			{
				threads.push_back(std::thread([&, diffuseProbesPath, i]
				{ 
					int endRange = (i < numThreads - 1) ? (i + 1) * probesPerThread : mDiffuseProbes.size();
					for (int j = i * probesPerThread; j < endRange; j++)
						mDiffuseProbes[j].LoadProbeFromDisk(game, diffuseProbesPath);
				}));
			}
			for (auto& t : threads) t.join();

			for (auto& probe : mDiffuseProbes)
			{
				if (!probe.IsLoadedFromDisk())
				{
					if (game.GetRHI()->GetAPI() == ER_GRAPHICS_API::DX11)
						probe.Compute(game, mTempDiffuseCubemapFacesRT, mTempDiffuseCubemapFacesConvolutedRT, mTempDiffuseCubemapDepthBuffers, diffuseProbesPath, aObjects, mQuadRenderer);
					else
						throw ER_CoreException("ER_LightProbesManager: Computing & saving the probes is only possible on DX11 at the moment");
				}
				else
				{
					//TODO load empty
				}
			}
			
			mDiffuseProbesReady = true;

			UpdateProbesByType(game, DIFFUSE_PROBE);

			// SH GPU buffer
			XMFLOAT3* shCPUBuffer = new XMFLOAT3[mDiffuseProbesCountTotal * SPHERICAL_HARMONICS_COEF_COUNT];
			for (int probeIndex = 0; probeIndex < mDiffuseProbesCountTotal; probeIndex++)
			{
				const std::vector<XMFLOAT3>& sh = mDiffuseProbes[probeIndex].GetSphericalHarmonics();
				for (int i = 0; i < SPHERICAL_HARMONICS_COEF_COUNT; i++)
					shCPUBuffer[probeIndex * SPHERICAL_HARMONICS_COEF_COUNT + i] = sh[i];
			}
			mDiffuseProbesSphericalHarmonicsGPUBuffer = rhi->CreateGPUBuffer("ER_RHI_GPUBuffer: diffuse probes SH buffer");
			mDiffuseProbesSphericalHarmonicsGPUBuffer->CreateGPUBufferResource(rhi, shCPUBuffer, mDiffuseProbesCountTotal* SPHERICAL_HARMONICS_COEF_COUNT, sizeof(XMFLOAT3),
				false, ER_BIND_SHADER_RESOURCE, 0, ER_RESOURCE_MISC_BUFFER_STRUCTURED);
			DeleteObjects(shCPUBuffer);
		}

		if (mDistanceBetweenSpecularProbes <= 0.0)
			mSpecularProbesReady = true;
		if (!mSpecularProbesReady && mDistanceBetweenSpecularProbes > 0)
		{
			std::wstring specularProbesPath = mLevelPath + L"specular_probes\\";

			std::vector<std::thread> threads;
			int numThreads = std::thread::hardware_concurrency();
			if (game.GetRHI()->GetAPI() != ER_GRAPHICS_API::DX11)
				numThreads = 1; //TODO fix this on DX12 (need to support multiple command lists)

			threads.reserve(numThreads);

			int probesPerThread = mSpecularProbes.size() / numThreads;

			for (int i = 0; i < numThreads; i++)
			{
				threads.push_back(std::thread([&, specularProbesPath, i]
				{
					int endRange = (i < numThreads - 1) ? (i + 1) * probesPerThread : mSpecularProbes.size();
					for (int j = i * probesPerThread; j < endRange; j++)
						mSpecularProbes[j].LoadProbeFromDisk(game, specularProbesPath);
				}));
			}
			for (auto& t : threads) t.join();

			for (auto& probe : mSpecularProbes)
			{
				if (!probe.IsLoadedFromDisk())
				{
					if (game.GetRHI()->GetAPI() == ER_GRAPHICS_API::DX11)
						probe.Compute(game, mTempSpecularCubemapFacesRT, mTempSpecularCubemapFacesConvolutedRT, mTempSpecularCubemapDepthBuffers, specularProbesPath, aObjects, mQuadRenderer);
					else
						throw ER_CoreException("ER_LightProbesManager: Computing & saving the probes is only possible on DX11 at the moment");

				}
				else
				{
					//TODO load empty
				}
			}
			mSpecularProbesReady = true;
		}
	}

	void ER_LightProbesManager::DrawDebugProbes(ER_RHI* rhi, ER_RHI_GPUTexture* aRenderTarget, ER_RHI_GPUTexture* aDepth, ER_ProbeType aType, ER_RHI_GPURootSignature* rs)
	{
		assert(aRenderTarget);
		assert(rhi);

		ER_RenderingObject* probeObject = aType == DIFFUSE_PROBE ? mDiffuseProbeRenderingObject : mSpecularProbeRenderingObject;
		bool ready = aType == DIFFUSE_PROBE ? (mDiffuseProbesReady && mDistanceBetweenDiffuseProbes > 0) : (mSpecularProbesReady && mDistanceBetweenSpecularProbes > 0);
		const std::string psoName = DIFFUSE_PROBE ? mDiffuseDebugLightProbePassPSOName : mSpecularDebugLightProbePassPSOName;

		ER_MaterialSystems materialSystems;
		materialSystems.mProbesManager = this;

		rhi->SetRootSignature(rs);
		if (probeObject && ready)
		{
			auto materialInfo = probeObject->GetMaterials().find(ER_MaterialHelper::debugLightProbeMaterialName);
			if (materialInfo != probeObject->GetMaterials().end())
			{
				ER_DebugLightProbeMaterial* material = static_cast<ER_DebugLightProbeMaterial*>(materialInfo->second);
				if (!rhi->IsPSOReady(psoName))
				{
					rhi->InitializePSO(psoName);
					material->PrepareShaders();
					rhi->SetRenderTargetFormats({ aRenderTarget }, aDepth);
					rhi->SetRasterizerState(ER_NO_CULLING);
					rhi->SetBlendState(ER_NO_BLEND);
					rhi->SetDepthStencilState(ER_DEPTH_ONLY_WRITE_COMPARISON_LESS_EQUAL);
					rhi->SetTopologyTypeToPSO(psoName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					rhi->SetRootSignatureToPSO(psoName, rs);
					rhi->FinalizePSO(psoName);
				}
				rhi->SetPSO(psoName);
				material->PrepareForRendering(materialSystems, probeObject, 0, static_cast<int>(aType), rs);
				probeObject->Draw(ER_MaterialHelper::debugLightProbeMaterialName);
				rhi->UnsetPSO();
			}
		}

	}

	void ER_LightProbesManager::UpdateProbesByType(ER_Core& game, ER_ProbeType aType)
	{
		std::vector<ER_LightProbe>& probes = (aType == DIFFUSE_PROBE) ? mDiffuseProbes : mSpecularProbes;

		ER_RenderingObject* probeRenderingObject = (aType == DIFFUSE_PROBE) ? mDiffuseProbeRenderingObject : mSpecularProbeRenderingObject;
		if (aType == DIFFUSE_PROBE)
		{
			if (!mDiffuseProbesReady || mDistanceBetweenDiffuseProbes <= 0)
				return;
		}
		else
		{
			if (!mSpecularProbesReady || mDistanceBetweenSpecularProbes <= 0)
				return;

			mNonCulledSpecularProbesCount = 0;
			mNonCulledSpecularProbesIndices.clear();
		}

		if (aType == SPECULAR_PROBE)
		{
			XMFLOAT3 minBounds = XMFLOAT3(
				-mSpecularProbesVolumeSize + mMainCamera.Position().x,
				-mSpecularProbesVolumeSize + mMainCamera.Position().y,
				-mSpecularProbesVolumeSize + mMainCamera.Position().z);
			XMFLOAT3 maxBounds = XMFLOAT3(
				mSpecularProbesVolumeSize + mMainCamera.Position().x,
				mSpecularProbesVolumeSize + mMainCamera.Position().y,
				mSpecularProbesVolumeSize + mMainCamera.Position().z);

			for (auto& lightProbe : probes)
				lightProbe.CPUCullAgainstProbeBoundingVolume(minBounds, maxBounds);
		}

		if (probeRenderingObject)
		{
			auto& oldInstancedData = probeRenderingObject->GetInstancesData();
			assert(oldInstancedData.size() == probes.size());

			for (int i = 0; i < oldInstancedData.size(); i++)
			{
				bool isCulled = probes[i].IsCulled();
				//writing culling flag to [4][4] of world instanced matrix
				oldInstancedData[i].World._44 = isCulled ? 1.0f : 0.0f;
				//writing cubemap index to [0][0] of world instanced matrix (since we don't need scale)
				oldInstancedData[i].World._11 = -1.0f;
				if (!isCulled)
				{
					if (aType == DIFFUSE_PROBE)
						oldInstancedData[i].World._11 = static_cast<float>(i);
					else
					{
						oldInstancedData[i].World._11 = static_cast<float>(mNonCulledSpecularProbesCount);
						mNonCulledSpecularProbesCount++;
						mNonCulledSpecularProbesIndices.push_back(i);
					}
				}
			}

			probeRenderingObject->UpdateInstanceBuffer(oldInstancedData);
		}

		ER_RHI* rhi = game.GetRHI();
		if (aType == SPECULAR_PROBE)
		{
			for (int i = 0; i < mSpecularProbesCountTotal; i++)
				mSpecularProbesTexArrayIndicesCPUBuffer[i] = -1;

			for (int i = 0; i < mMaxSpecularProbesInVolumeCount; i++)
			{
				if (i < mNonCulledSpecularProbesIndices.size() && i < probes.size())
				{
					int probeIndex = mNonCulledSpecularProbesIndices[i];

					rhi->TransitionResources({ static_cast<ER_RHI_GPUResource*>(mSpecularCubemapArrayRT), static_cast<ER_RHI_GPUResource*>(mSpecularProbes[probeIndex].GetCubemapTexture()) },
						{ ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_COPY_DEST, ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_COPY_SOURCE }, rhi->GetCurrentGraphicsCommandListIndex());

					for (int cubeI = 0; cubeI < CUBEMAP_FACES_COUNT; cubeI++)
					{
						for (int mip = 0; mip < SPECULAR_PROBE_MIP_COUNT; mip++)
						{
							rhi->CopyGPUTextureSubresourceRegion(mSpecularCubemapArrayRT, mip + (cubeI + CUBEMAP_FACES_COUNT * i) * SPECULAR_PROBE_MIP_COUNT, 0, 0, 0,
								mSpecularProbes[probeIndex].GetCubemapTexture(), mip + cubeI * SPECULAR_PROBE_MIP_COUNT, true);
						}
					}
					mSpecularProbesTexArrayIndicesCPUBuffer[probeIndex] = CUBEMAP_FACES_COUNT * i;

					rhi->TransitionResources({ static_cast<ER_RHI_GPUResource*>(mSpecularCubemapArrayRT), static_cast<ER_RHI_GPUResource*>(mSpecularProbes[probeIndex].GetCubemapTexture()) },
						{ ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_PIXEL_SHADER_RESOURCE }, rhi->GetCurrentGraphicsCommandListIndex());
				}
			}
			rhi->UpdateBuffer(mSpecularProbesTexArrayIndicesGPUBuffer, mSpecularProbesTexArrayIndicesCPUBuffer, sizeof(mSpecularProbesTexArrayIndicesCPUBuffer[0]) * mSpecularProbesCountTotal);
		}
	}

	void ER_LightProbesManager::UpdateProbes(ER_Core& game)
	{
		//int difProbeCellIndexCamera = GetCellIndex(mMainCamera.Position(), DIFFUSE_PROBE);
		if (mDistanceBetweenSpecularProbes > 0)
			assert(mMaxSpecularProbesInVolumeCount > 0);

		//UpdateProbesByType(game, DIFFUSE_PROBE); we do not need to update diffuse probes every frame anymore
		UpdateProbesByType(game, SPECULAR_PROBE);
	}

	void ER_LightProbesManager::PlaceProbesOnTerrain(ER_Core& game, ER_RHI_GPUBuffer* outputBuffer, ER_RHI_GPUBuffer* inputBuffer, XMFLOAT4* positions, int positionsCount, float customDampDelta /*= FLT_MAX*/)
	{
		assert(mIsPlacedOnTerrain);
		
		ER_Terrain* terrain = game.GetLevel()->mTerrain;
		if (!terrain)
			throw ER_CoreException("You want to place light probes on terrain but terrain is not found in the scene!");

		terrain->PlaceOnTerrain(outputBuffer, inputBuffer, positions, positionsCount, TerrainSplatChannels::NONE, nullptr, 0, customDampDelta);
	}
}