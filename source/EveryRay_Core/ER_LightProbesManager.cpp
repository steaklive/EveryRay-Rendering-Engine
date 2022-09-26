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
#include "ER_LightProbe.h"
#include "ER_QuadRenderer.h"
#include "ER_DebugLightProbeMaterial.h"
#include "ER_MaterialsCallbacks.h"

namespace EveryRay_Core
{

	ER_LightProbesManager::ER_LightProbesManager(ER_Core& game, ER_Camera& camera, ER_Scene* scene, ER_DirectionalLight& light, ER_ShadowMapper& shadowMapper)
		: mMainCamera(camera)
	{
		if (!scene)
			throw ER_CoreException("No scene to load light probes for!");

		ER_RHI* rhi = game.GetRHI();

		mTempDiffuseCubemapFacesRT = rhi->CreateGPUTexture();
		mTempDiffuseCubemapFacesRT->CreateGPUTextureResource(rhi, DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE_SIZE, 1, ER_FORMAT_R16G16B16A16_FLOAT,
			ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, 1, -1, CUBEMAP_FACES_COUNT, true);

		mTempDiffuseCubemapFacesConvolutedRT = rhi->CreateGPUTexture();
		mTempDiffuseCubemapFacesConvolutedRT->CreateGPUTextureResource(rhi, DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE_SIZE, 1, ER_FORMAT_R16G16B16A16_FLOAT,
			ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, 1, -1, CUBEMAP_FACES_COUNT, true);
		
		mTempSpecularCubemapFacesRT = rhi->CreateGPUTexture();
		mTempSpecularCubemapFacesRT->CreateGPUTextureResource(rhi, SPECULAR_PROBE_SIZE, SPECULAR_PROBE_SIZE, 1, ER_FORMAT_R8G8B8A8_UNORM,
			ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, SPECULAR_PROBE_MIP_COUNT, -1, CUBEMAP_FACES_COUNT, true);

		mTempSpecularCubemapFacesConvolutedRT = rhi->CreateGPUTexture(); 
		mTempSpecularCubemapFacesConvolutedRT->CreateGPUTextureResource(rhi, SPECULAR_PROBE_SIZE, SPECULAR_PROBE_SIZE, 1, ER_FORMAT_R8G8B8A8_UNORM,
			ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET, SPECULAR_PROBE_MIP_COUNT, -1, CUBEMAP_FACES_COUNT, true);

		for (int i = 0; i < CUBEMAP_FACES_COUNT; i++)
		{
			mTempDiffuseCubemapDepthBuffers[i] = rhi->CreateGPUTexture();
			mTempDiffuseCubemapDepthBuffers[i]->CreateGPUTextureResource(rhi, DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE_SIZE, 1u, ER_FORMAT_D24_UNORM_S8_UINT,ER_BIND_SHADER_RESOURCE | ER_BIND_DEPTH_STENCIL);
			
			mTempSpecularCubemapDepthBuffers[i] = rhi->CreateGPUTexture();
			mTempSpecularCubemapDepthBuffers[i]->CreateGPUTextureResource(rhi, SPECULAR_PROBE_SIZE, SPECULAR_PROBE_SIZE, 1u, ER_FORMAT_D24_UNORM_S8_UINT, ER_BIND_SHADER_RESOURCE | ER_BIND_DEPTH_STENCIL);
		}

		mQuadRenderer = (ER_QuadRenderer*)game.GetServices().FindService(ER_QuadRenderer::TypeIdClass());
		assert(mQuadRenderer);
		mConvolutionPS = rhi->CreateGPUShader();
		mConvolutionPS->CompileShader(rhi, "content\\shaders\\IBL\\ProbeConvolution.hlsl", "PSMain", ER_PIXEL);

		mIntegrationMapTextureSRV = rhi->CreateGPUTexture();
		mIntegrationMapTextureSRV->CreateGPUTextureResource(rhi, ER_Utility::GetFilePath(L"content\\textures\\IntegrationMapBrdf.dds"), true);

		if (!scene->HasLightProbesSupport())
		{
			mEnabled = false;

			SetupGlobalDiffuseProbe(game, camera, scene, light, shadowMapper);
			SetupGlobalSpecularProbe(game, camera, scene, light, shadowMapper);
			return;
		}

		mSceneProbesMinBounds = scene->GetLightProbesVolumeMinBounds();
		mSceneProbesMaxBounds = scene->GetLightProbesVolumeMaxBounds();

		mDistanceBetweenDiffuseProbes = scene->GetLightProbesDiffuseDistance();
		mDistanceBetweenSpecularProbes = scene->GetLightProbesSpecularDistance();

		if (mDistanceBetweenDiffuseProbes <= 0.0f || mDistanceBetweenSpecularProbes <= 0.0f)
			throw ER_CoreException("Loaded level has incorrect distances between probes (either diffuse or specular or both). Did you forget to assign them in the level file?");

		mSpecularProbesVolumeSize = MAX_CUBEMAPS_IN_VOLUME_PER_AXIS * mDistanceBetweenSpecularProbes * 0.5f;
		mMaxSpecularProbesInVolumeCount = MAX_CUBEMAPS_IN_VOLUME_PER_AXIS * MAX_CUBEMAPS_IN_VOLUME_PER_AXIS * MAX_CUBEMAPS_IN_VOLUME_PER_AXIS;

		SetupDiffuseProbes(game, camera, scene, light, shadowMapper);
		SetupSpecularProbes(game, camera, scene, light, shadowMapper);
	}

	ER_LightProbesManager::~ER_LightProbesManager()
	{
		DeleteObject(mGlobalDiffuseProbe);
		DeleteObject(mGlobalSpecularProbe);
		DeletePointerCollection(mDiffuseProbes);
		DeletePointerCollection(mSpecularProbes);

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

		DeleteObject(mDiffuseProbesCellsIndicesGPUBuffer);

		DeleteObject(mSpecularCubemapArrayRT);
		DeleteObject(mSpecularProbesCellsIndicesGPUBuffer);
		DeleteObjects(mSpecularProbesTexArrayIndicesCPUBuffer);
		DeleteObject(mSpecularProbesTexArrayIndicesGPUBuffer);
	}

	void ER_LightProbesManager::SetupGlobalDiffuseProbe(ER_Core& game, ER_Camera& camera, ER_Scene* scene, ER_DirectionalLight& light, ER_ShadowMapper& shadowMapper)
	{
		mGlobalDiffuseProbe = new ER_LightProbe(game, light, shadowMapper, DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE);
		mGlobalDiffuseProbe->SetIndex(-1);
		mGlobalDiffuseProbe->SetPosition(XMFLOAT3(0.0f, 2.0f, 0.0f)); //just a bit above the ground
		mGlobalDiffuseProbe->SetShaderInfoForConvolution(mConvolutionPS);
	}
	void ER_LightProbesManager::SetupGlobalSpecularProbe(ER_Core& game, ER_Camera& camera, ER_Scene* scene, ER_DirectionalLight& light, ER_ShadowMapper& shadowMapper)
	{
		mGlobalSpecularProbe = new ER_LightProbe(game, light, shadowMapper, SPECULAR_PROBE_SIZE, SPECULAR_PROBE);
		mGlobalSpecularProbe->SetIndex(-1);
		mGlobalSpecularProbe->SetPosition(XMFLOAT3(0.0f, 2.0f, 0.0f)); //just a bit above the ground
		mGlobalSpecularProbe->SetShaderInfoForConvolution(mConvolutionPS);
	}

	void ER_LightProbesManager::SetupDiffuseProbes(ER_Core& core, ER_Camera& camera, ER_Scene* scene, ER_DirectionalLight& light, ER_ShadowMapper& shadowMapper)
	{
		ER_RHI* rhi = core.GetRHI();

		ER_MaterialSystems materialSystems;
		materialSystems.mCamera = &camera;
		materialSystems.mDirectionalLight = &light;
		materialSystems.mShadowMapper = &shadowMapper;
		materialSystems.mProbesManager = this;

		XMFLOAT3& minBounds = mSceneProbesMinBounds;
		XMFLOAT3& maxBounds = mSceneProbesMaxBounds;

		SetupGlobalDiffuseProbe(core, camera, scene, light, shadowMapper);

		mDiffuseProbesCountX = (maxBounds.x - minBounds.x) / mDistanceBetweenDiffuseProbes + 1;
		mDiffuseProbesCountY = (maxBounds.y - minBounds.y) / mDistanceBetweenDiffuseProbes + 1;
		mDiffuseProbesCountZ = (maxBounds.z - minBounds.z) / mDistanceBetweenDiffuseProbes + 1;
		mDiffuseProbesCountTotal = mDiffuseProbesCountX * mDiffuseProbesCountY * mDiffuseProbesCountZ;
		assert(mDiffuseProbesCountTotal);
		mDiffuseProbes.reserve(mDiffuseProbesCountTotal);

		// cells setup
		mDiffuseProbesCellsCountX = (mDiffuseProbesCountX - 1);
		mDiffuseProbesCellsCountY = (mDiffuseProbesCountY - 1);
		mDiffuseProbesCellsCountZ = (mDiffuseProbesCountZ - 1);
		mDiffuseProbesCellsCountTotal = mDiffuseProbesCellsCountX * mDiffuseProbesCellsCountY * mDiffuseProbesCellsCountZ;
		mDiffuseProbesCells.resize(mDiffuseProbesCellsCountTotal, {});

		assert(mDiffuseProbesCellsCountTotal);

		float probeCellPositionOffset = static_cast<float>(mDistanceBetweenDiffuseProbes) / 2.0f;
		mDiffuseProbesCellBounds = {
			XMFLOAT3(-probeCellPositionOffset, -probeCellPositionOffset, -probeCellPositionOffset),
			XMFLOAT3(probeCellPositionOffset, probeCellPositionOffset, probeCellPositionOffset) };
		{
			for (int cellsY = 0; cellsY < mDiffuseProbesCellsCountY; cellsY++)
			{
				for (int cellsX = 0; cellsX < mDiffuseProbesCellsCountX; cellsX++)
				{
					for (int cellsZ = 0; cellsZ < mDiffuseProbesCellsCountZ; cellsZ++)
					{
						XMFLOAT3 pos = XMFLOAT3(
							minBounds.x + probeCellPositionOffset + cellsX * mDistanceBetweenDiffuseProbes,
							minBounds.y + probeCellPositionOffset + cellsY * mDistanceBetweenDiffuseProbes,
							minBounds.z + probeCellPositionOffset + cellsZ * mDistanceBetweenDiffuseProbes);

						int index = cellsY * (mDiffuseProbesCellsCountX * mDiffuseProbesCellsCountZ) + cellsX * mDiffuseProbesCellsCountZ + cellsZ;
						mDiffuseProbesCells[index].index = index;
						mDiffuseProbesCells[index].position = pos;
					}
				}
			}
		}

		// simple 3D grid distribution of probes
		for (size_t i = 0; i < mDiffuseProbesCountTotal; i++)
			mDiffuseProbes.emplace_back(new ER_LightProbe(core, light, shadowMapper, DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE));
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
					mDiffuseProbes[index]->SetIndex(index);
					mDiffuseProbes[index]->SetPosition(pos);
					mDiffuseProbes[index]->SetShaderInfoForConvolution(mConvolutionPS);
					AddProbeToCells(mDiffuseProbes[index], DIFFUSE_PROBE, minBounds, maxBounds);
				}
			}
		}

		// all probes positions GPU buffer
		XMFLOAT3* diffuseProbesPositionsCPUBuffer = new XMFLOAT3[mDiffuseProbesCountTotal];
		for (int probeIndex = 0; probeIndex < mDiffuseProbesCountTotal; probeIndex++)
			diffuseProbesPositionsCPUBuffer[probeIndex] = mDiffuseProbes[probeIndex]->GetPosition();
		mDiffuseProbesPositionsGPUBuffer = rhi->CreateGPUBuffer();
		mDiffuseProbesPositionsGPUBuffer->CreateGPUBufferResource(rhi, diffuseProbesPositionsCPUBuffer, mDiffuseProbesCountTotal, sizeof(XMFLOAT3), false, ER_BIND_SHADER_RESOURCE, 0, ER_RESOURCE_MISC_BUFFER_STRUCTURED);
		DeleteObjects(diffuseProbesPositionsCPUBuffer);

		// probe cell's indices GPU buffer, tex. array indices GPU/CPU buffers
		int* diffuseProbeCellsIndicesCPUBuffer = new int[mDiffuseProbesCellsCountTotal * PROBE_COUNT_PER_CELL];
		for (int probeIndex = 0; probeIndex < mDiffuseProbesCellsCountTotal; probeIndex++)
		{
			for (int indices = 0; indices < PROBE_COUNT_PER_CELL; indices++)
			{
				if (indices >= mDiffuseProbesCells[probeIndex].lightProbeIndices.size())
					diffuseProbeCellsIndicesCPUBuffer[probeIndex * PROBE_COUNT_PER_CELL + indices] = -1;
				else
					diffuseProbeCellsIndicesCPUBuffer[probeIndex * PROBE_COUNT_PER_CELL + indices] = mDiffuseProbesCells[probeIndex].lightProbeIndices[indices];
			}
		}
		mDiffuseProbesCellsIndicesGPUBuffer = rhi->CreateGPUBuffer();
		mDiffuseProbesCellsIndicesGPUBuffer->CreateGPUBufferResource(rhi, diffuseProbeCellsIndicesCPUBuffer, mDiffuseProbesCellsCountTotal * PROBE_COUNT_PER_CELL, sizeof(int), false, ER_BIND_SHADER_RESOURCE, 0, ER_RESOURCE_MISC_BUFFER_STRUCTURED);
		DeleteObjects(diffuseProbeCellsIndicesCPUBuffer);
		
		std::string name = "Debug diffuse lightprobes ";
		auto result = scene->objects.insert(
			std::pair<std::string, ER_RenderingObject*>(name, new ER_RenderingObject(name, scene->objects.size(), core, camera,
				std::unique_ptr<ER_Model>(new ER_Model(core, ER_Utility::GetFilePath("content\\models\\sphere_lowpoly.fbx"), true)), false, true))
		);

		if (!result.second)
			throw ER_CoreException("Could not add a diffuse probe global object into scene");

		MaterialShaderEntries shaderEntries;
		shaderEntries.vertexEntry += "_instancing";

		mDiffuseProbeRenderingObject = result.first->second;
		mDiffuseProbeRenderingObject->LoadMaterial(new ER_DebugLightProbeMaterial(core, shaderEntries, HAS_VERTEX_SHADER | HAS_PIXEL_SHADER, true), ER_MaterialHelper::debugLightProbeMaterialName);
		mDiffuseProbeRenderingObject->LoadRenderBuffers();
		mDiffuseProbeRenderingObject->LoadInstanceBuffers();
		mDiffuseProbeRenderingObject->ResetInstanceData(mDiffuseProbesCountTotal, true);
		for (int i = 0; i < mDiffuseProbesCountTotal; i++) {
			XMMATRIX worldT = XMMatrixTranslation(mDiffuseProbes[i]->GetPosition().x, mDiffuseProbes[i]->GetPosition().y, mDiffuseProbes[i]->GetPosition().z);
			mDiffuseProbeRenderingObject->AddInstanceData(worldT);
		}
		mDiffuseProbeRenderingObject->UpdateInstanceBuffer(mDiffuseProbeRenderingObject->GetInstancesData());
	}

	void ER_LightProbesManager::SetupSpecularProbes(ER_Core& game, ER_Camera& camera, ER_Scene* scene, ER_DirectionalLight& light, ER_ShadowMapper& shadowMapper)
	{
		ER_RHI* rhi = game.GetRHI();

		ER_MaterialSystems materialSystems;
		materialSystems.mCamera = &camera;
		materialSystems.mDirectionalLight = &light;
		materialSystems.mShadowMapper = &shadowMapper;
		materialSystems.mProbesManager = this;

		XMFLOAT3& minBounds = mSceneProbesMinBounds;
		XMFLOAT3& maxBounds = mSceneProbesMaxBounds;
		
		SetupGlobalSpecularProbe(game, camera, scene, light, shadowMapper);

		mSpecularProbesCountX = (maxBounds.x - minBounds.x) / mDistanceBetweenSpecularProbes + 1;
		mSpecularProbesCountY = (maxBounds.y - minBounds.y) / mDistanceBetweenSpecularProbes + 1;
		mSpecularProbesCountZ = (maxBounds.z - minBounds.z) / mDistanceBetweenSpecularProbes + 1;
		mSpecularProbesCountTotal = mSpecularProbesCountX * mSpecularProbesCountY * mSpecularProbesCountZ;
		assert(mSpecularProbesCountTotal);
		mSpecularProbes.reserve(mSpecularProbesCountTotal);

		// cells setup
		mSpecularProbesCellsCountX = (mSpecularProbesCountX - 1);
		mSpecularProbesCellsCountY = (mSpecularProbesCountY - 1);
		mSpecularProbesCellsCountZ = (mSpecularProbesCountZ - 1);
		mSpecularProbesCellsCountTotal = mSpecularProbesCellsCountX * mSpecularProbesCellsCountY * mSpecularProbesCellsCountZ;
		mSpecularProbesCells.resize(mSpecularProbesCellsCountTotal, {});
		assert(mSpecularProbesCellsCountTotal);

		float probeCellPositionOffset = static_cast<float>(mDistanceBetweenSpecularProbes) / 2.0f;
		mSpecularProbesCellBounds = {
			XMFLOAT3(-probeCellPositionOffset, -probeCellPositionOffset, -probeCellPositionOffset),
			XMFLOAT3(probeCellPositionOffset, probeCellPositionOffset, probeCellPositionOffset) };
		
		for (int cellsY = 0; cellsY < mSpecularProbesCellsCountY; cellsY++)
		{
			for (int cellsX = 0; cellsX < mSpecularProbesCellsCountX; cellsX++)
			{
				for (int cellsZ = 0; cellsZ < mSpecularProbesCellsCountZ; cellsZ++)
				{
					XMFLOAT3 pos = XMFLOAT3(
						minBounds.x + probeCellPositionOffset + cellsX * mDistanceBetweenSpecularProbes,
						minBounds.y + probeCellPositionOffset + cellsY * mDistanceBetweenSpecularProbes,
						minBounds.z + probeCellPositionOffset + cellsZ * mDistanceBetweenSpecularProbes);

					int index = cellsY * (mSpecularProbesCellsCountX * mSpecularProbesCellsCountZ) + cellsX * mSpecularProbesCellsCountZ + cellsZ;
					mSpecularProbesCells[index].index = index;
					mSpecularProbesCells[index].position = pos;
				}
			}
		}

		// simple 3D grid distribution of probes
		for (size_t i = 0; i < mSpecularProbesCountTotal; i++)
			mSpecularProbes.emplace_back(new ER_LightProbe(game, light, shadowMapper, SPECULAR_PROBE_SIZE, SPECULAR_PROBE));
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
					mSpecularProbes[index]->SetIndex(index);
					mSpecularProbes[index]->SetPosition(pos);
					mSpecularProbes[index]->SetShaderInfoForConvolution(mConvolutionPS);
					AddProbeToCells(mSpecularProbes[index], SPECULAR_PROBE, minBounds, maxBounds);
				}
			}
		}

		// all probes positions GPU buffer
		XMFLOAT3* specularProbesPositionsCPUBuffer = new XMFLOAT3[mSpecularProbesCountTotal];
		for (int probeIndex = 0; probeIndex < mSpecularProbesCountTotal; probeIndex++)
			specularProbesPositionsCPUBuffer[probeIndex] = mSpecularProbes[probeIndex]->GetPosition();
		mSpecularProbesPositionsGPUBuffer = rhi->CreateGPUBuffer();
		mSpecularProbesPositionsGPUBuffer->CreateGPUBufferResource(rhi, specularProbesPositionsCPUBuffer, mSpecularProbesCountTotal, sizeof(XMFLOAT3), false, ER_BIND_SHADER_RESOURCE, 0, ER_RESOURCE_MISC_BUFFER_STRUCTURED);
		DeleteObjects(specularProbesPositionsCPUBuffer);

		// probe cell's indices GPU buffer, tex. array indices GPU/CPU buffers
		int* specularProbeCellsIndicesCPUBuffer = new int[mSpecularProbesCellsCountTotal * PROBE_COUNT_PER_CELL];
		for (int probeIndex = 0; probeIndex < mSpecularProbesCellsCountTotal; probeIndex++)
		{
			for (int indices = 0; indices < PROBE_COUNT_PER_CELL; indices++)
			{
				if (indices >= mSpecularProbesCells[probeIndex].lightProbeIndices.size())
					specularProbeCellsIndicesCPUBuffer[probeIndex * PROBE_COUNT_PER_CELL + indices] = -1;
				else
					specularProbeCellsIndicesCPUBuffer[probeIndex * PROBE_COUNT_PER_CELL + indices] = mSpecularProbesCells[probeIndex].lightProbeIndices[indices];
			}
		}
		mSpecularProbesCellsIndicesGPUBuffer = rhi->CreateGPUBuffer();
		mSpecularProbesCellsIndicesGPUBuffer->CreateGPUBufferResource(rhi, specularProbeCellsIndicesCPUBuffer,	mSpecularProbesCellsCountTotal * PROBE_COUNT_PER_CELL, sizeof(int), false, ER_BIND_SHADER_RESOURCE, 0, ER_RESOURCE_MISC_BUFFER_STRUCTURED);
		DeleteObjects(specularProbeCellsIndicesCPUBuffer);

		mSpecularProbesTexArrayIndicesCPUBuffer = new int[mSpecularProbesCountTotal];
		mSpecularProbesTexArrayIndicesGPUBuffer = rhi->CreateGPUBuffer();
		mSpecularProbesTexArrayIndicesGPUBuffer->CreateGPUBufferResource(rhi, mSpecularProbesTexArrayIndicesCPUBuffer, mSpecularProbesCountTotal, sizeof(int), true, ER_BIND_SHADER_RESOURCE, 0, ER_RESOURCE_MISC_BUFFER_STRUCTURED);

		std::string name = "Debug specular lightprobes ";
		auto result = scene->objects.insert(
			std::pair<std::string, ER_RenderingObject*>(name, new ER_RenderingObject(name, scene->objects.size(), game, camera,
				std::unique_ptr<ER_Model>(new ER_Model(game, ER_Utility::GetFilePath("content\\models\\sphere_lowpoly.fbx"), true)), false, true))
		);

		if (!result.second)
			throw ER_CoreException("Could not add a specular probe global object into scene");
		
		MaterialShaderEntries shaderEntries;
		shaderEntries.vertexEntry += "_instancing";

		mSpecularProbeRenderingObject = result.first->second;
		mSpecularProbeRenderingObject->LoadMaterial(new ER_DebugLightProbeMaterial(game, shaderEntries, HAS_VERTEX_SHADER | HAS_PIXEL_SHADER, true), ER_MaterialHelper::debugLightProbeMaterialName);
		mSpecularProbeRenderingObject->LoadRenderBuffers();
		mSpecularProbeRenderingObject->LoadInstanceBuffers();
		mSpecularProbeRenderingObject->ResetInstanceData(mSpecularProbesCountTotal, true);
		for (int i = 0; i < mSpecularProbesCountTotal; i++) {
			XMMATRIX worldT = XMMatrixTranslation(mSpecularProbes[i]->GetPosition().x, mSpecularProbes[i]->GetPosition().y, mSpecularProbes[i]->GetPosition().z);
			mSpecularProbeRenderingObject->AddInstanceData(worldT);
		}
		mSpecularProbeRenderingObject->UpdateInstanceBuffer(mSpecularProbeRenderingObject->GetInstancesData());

		mSpecularCubemapArrayRT = rhi->CreateGPUTexture();
		mSpecularCubemapArrayRT->CreateGPUTextureResource(rhi, SPECULAR_PROBE_SIZE, SPECULAR_PROBE_SIZE, 1, ER_FORMAT_R8G8B8A8_UNORM, ER_BIND_SHADER_RESOURCE, SPECULAR_PROBE_MIP_COUNT, -1, CUBEMAP_FACES_COUNT, true, mMaxSpecularProbesInVolumeCount);
	}

	void ER_LightProbesManager::AddProbeToCells(ER_LightProbe* aProbe, ER_ProbeType aType, const XMFLOAT3& minBounds, const XMFLOAT3& maxBounds)
	{
		int index = aProbe->GetIndex();
		if (aType == DIFFUSE_PROBE)
		{
			// brute force O(cells * probes)
			for (auto& cell : mDiffuseProbesCells)
			{
				if (IsProbeInCell(aProbe, cell, mDiffuseProbesCellBounds))
					cell.lightProbeIndices.push_back(index);

				if (cell.lightProbeIndices.size() > PROBE_COUNT_PER_CELL)
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

				if (cell.lightProbeIndices.size() > PROBE_COUNT_PER_CELL)
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
			int yIndex = floor((pos.y - mSceneProbesMinBounds.y) / static_cast<float>(mDistanceBetweenDiffuseProbes));
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
			int yIndex = floor((pos.y - mSceneProbesMinBounds.y) / static_cast<float>(mDistanceBetweenSpecularProbes));
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

	const DirectX::XMFLOAT4& ER_LightProbesManager::GetProbesCellsCount(ER_ProbeType aType)
	{
		if (aType == DIFFUSE_PROBE)
			return XMFLOAT4(mDiffuseProbesCellsCountX, mDiffuseProbesCellsCountY, mDiffuseProbesCellsCountZ, mDiffuseProbesCellsCountTotal);
		else
			return XMFLOAT4(mSpecularProbesCellsCountX, mSpecularProbesCellsCountY, mSpecularProbesCellsCountZ, mSpecularProbesCellsCountTotal);
	}

	bool ER_LightProbesManager::IsProbeInCell(ER_LightProbe* aProbe, ER_LightProbeCell& aCell, ER_AABB& aCellBounds)
	{
		XMFLOAT3 pos = aProbe->GetPosition();
		float epsilon = 0.00001f;

		XMFLOAT3 maxBounds = XMFLOAT3(
			aCellBounds.second.x + aCell.position.x,
			aCellBounds.second.y + aCell.position.y,
			aCellBounds.second.z + aCell.position.z);

		XMFLOAT3 minBounds = XMFLOAT3(
			aCellBounds.first.x + aCell.position.x,
			aCellBounds.first.y + aCell.position.y,
			aCellBounds.first.z + aCell.position.z);

		return	(pos.x <= (maxBounds.x + epsilon) && pos.x >= (minBounds.x - epsilon)) &&
				(pos.y <= (maxBounds.y + epsilon) && pos.y >= (minBounds.y - epsilon)) &&
				(pos.z <= (maxBounds.z + epsilon) && pos.z >= (minBounds.z - epsilon));
	}

	void ER_LightProbesManager::ComputeOrLoadGlobalProbes(ER_Core& game, ProbesRenderingObjectsInfo& aObjects, ER_Skybox* skybox)
	{
		assert(skybox);

		// DIFFUSE_PROBE
		{
			if (mGlobalDiffuseProbeReady)
				return;

			std::wstring diffuseProbesPath = mLevelPath + L"diffuse_probes\\";
			if (!mGlobalDiffuseProbe->LoadProbeFromDisk(game, diffuseProbesPath))
#ifdef ER_PLATFORM_WIN64_DX11
				mGlobalDiffuseProbe->Compute(game, mTempDiffuseCubemapFacesRT, mTempDiffuseCubemapFacesConvolutedRT, mTempDiffuseCubemapDepthBuffers, diffuseProbesPath, aObjects, mQuadRenderer, skybox);
#else
				throw ER_CoreException("ER_LightProbesManager: Computing & saving the probes is only possible on DX11 at the moment");
#endif
			mGlobalDiffuseProbeReady = true;
		}
		// SPECULAR_PROBE
		{
			if (mGlobalSpecularProbeReady)
				return;

			std::wstring specularProbesPath = mLevelPath + L"specular_probes\\";

			if (!mGlobalSpecularProbe->LoadProbeFromDisk(game, specularProbesPath))
#ifdef ER_PLATFORM_WIN64_DX11
				mGlobalSpecularProbe->Compute(game, mTempSpecularCubemapFacesRT, mTempSpecularCubemapFacesConvolutedRT, mTempSpecularCubemapDepthBuffers, specularProbesPath, aObjects, mQuadRenderer, skybox);
#else
				throw ER_CoreException("ER_LightProbesManager: Computing & saving the probes is only possible on DX11 at the moment");
#endif
			mGlobalSpecularProbeReady = true;
		}
	}

	void ER_LightProbesManager::ComputeOrLoadLocalProbes(ER_Core& game, ProbesRenderingObjectsInfo& aObjects, ER_Skybox* skybox)
	{
		ER_RHI* rhi = game.GetRHI();

		int numThreads = std::thread::hardware_concurrency();
		assert(numThreads > 0);

		if (!mDiffuseProbesReady)
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
						mDiffuseProbes[j]->LoadProbeFromDisk(game, diffuseProbesPath);
				}));
			}
			for (auto& t : threads) t.join();

			for (auto& probe : mDiffuseProbes)
			{
				if (!probe->IsLoadedFromDisk())
#ifdef ER_PLATFORM_WIN64_DX11
					probe->Compute(game, mTempDiffuseCubemapFacesRT, mTempDiffuseCubemapFacesConvolutedRT, mTempDiffuseCubemapDepthBuffers, diffuseProbesPath, aObjects, mQuadRenderer, skybox);
#else
					//TODO load empty
					throw ER_CoreException("ER_LightProbesManager: Computing & saving the probes is only possible on DX11 at the moment");
#endif
			}
			
			mDiffuseProbesReady = true;

			UpdateProbesByType(game, DIFFUSE_PROBE);

			// SH GPU buffer
			XMFLOAT3* shCPUBuffer = new XMFLOAT3[mDiffuseProbesCountTotal * SPHERICAL_HARMONICS_COEF_COUNT];
			for (int probeIndex = 0; probeIndex < mDiffuseProbesCountTotal; probeIndex++)
			{
				const std::vector<XMFLOAT3> sh = mDiffuseProbes[probeIndex]->GetSphericalHarmonics();
				for (int i = 0; i < SPHERICAL_HARMONICS_COEF_COUNT; i++)
					shCPUBuffer[probeIndex * SPHERICAL_HARMONICS_COEF_COUNT + i] = sh[i];
			}
			mDiffuseProbesSphericalHarmonicsGPUBuffer = rhi->CreateGPUBuffer();
			mDiffuseProbesSphericalHarmonicsGPUBuffer->CreateGPUBufferResource(rhi, shCPUBuffer, mDiffuseProbesCountTotal* SPHERICAL_HARMONICS_COEF_COUNT, sizeof(XMFLOAT3),
				false, ER_BIND_SHADER_RESOURCE, 0, ER_RESOURCE_MISC_BUFFER_STRUCTURED);
			DeleteObjects(shCPUBuffer);
		}

		if (!mSpecularProbesReady)
		{
			std::wstring specularProbesPath = mLevelPath + L"specular_probes\\";

			std::vector<std::thread> threads;
			int numThreads = std::thread::hardware_concurrency();
			threads.reserve(numThreads);

			int probesPerThread = mSpecularProbes.size() / numThreads;

			for (int i = 0; i < numThreads; i++)
			{
				threads.push_back(std::thread([&, specularProbesPath, i]
				{
					int endRange = (i < numThreads - 1) ? (i + 1) * probesPerThread : mSpecularProbes.size();
					for (int j = i * probesPerThread; j < endRange; j++)
						mSpecularProbes[j]->LoadProbeFromDisk(game, specularProbesPath);
				}));
			}
			for (auto& t : threads) t.join();

			for (auto& probe : mSpecularProbes)
			{
				if (!probe->IsLoadedFromDisk())
#ifdef ER_PLATFORM_WIN64_DX11
					probe->Compute(game, mTempSpecularCubemapFacesRT, mTempSpecularCubemapFacesConvolutedRT, mTempSpecularCubemapDepthBuffers, specularProbesPath, aObjects, mQuadRenderer, skybox);
#else
					//TODO load empty
					throw ER_CoreException("ER_LightProbesManager: Computing & saving the probes is only possible on DX11 at the moment");
#endif		
			}
			mSpecularProbesReady = true;
		}
	}

	void ER_LightProbesManager::DrawDebugProbes(ER_RHI* rhi, ER_RHI_GPUTexture* aRenderTarget, ER_ProbeType aType, ER_RHI_GPURootSignature* rs)
	{
		assert(aRenderTarget);
		assert(rhi);

		ER_RenderingObject* probeObject = aType == DIFFUSE_PROBE ? mDiffuseProbeRenderingObject : mSpecularProbeRenderingObject;
		bool ready = aType == DIFFUSE_PROBE ? mDiffuseProbesReady : mSpecularProbesReady;
		std::string& psoName = DIFFUSE_PROBE ? mDiffuseDebugLightProbePassPSOName : mSpecularDebugLightProbePassPSOName;

		ER_MaterialSystems materialSystems;
		materialSystems.mProbesManager = this;

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
					rhi->SetRenderTargetFormats({ aRenderTarget });
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
		std::vector<ER_LightProbe*>& probes = (aType == DIFFUSE_PROBE) ? mDiffuseProbes : mSpecularProbes;

		ER_RenderingObject* probeRenderingObject = (aType == DIFFUSE_PROBE) ? mDiffuseProbeRenderingObject : mSpecularProbeRenderingObject;
		if (aType == DIFFUSE_PROBE)
		{
			if (!mDiffuseProbesReady)
				return;
		}
		else
		{
			if (!mSpecularProbesReady)
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
				lightProbe->CPUCullAgainstProbeBoundingVolume(minBounds, maxBounds);
		}

		if (probeRenderingObject)
		{
			auto& oldInstancedData = probeRenderingObject->GetInstancesData();
			assert(oldInstancedData.size() == probes.size());

			for (int i = 0; i < oldInstancedData.size(); i++)
			{
				bool isCulled = probes[i]->IsCulled();
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
					for (int cubeI = 0; cubeI < CUBEMAP_FACES_COUNT; cubeI++)
					{
						for (int mip = 0; mip < SPECULAR_PROBE_MIP_COUNT; mip++)
						{
							rhi->CopyGPUTextureSubresourceRegion(mSpecularCubemapArrayRT, mip + (cubeI + CUBEMAP_FACES_COUNT * i) * SPECULAR_PROBE_MIP_COUNT, 0, 0, 0,
								probes[probeIndex]->GetCubemapTexture(), mip + cubeI * SPECULAR_PROBE_MIP_COUNT);
						}
					}
					mSpecularProbesTexArrayIndicesCPUBuffer[probeIndex] = CUBEMAP_FACES_COUNT * i;
				}
			}
			rhi->UpdateBuffer(mSpecularProbesTexArrayIndicesGPUBuffer, mSpecularProbesTexArrayIndicesCPUBuffer, sizeof(mSpecularProbesTexArrayIndicesCPUBuffer[0]) * mSpecularProbesCountTotal);
		}
	}

	void ER_LightProbesManager::UpdateProbes(ER_Core& game)
	{
		//int difProbeCellIndexCamera = GetCellIndex(mMainCamera.Position(), DIFFUSE_PROBE);
		assert(mMaxSpecularProbesInVolumeCount > 0);

		//UpdateProbesByType(game, DIFFUSE_PROBE); we do not need to update diffuse probes every frame anymore
		UpdateProbesByType(game, SPECULAR_PROBE);
	}
}

