#include "ER_LightProbesManager.h"
#include "Game.h"
#include "GameTime.h"
#include "GameException.h"
#include "Utility.h"
#include "DirectionalLight.h"
#include "MatrixHelper.h"
#include "ER_MaterialHelper.h"
#include "VectorHelper.h"
#include "ShaderCompiler.h"
#include "ER_RenderingObject.h"
#include "ER_Model.h"
#include "ER_Scene.h"
#include "ER_RenderableAABB.h"
#include "ER_GPUBuffer.h"
#include "ER_LightProbe.h"
#include "ER_QuadRenderer.h"
#include "ER_DebugLightProbeMaterial.h"
#include "ER_MaterialsCallbacks.h"

namespace Library
{
	ER_LightProbesManager::ER_LightProbesManager(Game& game, Camera& camera, ER_Scene* scene, DirectionalLight& light, ER_ShadowMapper& shadowMapper)
		: mMainCamera(camera)
	{
		if (!scene)
			throw GameException("No scene to load light probes for!");

		mTempDiffuseCubemapFacesRT = new ER_GPUTexture(game.Direct3DDevice(), DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE_SIZE, 1, DXGI_FORMAT_R16G16B16A16_FLOAT,
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 1, -1, CUBEMAP_FACES_COUNT, true);
		mTempDiffuseCubemapFacesConvolutedRT = new ER_GPUTexture(game.Direct3DDevice(), DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE_SIZE, 1, DXGI_FORMAT_R16G16B16A16_FLOAT,
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 1, -1, CUBEMAP_FACES_COUNT, true);
		mTempSpecularCubemapFacesRT = new ER_GPUTexture(game.Direct3DDevice(), SPECULAR_PROBE_SIZE, SPECULAR_PROBE_SIZE, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, SPECULAR_PROBE_MIP_COUNT, -1, CUBEMAP_FACES_COUNT, true);
		mTempSpecularCubemapFacesConvolutedRT = new ER_GPUTexture(game.Direct3DDevice(), SPECULAR_PROBE_SIZE, SPECULAR_PROBE_SIZE, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, SPECULAR_PROBE_MIP_COUNT, -1, CUBEMAP_FACES_COUNT, true);

		for (int i = 0; i < CUBEMAP_FACES_COUNT; i++)
		{
			mTempDiffuseCubemapDepthBuffers[i] = DepthTarget::Create(game.Direct3DDevice(), DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE_SIZE, 1u, DXGI_FORMAT_D24_UNORM_S8_UINT);
			mTempSpecularCubemapDepthBuffers[i] = DepthTarget::Create(game.Direct3DDevice(), SPECULAR_PROBE_SIZE, SPECULAR_PROBE_SIZE, 1u, DXGI_FORMAT_D24_UNORM_S8_UINT);
		}

		mQuadRenderer = (ER_QuadRenderer*)game.Services().GetService(ER_QuadRenderer::TypeIdClass());
		assert(mQuadRenderer);
		{
			ID3DBlob* blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\IBL\\ProbeConvolution.hlsl").c_str(), "PSMain", "ps_5_0", &blob)))
				throw GameException("Failed to load PSMain from shader: ProbeConvolution.hlsl!");
			if (FAILED(game.Direct3DDevice()->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mConvolutionPS)))
				throw GameException("Failed to create pixel shader from ProbeConvolution.hlsl!");
			blob->Release();
		}

		// Load a pre-computed Integration Map
		if (FAILED(DirectX::CreateDDSTextureFromFile(game.Direct3DDevice(), game.Direct3DDeviceContext(),
			Utility::GetFilePath(L"content\\textures\\IntegrationMapBrdf.dds").c_str(), nullptr, &mIntegrationMapTextureSRV)))
			throw GameException("Failed to create Integration Texture.");

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
			throw GameException("Loaded level has incorrect distances between probes (either diffuse or specular or both). Did you forget to assign them in the level file?");

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

		ReleaseObject(mConvolutionPS);

		DeleteObject(mTempDiffuseCubemapFacesRT);
		DeleteObject(mTempDiffuseCubemapFacesConvolutedRT);
		DeleteObject(mTempSpecularCubemapFacesRT);
		DeleteObject(mTempSpecularCubemapFacesConvolutedRT);
		for (int i = 0; i < CUBEMAP_FACES_COUNT; i++)
		{	
			DeleteObject(mTempDiffuseCubemapDepthBuffers[i]);
			DeleteObject(mTempSpecularCubemapDepthBuffers[i]);
		}
		ReleaseObject(mIntegrationMapTextureSRV);

		DeleteObject(mDiffuseProbesSphericalHarmonicsGPUBuffer);

		DeleteObject(mDiffuseProbesPositionsGPUBuffer);
		DeleteObject(mSpecularProbesPositionsGPUBuffer);

		DeleteObject(mDiffuseProbesCellsIndicesGPUBuffer);

		DeleteObject(mSpecularCubemapArrayRT);
		DeleteObject(mSpecularProbesCellsIndicesGPUBuffer);
		DeleteObjects(mSpecularProbesTexArrayIndicesCPUBuffer);
		DeleteObject(mSpecularProbesTexArrayIndicesGPUBuffer);
	}

	void ER_LightProbesManager::SetupGlobalDiffuseProbe(Game& game, Camera& camera, ER_Scene* scene, DirectionalLight& light, ER_ShadowMapper& shadowMapper)
	{
		mGlobalDiffuseProbe = new ER_LightProbe(game, light, shadowMapper, DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE);
		mGlobalDiffuseProbe->SetIndex(-1);
		mGlobalDiffuseProbe->SetPosition(XMFLOAT3(0.0f, 2.0f, 0.0f)); //just a bit above the ground
		mGlobalDiffuseProbe->SetShaderInfoForConvolution(mConvolutionPS);
	}
	void ER_LightProbesManager::SetupGlobalSpecularProbe(Game& game, Camera& camera, ER_Scene* scene, DirectionalLight& light, ER_ShadowMapper& shadowMapper)
	{
		mGlobalSpecularProbe = new ER_LightProbe(game, light, shadowMapper, SPECULAR_PROBE_SIZE, SPECULAR_PROBE);
		mGlobalSpecularProbe->SetIndex(-1);
		mGlobalSpecularProbe->SetPosition(XMFLOAT3(0.0f, 2.0f, 0.0f)); //just a bit above the ground
		mGlobalSpecularProbe->SetShaderInfoForConvolution(mConvolutionPS);
	}

	void ER_LightProbesManager::SetupDiffuseProbes(Game& game, Camera& camera, ER_Scene* scene, DirectionalLight& light, ER_ShadowMapper& shadowMapper)
	{
		ER_MaterialSystems materialSystems;
		materialSystems.mCamera = &camera;
		materialSystems.mDirectionalLight = &light;
		materialSystems.mShadowMapper = &shadowMapper;
		materialSystems.mProbesManager = this;

		XMFLOAT3& minBounds = mSceneProbesMinBounds;
		XMFLOAT3& maxBounds = mSceneProbesMaxBounds;

		SetupGlobalDiffuseProbe(game, camera, scene, light, shadowMapper);

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
			mDiffuseProbes.emplace_back(new ER_LightProbe(game, light, shadowMapper, DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE));
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
		mDiffuseProbesPositionsGPUBuffer = new ER_GPUBuffer(game.Direct3DDevice(), diffuseProbesPositionsCPUBuffer, mDiffuseProbesCountTotal, sizeof(XMFLOAT3),
			D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED);
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
		mDiffuseProbesCellsIndicesGPUBuffer = new ER_GPUBuffer(game.Direct3DDevice(), diffuseProbeCellsIndicesCPUBuffer,
			mDiffuseProbesCellsCountTotal * PROBE_COUNT_PER_CELL, sizeof(int),
			D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED);
		DeleteObjects(diffuseProbeCellsIndicesCPUBuffer);
		
		std::string name = "Debug diffuse lightprobes ";
		auto result = scene->objects.insert(
			std::pair<std::string, ER_RenderingObject*>(name, new ER_RenderingObject(name, scene->objects.size(), game, camera,
				std::unique_ptr<ER_Model>(new ER_Model(game, Utility::GetFilePath("content\\models\\sphere_lowpoly.fbx"), true)), false, true))
		);

		if (!result.second)
			throw GameException("Could not add a diffuse probe global object into scene");

		MaterialShaderEntries shaderEntries;
		shaderEntries.vertexEntry += "_instancing";

		mDiffuseProbeRenderingObject = result.first->second;
		mDiffuseProbeRenderingObject->LoadMaterial(new ER_DebugLightProbeMaterial(game, shaderEntries, HAS_VERTEX_SHADER | HAS_PIXEL_SHADER, true), ER_MaterialHelper::debugLightProbeMaterialName);
		mDiffuseProbeRenderingObject->LoadRenderBuffers();
		mDiffuseProbeRenderingObject->LoadInstanceBuffers();
		mDiffuseProbeRenderingObject->ResetInstanceData(mDiffuseProbesCountTotal, true);
		for (int i = 0; i < mDiffuseProbesCountTotal; i++) {
			XMMATRIX worldT = XMMatrixTranslation(mDiffuseProbes[i]->GetPosition().x, mDiffuseProbes[i]->GetPosition().y, mDiffuseProbes[i]->GetPosition().z);
			mDiffuseProbeRenderingObject->AddInstanceData(worldT);
		}
		mDiffuseProbeRenderingObject->UpdateInstanceBuffer(mDiffuseProbeRenderingObject->GetInstancesData());
	}

	void ER_LightProbesManager::SetupSpecularProbes(Game& game, Camera& camera, ER_Scene* scene, DirectionalLight& light, ER_ShadowMapper& shadowMapper)
	{
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
		mSpecularProbesPositionsGPUBuffer = new ER_GPUBuffer(game.Direct3DDevice(), specularProbesPositionsCPUBuffer, mSpecularProbesCountTotal, sizeof(XMFLOAT3),
			D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED);
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
		mSpecularProbesCellsIndicesGPUBuffer = new ER_GPUBuffer(game.Direct3DDevice(), specularProbeCellsIndicesCPUBuffer,
			mSpecularProbesCellsCountTotal * PROBE_COUNT_PER_CELL, sizeof(int),
			D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED);
		DeleteObjects(specularProbeCellsIndicesCPUBuffer);

		mSpecularProbesTexArrayIndicesCPUBuffer = new int[mSpecularProbesCountTotal];
		mSpecularProbesTexArrayIndicesGPUBuffer = new ER_GPUBuffer(game.Direct3DDevice(), mSpecularProbesTexArrayIndicesCPUBuffer, mSpecularProbesCountTotal, sizeof(int),
			D3D11_USAGE_DYNAMIC, D3D11_BIND_SHADER_RESOURCE, D3D11_CPU_ACCESS_WRITE, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED);

		std::string name = "Debug specular lightprobes ";
		auto result = scene->objects.insert(
			std::pair<std::string, ER_RenderingObject*>(name, new ER_RenderingObject(name, scene->objects.size(), game, camera,
				std::unique_ptr<ER_Model>(new ER_Model(game, Utility::GetFilePath("content\\models\\sphere_lowpoly.fbx"), true)), false, true))
		);

		if (!result.second)
			throw GameException("Could not add a specular probe global object into scene");
		
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

		mSpecularCubemapArrayRT = new ER_GPUTexture(game.Direct3DDevice(), SPECULAR_PROBE_SIZE, SPECULAR_PROBE_SIZE, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
			D3D11_BIND_SHADER_RESOURCE, SPECULAR_PROBE_MIP_COUNT, -1, CUBEMAP_FACES_COUNT, true, mMaxSpecularProbesInVolumeCount);
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
					throw GameException("Too many diffuse probes per cell!");
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
					throw GameException("Too many specular probes per cell!");
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

	void ER_LightProbesManager::ComputeOrLoadGlobalProbes(Game& game, ProbesRenderingObjectsInfo& aObjects, ER_Skybox* skybox)
	{
		assert(skybox);

		// DIFFUSE_PROBE
		{
			if (mGlobalDiffuseProbeReady)
				return;

			std::wstring diffuseProbesPath = mLevelPath + L"diffuse_probes\\";
			if (!mGlobalDiffuseProbe->LoadProbeFromDisk(game, diffuseProbesPath))
				mGlobalDiffuseProbe->Compute(game, mTempDiffuseCubemapFacesRT, mTempDiffuseCubemapFacesConvolutedRT, mTempDiffuseCubemapDepthBuffers, diffuseProbesPath, aObjects, mQuadRenderer, skybox);

			mGlobalDiffuseProbeReady = true;
		}
		// SPECULAR_PROBE
		{
			if (mGlobalSpecularProbeReady)
				return;

			std::wstring specularProbesPath = mLevelPath + L"specular_probes\\";
			if (!mGlobalSpecularProbe->LoadProbeFromDisk(game, specularProbesPath))
				mGlobalSpecularProbe->Compute(game, mTempSpecularCubemapFacesRT, mTempSpecularCubemapFacesConvolutedRT, mTempSpecularCubemapDepthBuffers, specularProbesPath, aObjects, mQuadRenderer, skybox);

			mGlobalSpecularProbeReady = true;
		}
	}

	void ER_LightProbesManager::ComputeOrLoadLocalProbes(Game& game, ProbesRenderingObjectsInfo& aObjects, ER_Skybox* skybox)
	{
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
					probe->Compute(game, mTempDiffuseCubemapFacesRT, mTempDiffuseCubemapFacesConvolutedRT, mTempDiffuseCubemapDepthBuffers, diffuseProbesPath, aObjects, mQuadRenderer, skybox);
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
			mDiffuseProbesSphericalHarmonicsGPUBuffer = new ER_GPUBuffer(game.Direct3DDevice(), shCPUBuffer, mDiffuseProbesCountTotal * SPHERICAL_HARMONICS_COEF_COUNT, sizeof(XMFLOAT3),
				D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, D3D11_CPU_ACCESS_WRITE, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED);
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
					probe->Compute(game, mTempSpecularCubemapFacesRT, mTempSpecularCubemapFacesConvolutedRT, mTempSpecularCubemapDepthBuffers, specularProbesPath, aObjects, mQuadRenderer, skybox);
			}
			mSpecularProbesReady = true;
		}
	}

	void ER_LightProbesManager::DrawDebugProbes(ER_ProbeType aType)
	{
		ER_RenderingObject* probeObject = aType == DIFFUSE_PROBE ? mDiffuseProbeRenderingObject : mSpecularProbeRenderingObject;
		bool ready = aType == DIFFUSE_PROBE ? mDiffuseProbesReady : mSpecularProbesReady;
		
		ER_MaterialSystems materialSystems;
		materialSystems.mProbesManager = this;

		if (probeObject && ready)
		{
			auto materialInfo = probeObject->GetMaterials().find(ER_MaterialHelper::debugLightProbeMaterialName);
			if (materialInfo != probeObject->GetMaterials().end())
			{
				static_cast<ER_DebugLightProbeMaterial*>(materialInfo->second)->PrepareForRendering(materialSystems, probeObject, 0, static_cast<int>(aType));
				probeObject->Draw(ER_MaterialHelper::debugLightProbeMaterialName);
			}
		}

	}

	void ER_LightProbesManager::UpdateProbesByType(Game& game, ER_ProbeType aType)
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

		auto context = game.Direct3DDeviceContext();
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
							context->CopySubresourceRegion(mSpecularCubemapArrayRT->GetTexture2D(), D3D11CalcSubresource(mip, cubeI + CUBEMAP_FACES_COUNT * i, SPECULAR_PROBE_MIP_COUNT), 0, 0, 0,
								probes[probeIndex]->GetCubemapTexture2D(), D3D11CalcSubresource(mip, cubeI, SPECULAR_PROBE_MIP_COUNT), NULL);
						}
					}
					mSpecularProbesTexArrayIndicesCPUBuffer[probeIndex] = CUBEMAP_FACES_COUNT * i;
				}
			}
			mSpecularProbesTexArrayIndicesGPUBuffer->Update(context, mSpecularProbesTexArrayIndicesCPUBuffer, sizeof(mSpecularProbesTexArrayIndicesCPUBuffer[0]) * mSpecularProbesCountTotal, D3D11_MAP_WRITE_DISCARD);
		}
	}

	void ER_LightProbesManager::UpdateProbes(Game& game)
	{
		//int difProbeCellIndexCamera = GetCellIndex(mMainCamera.Position(), DIFFUSE_PROBE);
		assert(mMaxSpecularProbesInVolumeCount > 0);

		//UpdateProbesByType(game, DIFFUSE_PROBE); we do not need to update diffuse probes every frame anymore
		UpdateProbesByType(game, SPECULAR_PROBE);
	}
}

