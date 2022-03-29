#include "ER_IlluminationProbeManager.h"
#include "Game.h"
#include "GameTime.h"
#include "GameException.h"
#include "Utility.h"
#include "DirectionalLight.h"
#include "MatrixHelper.h"
#include "MaterialHelper.h"
#include "VectorHelper.h"
#include "ShaderCompiler.h"
#include "ER_RenderingObject.h"
#include "Model.h"
#include "Scene.h"
#include "RenderableAABB.h"
#include "ER_GPUBuffer.h"
#include "ER_LightProbe.h"
#include "ER_QuadRenderer.h"
#include "ER_DebugLightProbeMaterial.h"
#include "ER_MaterialsCallbacks.h"

namespace Library
{
	ER_IlluminationProbeManager::ER_IlluminationProbeManager(Game& game, Camera& camera, Scene* scene, DirectionalLight& light, ER_ShadowMapper& shadowMapper)
		: mMainCamera(camera)
	{
		//TODO temp

		if (!scene)
			throw GameException("No scene to load light probes for!");

		if (!scene->HasLightProbesSupport())
		{
			mEnabled = false;
			return;
		}

		D3D11_SAMPLER_DESC sam_desc;
		sam_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sam_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sam_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sam_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		sam_desc.MipLODBias = 0;
		sam_desc.MaxAnisotropy = 1;
		if (FAILED(game.Direct3DDevice()->CreateSamplerState(&sam_desc, &mLinearSamplerState)))
			throw GameException("Failed to create sampler mLinearSamplerState!");

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

		mSceneProbesMinBounds = scene->GetLightProbesVolumeMinBounds();
		mSceneProbesMaxBounds = scene->GetLightProbesVolumeMaxBounds();

		for (int volumeIndex = 0; volumeIndex < NUM_PROBE_VOLUME_CASCADES; volumeIndex++)
		{
			mDebugDiffuseProbeVolumeGizmo[volumeIndex] = new RenderableAABB(game, mMainCamera, XMFLOAT4(0.44f, 0.2f, 0.94f, 1.0f));
			mDebugDiffuseProbeVolumeGizmo[volumeIndex]->Initialize();
			mDebugDiffuseProbeVolumeGizmo[volumeIndex]->InitializeGeometry({
				XMFLOAT3(-DiffuseProbesVolumeCascadeSizes[volumeIndex], -DiffuseProbesVolumeCascadeSizes[volumeIndex], -DiffuseProbesVolumeCascadeSizes[volumeIndex]),
				XMFLOAT3(DiffuseProbesVolumeCascadeSizes[volumeIndex], DiffuseProbesVolumeCascadeSizes[volumeIndex], DiffuseProbesVolumeCascadeSizes[volumeIndex]) }, XMMatrixScaling(1, 1, 1));
			mDebugDiffuseProbeVolumeGizmo[volumeIndex]->SetPosition(mMainCamera.Position());

			mDebugSpecularProbeVolumeGizmo[volumeIndex] = new RenderableAABB(game, mMainCamera, XMFLOAT4(0.44f, 0.2f, 0.94f, 1.0f));
			mDebugSpecularProbeVolumeGizmo[volumeIndex]->Initialize();
			mDebugSpecularProbeVolumeGizmo[volumeIndex]->InitializeGeometry({
				XMFLOAT3(-SpecularProbesVolumeCascadeSizes[volumeIndex], -SpecularProbesVolumeCascadeSizes[volumeIndex], -SpecularProbesVolumeCascadeSizes[volumeIndex]),
				XMFLOAT3(SpecularProbesVolumeCascadeSizes[volumeIndex], SpecularProbesVolumeCascadeSizes[volumeIndex], SpecularProbesVolumeCascadeSizes[volumeIndex]) }, XMMatrixScaling(1, 1, 1));
			mDebugSpecularProbeVolumeGizmo[volumeIndex]->SetPosition(mMainCamera.Position());
		}
	
		// TODO Load a pre-computed Integration Map
		if (FAILED(DirectX::CreateDDSTextureFromFile(game.Direct3DDevice(), game.Direct3DDeviceContext(),
			Utility::GetFilePath(L"content\\textures\\skyboxes\\textureBrdf.dds").c_str(), nullptr, &mIntegrationMapTextureSRV)))
			throw GameException("Failed to create Integration Texture.");

		mMaxProbesInVolumeCount = MAX_PROBES_IN_VOLUME_PER_AXIS * MAX_PROBES_IN_VOLUME_PER_AXIS * MAX_PROBES_IN_VOLUME_PER_AXIS;

		SetupDiffuseProbes(game, camera, scene, light, shadowMapper);
		SetupSpecularProbes(game, camera, scene, light, shadowMapper);

		for (int i = 0; i < CUBEMAP_FACES_COUNT; i++)
		{
			mTempDiffuseCubemapDepthBuffers[i] = DepthTarget::Create(game.Direct3DDevice(), DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE_SIZE, 1u, DXGI_FORMAT_D24_UNORM_S8_UINT);
			mTempSpecularCubemapDepthBuffers[i] = DepthTarget::Create(game.Direct3DDevice(), SPECULAR_PROBE_SIZE, SPECULAR_PROBE_SIZE, 1u, DXGI_FORMAT_D24_UNORM_S8_UINT);
		}
	}

	ER_IlluminationProbeManager::~ER_IlluminationProbeManager()
	{
		DeleteObject(mGlobalDiffuseProbe);
		DeletePointerCollection(mDiffuseProbes);
		DeletePointerCollection(mSpecularProbes);

		ReleaseObject(mConvolutionPS);
		ReleaseObject(mLinearSamplerState);

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

		DeleteObject(mDiffuseProbesPositionsGPUBuffer);
		DeleteObject(mSpecularProbesPositionsGPUBuffer);
		for (int i = 0; i < NUM_PROBE_VOLUME_CASCADES; i++)
		{
			DeleteObject(mDebugDiffuseProbeVolumeGizmo[i]);
			DeleteObject(mDebugSpecularProbeVolumeGizmo[i]);

			DeleteObject(mDiffuseCubemapArrayRT[i]);
			DeleteObject(mDiffuseProbesCellsIndicesGPUBuffer[i]);
			DeleteObjects(mDiffuseProbesTexArrayIndicesCPUBuffer[i]);
			DeleteObject(mDiffuseProbesTexArrayIndicesGPUBuffer[i]);

			DeleteObject(mSpecularCubemapArrayRT[i]);
			DeleteObject(mSpecularProbesCellsIndicesGPUBuffer[i]);
			DeleteObjects(mSpecularProbesTexArrayIndicesCPUBuffer[i]);
			DeleteObject(mSpecularProbesTexArrayIndicesGPUBuffer[i]);
		}
	}

	void ER_IlluminationProbeManager::SetupDiffuseProbes(Game& game, Camera& camera, Scene* scene, DirectionalLight& light, ER_ShadowMapper& shadowMapper)
	{
		ER_MaterialSystems materialSystems;
		materialSystems.mCamera = &camera;
		materialSystems.mDirectionalLight = &light;
		materialSystems.mShadowMapper = &shadowMapper;
		materialSystems.mProbesManager = this;

		XMFLOAT3& minBounds = mSceneProbesMinBounds;
		XMFLOAT3& maxBounds = mSceneProbesMaxBounds;

		//global probe
		mGlobalDiffuseProbe = new ER_LightProbe(game, light, shadowMapper, DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE);
		mGlobalDiffuseProbe->SetIndex(-1);
		mGlobalDiffuseProbe->SetPosition(Vector3Helper::Zero);
		mGlobalDiffuseProbe->SetShaderInfoForConvolution(mConvolutionPS, mLinearSamplerState);

		mDiffuseProbesCountX = (maxBounds.x - minBounds.x) / DISTANCE_BETWEEN_DIFFUSE_PROBES + 1;
		mDiffuseProbesCountY = (maxBounds.y - minBounds.y) / DISTANCE_BETWEEN_DIFFUSE_PROBES + 1;
		mDiffuseProbesCountZ = (maxBounds.z - minBounds.z) / DISTANCE_BETWEEN_DIFFUSE_PROBES + 1;
		mDiffuseProbesCountTotal = mDiffuseProbesCountX * mDiffuseProbesCountY * mDiffuseProbesCountZ;
		assert(mDiffuseProbesCountTotal);
		mDiffuseProbes.reserve(mDiffuseProbesCountTotal);

		// cells setup
		for (int volumeIndex = 0; volumeIndex < NUM_PROBE_VOLUME_CASCADES; volumeIndex++)
		{
			assert((mDiffuseProbesCountX - 1) % VolumeProbeIndexSkips[volumeIndex] == 0);
			assert((mDiffuseProbesCountY - 1) % VolumeProbeIndexSkips[volumeIndex] == 0);
			assert((mDiffuseProbesCountZ - 1) % VolumeProbeIndexSkips[volumeIndex] == 0);

			mDiffuseProbesCellsCountX[volumeIndex] = (mDiffuseProbesCountX - 1) / VolumeProbeIndexSkips[volumeIndex];
			mDiffuseProbesCellsCountY[volumeIndex] = (mDiffuseProbesCountY - 1) / VolumeProbeIndexSkips[volumeIndex];
			mDiffuseProbesCellsCountZ[volumeIndex] = (mDiffuseProbesCountZ - 1) / VolumeProbeIndexSkips[volumeIndex];
			mDiffuseProbesCellsCountTotal[volumeIndex] = mDiffuseProbesCellsCountX[volumeIndex] * mDiffuseProbesCellsCountY[volumeIndex] * mDiffuseProbesCellsCountZ[volumeIndex];
			mDiffuseProbesCells[volumeIndex].resize(mDiffuseProbesCellsCountTotal[volumeIndex], {});

			assert(mDiffuseProbesCellsCountTotal[volumeIndex]);

			float probeCellPositionOffset = static_cast<float>(DISTANCE_BETWEEN_DIFFUSE_PROBES * VolumeProbeIndexSkips[volumeIndex]) / 2.0f;
			mDiffuseProbesCellBounds[volumeIndex] = {
				XMFLOAT3(-probeCellPositionOffset, -probeCellPositionOffset, -probeCellPositionOffset),
				XMFLOAT3(probeCellPositionOffset, probeCellPositionOffset, probeCellPositionOffset) };
			{
				for (int cellsY = 0; cellsY < mDiffuseProbesCellsCountY[volumeIndex]; cellsY++)
				{
					for (int cellsX = 0; cellsX < mDiffuseProbesCellsCountX[volumeIndex]; cellsX++)
					{
						for (int cellsZ = 0; cellsZ < mDiffuseProbesCellsCountZ[volumeIndex]; cellsZ++)
						{
							XMFLOAT3 pos = XMFLOAT3(
								minBounds.x + probeCellPositionOffset + cellsX * DISTANCE_BETWEEN_DIFFUSE_PROBES * VolumeProbeIndexSkips[volumeIndex],
								minBounds.y + probeCellPositionOffset + cellsY * DISTANCE_BETWEEN_DIFFUSE_PROBES * VolumeProbeIndexSkips[volumeIndex],
								minBounds.z + probeCellPositionOffset + cellsZ * DISTANCE_BETWEEN_DIFFUSE_PROBES * VolumeProbeIndexSkips[volumeIndex]);

							int index = cellsY * (mDiffuseProbesCellsCountX[volumeIndex] * mDiffuseProbesCellsCountZ[volumeIndex]) + cellsX * mDiffuseProbesCellsCountZ[volumeIndex] + cellsZ;
							mDiffuseProbesCells[volumeIndex][index].index = index;
							mDiffuseProbesCells[volumeIndex][index].position = pos;
						}
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
						minBounds.x + probesX * DISTANCE_BETWEEN_DIFFUSE_PROBES,
						minBounds.y + probesY * DISTANCE_BETWEEN_DIFFUSE_PROBES,
						minBounds.z + probesZ * DISTANCE_BETWEEN_DIFFUSE_PROBES);
					int index = probesY * (mDiffuseProbesCountX * mDiffuseProbesCountZ) + probesX * mDiffuseProbesCountZ + probesZ;
					mDiffuseProbes[index]->SetIndex(index);
					mDiffuseProbes[index]->SetPosition(pos);
					mDiffuseProbes[index]->SetShaderInfoForConvolution(mConvolutionPS, mLinearSamplerState);
					for (int volumeIndex = 0; volumeIndex < NUM_PROBE_VOLUME_CASCADES; volumeIndex++)
					{
						if (probesY % VolumeProbeIndexSkips[volumeIndex] == 0 &&
							probesX % VolumeProbeIndexSkips[volumeIndex] == 0 &&
							probesZ % VolumeProbeIndexSkips[volumeIndex] == 0)
							mDiffuseProbes[index]->SetIsTaggedByVolume(volumeIndex);
					}
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
		for (int volumeIndex = 0; volumeIndex < NUM_PROBE_VOLUME_CASCADES; volumeIndex++)
		{
			int* diffuseProbeCellsIndicesCPUBuffer = new int[mDiffuseProbesCellsCountTotal[volumeIndex] * PROBE_COUNT_PER_CELL];
			for (int probeIndex = 0; probeIndex < mDiffuseProbesCellsCountTotal[volumeIndex]; probeIndex++)
			{
				for (int indices = 0; indices < PROBE_COUNT_PER_CELL; indices++)
					diffuseProbeCellsIndicesCPUBuffer[probeIndex * PROBE_COUNT_PER_CELL + indices] = mDiffuseProbesCells[volumeIndex][probeIndex].lightProbeIndices[indices];
			}
			mDiffuseProbesCellsIndicesGPUBuffer[volumeIndex] = new ER_GPUBuffer(game.Direct3DDevice(), diffuseProbeCellsIndicesCPUBuffer,
				mDiffuseProbesCellsCountTotal[volumeIndex] * PROBE_COUNT_PER_CELL, sizeof(int),
				D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED);
			DeleteObjects(diffuseProbeCellsIndicesCPUBuffer);
			
			mDiffuseProbesTexArrayIndicesCPUBuffer[volumeIndex] = new int[mDiffuseProbesCountTotal];
			mDiffuseProbesTexArrayIndicesGPUBuffer[volumeIndex] = new ER_GPUBuffer(game.Direct3DDevice(), mDiffuseProbesTexArrayIndicesCPUBuffer[volumeIndex], mDiffuseProbesCountTotal, sizeof(int),
				D3D11_USAGE_DYNAMIC, D3D11_BIND_SHADER_RESOURCE, D3D11_CPU_ACCESS_WRITE, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED);

			std::string name = "Debug diffuse lightprobes " + std::to_string(volumeIndex) + " volume";
			auto result = scene->objects.insert(
				std::pair<std::string, ER_RenderingObject*>(name, new ER_RenderingObject(name, scene->objects.size(), game, camera,
					std::unique_ptr<Model>(new Model(game, Utility::GetFilePath("content\\models\\sphere_lowpoly.fbx"), true)), false, true))
			);

			if (!result.second)
				throw GameException("Could not add a diffuse probe global object into scene");

			MaterialShaderEntries shaderEntries;
			shaderEntries.vertexEntry += "_instancing";

			mDiffuseProbeRenderingObject[volumeIndex] = result.first->second;
			mDiffuseProbeRenderingObject[volumeIndex]->LoadMaterial(new ER_DebugLightProbeMaterial(game, shaderEntries, HAS_VERTEX_SHADER | HAS_PIXEL_SHADER, true), MaterialHelper::debugLightProbeMaterialName);
			mDiffuseProbeRenderingObject[volumeIndex]->LoadRenderBuffers();
			mDiffuseProbeRenderingObject[volumeIndex]->LoadInstanceBuffers();
			mDiffuseProbeRenderingObject[volumeIndex]->ResetInstanceData(mDiffuseProbesCountTotal, true);
			for (int i = 0; i < mDiffuseProbesCountTotal; i++) {
				XMMATRIX worldT = XMMatrixTranslation(mDiffuseProbes[i]->GetPosition().x, mDiffuseProbes[i]->GetPosition().y, mDiffuseProbes[i]->GetPosition().z);
				mDiffuseProbeRenderingObject[volumeIndex]->AddInstanceData(worldT);
			}
			mDiffuseProbeRenderingObject[volumeIndex]->UpdateInstanceBuffer(mDiffuseProbeRenderingObject[volumeIndex]->GetInstancesData());
		}

		mTempDiffuseCubemapFacesRT = new ER_GPUTexture(game.Direct3DDevice(), DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE_SIZE, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 1, -1, CUBEMAP_FACES_COUNT, true);
		mTempDiffuseCubemapFacesConvolutedRT = new ER_GPUTexture(game.Direct3DDevice(), DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE_SIZE, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 1, -1, CUBEMAP_FACES_COUNT, true);

		for (int i = 0; i < NUM_PROBE_VOLUME_CASCADES; i++)
		{
			mDiffuseCubemapArrayRT[i] = new ER_GPUTexture(game.Direct3DDevice(), DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE_SIZE, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
				D3D11_BIND_SHADER_RESOURCE, 1, -1, CUBEMAP_FACES_COUNT, true, mMaxProbesInVolumeCount);
		}
	}

	void ER_IlluminationProbeManager::SetupSpecularProbes(Game& game, Camera& camera, Scene* scene, DirectionalLight& light, ER_ShadowMapper& shadowMapper)
	{
		ER_MaterialSystems materialSystems;
		materialSystems.mCamera = &camera;
		materialSystems.mDirectionalLight = &light;
		materialSystems.mShadowMapper = &shadowMapper;
		materialSystems.mProbesManager = this;

		XMFLOAT3& minBounds = mSceneProbesMinBounds;
		XMFLOAT3& maxBounds = mSceneProbesMaxBounds;

		mSpecularProbesCountX = (maxBounds.x - minBounds.x) / DISTANCE_BETWEEN_SPECULAR_PROBES + 1;
		mSpecularProbesCountY = (maxBounds.y - minBounds.y) / DISTANCE_BETWEEN_SPECULAR_PROBES + 1;
		mSpecularProbesCountZ = (maxBounds.z - minBounds.z) / DISTANCE_BETWEEN_SPECULAR_PROBES + 1;
		mSpecularProbesCountTotal = mSpecularProbesCountX * mSpecularProbesCountY * mSpecularProbesCountZ;
		assert(mSpecularProbesCountTotal);
		mSpecularProbes.reserve(mSpecularProbesCountTotal);

		// cells setup
		for (int volumeIndex = 0; volumeIndex < NUM_PROBE_VOLUME_CASCADES; volumeIndex++)
		{
			assert((mSpecularProbesCountX - 1) % VolumeProbeIndexSkips[volumeIndex] == 0);
			assert((mSpecularProbesCountY - 1) % VolumeProbeIndexSkips[volumeIndex] == 0);
			assert((mSpecularProbesCountZ - 1) % VolumeProbeIndexSkips[volumeIndex] == 0);

			mSpecularProbesCellsCountX[volumeIndex] = (mSpecularProbesCountX - 1) / VolumeProbeIndexSkips[volumeIndex];
			mSpecularProbesCellsCountY[volumeIndex] = (mSpecularProbesCountY - 1) / VolumeProbeIndexSkips[volumeIndex];
			mSpecularProbesCellsCountZ[volumeIndex] = (mSpecularProbesCountZ - 1) / VolumeProbeIndexSkips[volumeIndex];
			mSpecularProbesCellsCountTotal[volumeIndex] = mSpecularProbesCellsCountX[volumeIndex] * mSpecularProbesCellsCountY[volumeIndex] * mSpecularProbesCellsCountZ[volumeIndex];
			mSpecularProbesCells[volumeIndex].resize(mSpecularProbesCellsCountTotal[volumeIndex], {});

			assert(mSpecularProbesCellsCountTotal[volumeIndex]);

			float probeCellPositionOffset = static_cast<float>(DISTANCE_BETWEEN_SPECULAR_PROBES * VolumeProbeIndexSkips[volumeIndex]) / 2.0f;
			mSpecularProbesCellBounds[volumeIndex] = {
				XMFLOAT3(-probeCellPositionOffset, -probeCellPositionOffset, -probeCellPositionOffset),
				XMFLOAT3(probeCellPositionOffset, probeCellPositionOffset, probeCellPositionOffset) };
			{
				for (int cellsY = 0; cellsY < mSpecularProbesCellsCountY[volumeIndex]; cellsY++)
				{
					for (int cellsX = 0; cellsX < mSpecularProbesCellsCountX[volumeIndex]; cellsX++)
					{
						for (int cellsZ = 0; cellsZ < mSpecularProbesCellsCountZ[volumeIndex]; cellsZ++)
						{
							XMFLOAT3 pos = XMFLOAT3(
								minBounds.x + probeCellPositionOffset + cellsX * DISTANCE_BETWEEN_SPECULAR_PROBES * VolumeProbeIndexSkips[volumeIndex],
								minBounds.y + probeCellPositionOffset + cellsY * DISTANCE_BETWEEN_SPECULAR_PROBES * VolumeProbeIndexSkips[volumeIndex],
								minBounds.z + probeCellPositionOffset + cellsZ * DISTANCE_BETWEEN_SPECULAR_PROBES * VolumeProbeIndexSkips[volumeIndex]);

							int index = cellsY * (mSpecularProbesCellsCountX[volumeIndex] * mSpecularProbesCellsCountZ[volumeIndex]) + cellsX * mSpecularProbesCellsCountZ[volumeIndex] + cellsZ;
							mSpecularProbesCells[volumeIndex][index].index = index;
							mSpecularProbesCells[volumeIndex][index].position = pos;
						}
					}
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
						minBounds.x + probesX * DISTANCE_BETWEEN_SPECULAR_PROBES,
						minBounds.y + probesY * DISTANCE_BETWEEN_SPECULAR_PROBES,
						minBounds.z + probesZ * DISTANCE_BETWEEN_SPECULAR_PROBES);
					int index = probesY * (mSpecularProbesCountX * mSpecularProbesCountZ) + probesX * mSpecularProbesCountZ + probesZ;
					mSpecularProbes[index]->SetIndex(index);
					mSpecularProbes[index]->SetPosition(pos);
					mSpecularProbes[index]->SetShaderInfoForConvolution(mConvolutionPS, mLinearSamplerState);
					for (int volumeIndex = 0; volumeIndex < NUM_PROBE_VOLUME_CASCADES; volumeIndex++)
					{
						if (probesY % VolumeProbeIndexSkips[volumeIndex] == 0 &&
							probesX % VolumeProbeIndexSkips[volumeIndex] == 0 &&
							probesZ % VolumeProbeIndexSkips[volumeIndex] == 0)
							mSpecularProbes[index]->SetIsTaggedByVolume(volumeIndex);
					}
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
		for (int volumeIndex = 0; volumeIndex < NUM_PROBE_VOLUME_CASCADES; volumeIndex++)
		{
			int* specularProbeCellsIndicesCPUBuffer = new int[mSpecularProbesCellsCountTotal[volumeIndex] * PROBE_COUNT_PER_CELL];
			for (int probeIndex = 0; probeIndex < mSpecularProbesCellsCountTotal[volumeIndex]; probeIndex++)
			{
				for (int indices = 0; indices < PROBE_COUNT_PER_CELL; indices++)
					specularProbeCellsIndicesCPUBuffer[probeIndex * PROBE_COUNT_PER_CELL + indices] = mSpecularProbesCells[volumeIndex][probeIndex].lightProbeIndices[indices];
			}
			mSpecularProbesCellsIndicesGPUBuffer[volumeIndex] = new ER_GPUBuffer(game.Direct3DDevice(), specularProbeCellsIndicesCPUBuffer,
				mSpecularProbesCellsCountTotal[volumeIndex] * PROBE_COUNT_PER_CELL, sizeof(int),
				D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED);
			DeleteObjects(specularProbeCellsIndicesCPUBuffer);

			mSpecularProbesTexArrayIndicesCPUBuffer[volumeIndex] = new int[mSpecularProbesCountTotal];
			mSpecularProbesTexArrayIndicesGPUBuffer[volumeIndex] = new ER_GPUBuffer(game.Direct3DDevice(), mSpecularProbesTexArrayIndicesCPUBuffer[volumeIndex], mSpecularProbesCountTotal, sizeof(int),
				D3D11_USAGE_DYNAMIC, D3D11_BIND_SHADER_RESOURCE, D3D11_CPU_ACCESS_WRITE, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED);

			std::string name = "Debug specular lightprobes " + std::to_string(volumeIndex) + " volume";
			auto result = scene->objects.insert(
				std::pair<std::string, ER_RenderingObject*>(name, new ER_RenderingObject(name, scene->objects.size(), game, camera,
					std::unique_ptr<Model>(new Model(game, Utility::GetFilePath("content\\models\\sphere_lowpoly.fbx"), true)), false, true))
			);

			if (!result.second)
				throw GameException("Could not add a specular probe global object into scene");
			
			MaterialShaderEntries shaderEntries;
			shaderEntries.vertexEntry += "_instancing";

			mSpecularProbeRenderingObject[volumeIndex] = result.first->second;
			mSpecularProbeRenderingObject[volumeIndex]->LoadMaterial(new ER_DebugLightProbeMaterial(game, shaderEntries, HAS_VERTEX_SHADER | HAS_PIXEL_SHADER, true), MaterialHelper::debugLightProbeMaterialName);
			mSpecularProbeRenderingObject[volumeIndex]->LoadRenderBuffers();
			mSpecularProbeRenderingObject[volumeIndex]->LoadInstanceBuffers();
			mSpecularProbeRenderingObject[volumeIndex]->ResetInstanceData(mSpecularProbesCountTotal, true);
			for (int i = 0; i < mSpecularProbesCountTotal; i++) {
				XMMATRIX worldT = XMMatrixTranslation(mSpecularProbes[i]->GetPosition().x, mSpecularProbes[i]->GetPosition().y, mSpecularProbes[i]->GetPosition().z);
				mSpecularProbeRenderingObject[volumeIndex]->AddInstanceData(worldT);
			}
			mSpecularProbeRenderingObject[volumeIndex]->UpdateInstanceBuffer(mSpecularProbeRenderingObject[volumeIndex]->GetInstancesData());
		}

		mTempSpecularCubemapFacesRT = new ER_GPUTexture(game.Direct3DDevice(), SPECULAR_PROBE_SIZE, SPECULAR_PROBE_SIZE, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, SPECULAR_PROBE_MIP_COUNT, -1, CUBEMAP_FACES_COUNT, true);
		mTempSpecularCubemapFacesConvolutedRT = new ER_GPUTexture(game.Direct3DDevice(), SPECULAR_PROBE_SIZE, SPECULAR_PROBE_SIZE, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, SPECULAR_PROBE_MIP_COUNT, -1, CUBEMAP_FACES_COUNT, true);

		for (int i = 0; i < NUM_PROBE_VOLUME_CASCADES; i++)
		{
			mSpecularCubemapArrayRT[i] = new ER_GPUTexture(game.Direct3DDevice(), SPECULAR_PROBE_SIZE, SPECULAR_PROBE_SIZE, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
				D3D11_BIND_SHADER_RESOURCE, SPECULAR_PROBE_MIP_COUNT, -1, CUBEMAP_FACES_COUNT, true, mMaxProbesInVolumeCount);
		}
	}

	void ER_IlluminationProbeManager::AddProbeToCells(ER_LightProbe* aProbe, ER_ProbeType aType, const XMFLOAT3& minBounds, const XMFLOAT3& maxBounds)
	{
		int index = aProbe->GetIndex();
		if (aType == DIFFUSE_PROBE)
		{
			for (int volumeIndex = 0; volumeIndex < NUM_PROBE_VOLUME_CASCADES; volumeIndex++)
			{
				// brute force O(cells * probes)
				for (auto& cell : mDiffuseProbesCells[volumeIndex])
				{
					if (IsProbeInCell(aProbe, cell, mDiffuseProbesCellBounds[volumeIndex]) && aProbe->IsTaggedByVolume(volumeIndex))
						cell.lightProbeIndices.push_back(index);

					if (cell.lightProbeIndices.size() > PROBE_COUNT_PER_CELL)
						throw GameException("Too many diffuse probes per cell!");
				}
			}
		}
		else
		{
			for (int volumeIndex = 0; volumeIndex < NUM_PROBE_VOLUME_CASCADES; volumeIndex++)
			{
				// brute force O(cells * probes)
				for (auto& cell : mSpecularProbesCells[volumeIndex])
				{
					if (IsProbeInCell(aProbe, cell, mSpecularProbesCellBounds[volumeIndex]) && aProbe->IsTaggedByVolume(volumeIndex))
						cell.lightProbeIndices.push_back(index);

					if (cell.lightProbeIndices.size() > PROBE_COUNT_PER_CELL)
						throw GameException("Too many specular probes per cell!");
				}
			}
		}
	}

	// Fast uniform-grid searching approach (WARNING: can not do multiple indices per pos. (i.e., when pos. is on the edge of several cells))
	int ER_IlluminationProbeManager::GetCellIndex(const XMFLOAT3& pos, ER_ProbeType aType, int volumeIndex)
	{
		int finalIndex = -1;

		if (aType == DIFFUSE_PROBE)
		{
			int xIndex = (pos.x - mSceneProbesMinBounds.x) / (DISTANCE_BETWEEN_DIFFUSE_PROBES * VolumeProbeIndexSkips[volumeIndex]);
			int yIndex = (pos.y - mSceneProbesMinBounds.y) / (DISTANCE_BETWEEN_DIFFUSE_PROBES * VolumeProbeIndexSkips[volumeIndex]);
			int zIndex = (pos.z - mSceneProbesMinBounds.z) / (DISTANCE_BETWEEN_DIFFUSE_PROBES * VolumeProbeIndexSkips[volumeIndex]);

			if (xIndex < 0 || xIndex > mDiffuseProbesCellsCountX[volumeIndex])
				return -1;
			if (yIndex < 0 || yIndex > mDiffuseProbesCellsCountY[volumeIndex])
				return -1;
			if (zIndex < 0 || zIndex > mDiffuseProbesCellsCountZ[volumeIndex])
				return -1;

			//little hacky way to prevent from out-of-bounds
			if (xIndex == mDiffuseProbesCellsCountX[volumeIndex])
				xIndex = mDiffuseProbesCellsCountX[volumeIndex] - 1;
			if (yIndex == mDiffuseProbesCellsCountY[volumeIndex])
				yIndex = mDiffuseProbesCellsCountY[volumeIndex] - 1;
			if (zIndex == mDiffuseProbesCellsCountZ[volumeIndex])
				zIndex = mDiffuseProbesCellsCountZ[volumeIndex] - 1;

			finalIndex = yIndex * (mDiffuseProbesCellsCountX[volumeIndex] * mDiffuseProbesCellsCountZ[volumeIndex]) + xIndex * mDiffuseProbesCellsCountZ[volumeIndex] + zIndex;

			if (finalIndex >= mDiffuseProbesCellsCountTotal[volumeIndex])
				return -1;
		}
		else
		{
			int xIndex = (pos.x - mSceneProbesMinBounds.x) / (DISTANCE_BETWEEN_SPECULAR_PROBES * VolumeProbeIndexSkips[volumeIndex]);
			int yIndex = (pos.y - mSceneProbesMinBounds.y) / (DISTANCE_BETWEEN_SPECULAR_PROBES * VolumeProbeIndexSkips[volumeIndex]);
			int zIndex = (pos.z - mSceneProbesMinBounds.z) / (DISTANCE_BETWEEN_SPECULAR_PROBES * VolumeProbeIndexSkips[volumeIndex]);

			if (xIndex < 0 || xIndex > mSpecularProbesCellsCountX[volumeIndex])
				return -1;
			if (yIndex < 0 || yIndex > mSpecularProbesCellsCountY[volumeIndex])
				return -1;
			if (zIndex < 0 || zIndex > mSpecularProbesCellsCountZ[volumeIndex])
				return -1;

			//little hacky way to prevent from out-of-bounds
			if (xIndex == mSpecularProbesCellsCountX[volumeIndex])
				xIndex = mSpecularProbesCellsCountX[volumeIndex] - 1;
			if (yIndex == mSpecularProbesCellsCountY[volumeIndex])
				yIndex = mSpecularProbesCellsCountY[volumeIndex] - 1;
			if (zIndex == mSpecularProbesCellsCountZ[volumeIndex])
				zIndex = mSpecularProbesCellsCountZ[volumeIndex] - 1;

			finalIndex = yIndex * (mSpecularProbesCellsCountX[volumeIndex] * mSpecularProbesCellsCountZ[volumeIndex]) + xIndex * mSpecularProbesCellsCountZ[volumeIndex] + zIndex;

			if (finalIndex >= mSpecularProbesCellsCountTotal[volumeIndex])
				return -1;

		}

		return finalIndex;
	}

	const DirectX::XMFLOAT4& ER_IlluminationProbeManager::GetProbesCellsCount(ER_ProbeType aType, int volumeIndex)
	{
		if (aType == DIFFUSE_PROBE)
			return XMFLOAT4(mDiffuseProbesCellsCountX[volumeIndex], mDiffuseProbesCellsCountY[volumeIndex], mDiffuseProbesCellsCountZ[volumeIndex], mDiffuseProbesCellsCountTotal[volumeIndex]);
		else
			return XMFLOAT4(mSpecularProbesCellsCountX[volumeIndex], mSpecularProbesCellsCountY[volumeIndex], mSpecularProbesCellsCountZ[volumeIndex], mSpecularProbesCellsCountTotal[volumeIndex]);
	}

	float ER_IlluminationProbeManager::GetProbesIndexSkip(int volumeIndex)
	{
		return static_cast<float>(VolumeProbeIndexSkips[volumeIndex]);
	}

	bool ER_IlluminationProbeManager::IsProbeInCell(ER_LightProbe* aProbe, ER_LightProbeCell& aCell, ER_AABB& aCellBounds)
	{
		XMFLOAT3 pos = aProbe->GetPosition();

		XMFLOAT3 maxBounds = XMFLOAT3(
			aCellBounds.second.x + aCell.position.x,
			aCellBounds.second.y + aCell.position.y,
			aCellBounds.second.z + aCell.position.z);

		XMFLOAT3 minBounds = XMFLOAT3(
			aCellBounds.first.x + aCell.position.x,
			aCellBounds.first.y + aCell.position.y,
			aCellBounds.first.z + aCell.position.z);

		return	(pos.x <= maxBounds.x && pos.x >= minBounds.x) &&
				(pos.y <= maxBounds.y && pos.y >= minBounds.y) &&
				(pos.z <= maxBounds.z && pos.z >= minBounds.z);
	}

	void ER_IlluminationProbeManager::ComputeOrLoadProbes(Game& game, const GameTime& gameTime, ProbesRenderingObjectsInfo& aObjects, Skybox* skybox)
	{
		int numThreads = std::thread::hardware_concurrency();
		assert(numThreads > 0);

		if (!mGlobalDiffuseProbeReady)
		{
			std::wstring diffuseProbesPath = mLevelPath + L"diffuse_probes\\";
			if (!mGlobalDiffuseProbe->LoadProbeFromDisk(game, diffuseProbesPath))
				mGlobalDiffuseProbe->Compute(game, gameTime, mTempDiffuseCubemapFacesRT, mTempDiffuseCubemapFacesConvolutedRT, mTempDiffuseCubemapDepthBuffers, diffuseProbesPath, aObjects, mQuadRenderer, skybox);
			
			mGlobalDiffuseProbeReady = true;
		}

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
					probe->Compute(game, gameTime, mTempDiffuseCubemapFacesRT, mTempDiffuseCubemapFacesConvolutedRT, mTempDiffuseCubemapDepthBuffers, diffuseProbesPath, aObjects, mQuadRenderer, skybox);
			}
			
			mDiffuseProbesReady = true;
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
					probe->Compute(game, gameTime, mTempSpecularCubemapFacesRT, mTempSpecularCubemapFacesConvolutedRT, mTempSpecularCubemapDepthBuffers, specularProbesPath, aObjects, mQuadRenderer, skybox);
			}
			mSpecularProbesReady = true;
		}
	}

	void ER_IlluminationProbeManager::DrawDebugProbes(ER_ProbeType aType, int volumeIndex)
	{
		ER_RenderingObject* probeObject = aType == DIFFUSE_PROBE ? mDiffuseProbeRenderingObject[volumeIndex] : mSpecularProbeRenderingObject[volumeIndex];
		bool ready = aType == DIFFUSE_PROBE ? mDiffuseProbesReady : mSpecularProbesReady;
		
		ER_MaterialSystems materialSystems;
		materialSystems.mProbesManager = this;

		if (probeObject && ready)
		{
			auto materialInfo = probeObject->GetNewMaterials().find(MaterialHelper::debugLightProbeMaterialName);
			if (materialInfo != probeObject->GetNewMaterials().end())
			{
				static_cast<ER_DebugLightProbeMaterial*>(materialInfo->second)->PrepareForRendering(materialSystems, probeObject, 0, static_cast<int>(aType), volumeIndex);
				probeObject->Draw(MaterialHelper::debugLightProbeMaterialName);
			}
		}

	}

	void ER_IlluminationProbeManager::DrawDebugProbesVolumeGizmo(ER_ProbeType aType, int volumeIndex)
	{
		if (aType == DIFFUSE_PROBE && mDebugDiffuseProbeVolumeGizmo[volumeIndex])
			mDebugDiffuseProbeVolumeGizmo[volumeIndex]->Draw();
		else if (aType == SPECULAR_PROBE && mDebugSpecularProbeVolumeGizmo[volumeIndex])
			mDebugSpecularProbeVolumeGizmo[volumeIndex]->Draw();
	}

	void ER_IlluminationProbeManager::UpdateProbesByType(Game& game, ER_ProbeType aType)
	{
		std::vector<ER_LightProbe*>& probes = (aType == DIFFUSE_PROBE) ? mDiffuseProbes : mSpecularProbes;
		auto minBounds = (aType == DIFFUSE_PROBE) ? mCurrentDiffuseVolumesMinBounds : mCurrentSpecularVolumesMinBounds;
		auto maxBounds = (aType == DIFFUSE_PROBE) ? mCurrentDiffuseVolumesMaxBounds : mCurrentSpecularVolumesMaxBounds;

		for (int volumeIndex = 0; volumeIndex < NUM_PROBE_VOLUME_CASCADES; volumeIndex++)
		{
			ER_RenderingObject* probeRenderingObject = (aType == DIFFUSE_PROBE) ? mDiffuseProbeRenderingObject[volumeIndex] : mSpecularProbeRenderingObject[volumeIndex];
			if (aType == DIFFUSE_PROBE)
			{
				if (!mDiffuseProbesReady)
					return;

				mNonCulledDiffuseProbesCount[volumeIndex] = 0;
				mNonCulledDiffuseProbesIndices[volumeIndex].clear();
			}
			else
			{
				if (!mSpecularProbesReady)
					return;

				mNonCulledSpecularProbesCount[volumeIndex] = 0;
				mNonCulledSpecularProbesIndices[volumeIndex].clear();
			}

			for (auto& lightProbe : probes)
				lightProbe->CPUCullAgainstProbeBoundingVolume(volumeIndex, minBounds[volumeIndex], maxBounds[volumeIndex]);

			if (probeRenderingObject)
			{
				auto& oldInstancedData = probeRenderingObject->GetInstancesData();
				assert(oldInstancedData.size() == probes.size());

				for (int i = 0; i < oldInstancedData.size(); i++)
				{
					bool isCulled = probes[i]->IsCulled(volumeIndex) || !probes[i]->IsTaggedByVolume(volumeIndex);
					//writing culling flag to [4][4] of world instanced matrix
					oldInstancedData[i].World._44 = isCulled ? 1.0f : 0.0f;
					//writing cubemap index to [0][0] of world instanced matrix (since we don't need scale)
					oldInstancedData[i].World._11 = -1.0f;
					if (!isCulled)
					{
						if (aType == DIFFUSE_PROBE)
						{
							oldInstancedData[i].World._11 = static_cast<float>(mNonCulledDiffuseProbesCount[volumeIndex]);
							mNonCulledDiffuseProbesCount[volumeIndex]++;
							mNonCulledDiffuseProbesIndices[volumeIndex].push_back(i);
						}
						else
						{
							oldInstancedData[i].World._11 = static_cast<float>(mNonCulledSpecularProbesCount[volumeIndex]);
							mNonCulledSpecularProbesCount[volumeIndex]++;
							mNonCulledSpecularProbesIndices[volumeIndex].push_back(i);
						}
					}
				}

				if (volumeIndex == 0)
					probeRenderingObject->UpdateInstanceBuffer(oldInstancedData);
			}

			auto context = game.Direct3DDeviceContext();
			if (aType == DIFFUSE_PROBE)
			{
				for (int i = 0; i < mDiffuseProbesCountTotal; i++)
					mDiffuseProbesTexArrayIndicesCPUBuffer[volumeIndex][i] = -1;

				for (int i = 0; i < mMaxProbesInVolumeCount; i++)
				{
					if (i < mNonCulledDiffuseProbesIndices[volumeIndex].size() && i < probes.size())
					{
						int probeIndex = mNonCulledDiffuseProbesIndices[volumeIndex][i];
						for (int cubeI = 0; cubeI < CUBEMAP_FACES_COUNT; cubeI++)
							context->CopySubresourceRegion(mDiffuseCubemapArrayRT[volumeIndex]->GetTexture2D(), D3D11CalcSubresource(0, cubeI + CUBEMAP_FACES_COUNT * i, 1), 0, 0, 0,
								probes[probeIndex]->GetCubemapTexture2D(), D3D11CalcSubresource(0, cubeI, 1), NULL);

						mDiffuseProbesTexArrayIndicesCPUBuffer[volumeIndex][probeIndex] = CUBEMAP_FACES_COUNT * i;
					}
				}
				mDiffuseProbesTexArrayIndicesGPUBuffer[volumeIndex]->Update(context, mDiffuseProbesTexArrayIndicesCPUBuffer[volumeIndex], sizeof(mDiffuseProbesTexArrayIndicesCPUBuffer[volumeIndex][0]) * mDiffuseProbesCountTotal, D3D11_MAP_WRITE_DISCARD);
			}
			else
			{
				for (int i = 0; i < mSpecularProbesCountTotal; i++)
					mSpecularProbesTexArrayIndicesCPUBuffer[volumeIndex][i] = -1;

				for (int i = 0; i < mMaxProbesInVolumeCount; i++)
				{
					if (i < mNonCulledSpecularProbesIndices[volumeIndex].size() && i < probes.size())
					{
						int probeIndex = mNonCulledSpecularProbesIndices[volumeIndex][i];
						for (int cubeI = 0; cubeI < CUBEMAP_FACES_COUNT; cubeI++)
						{
							for (int mip = 0; mip < SPECULAR_PROBE_MIP_COUNT; mip++)
							{
								context->CopySubresourceRegion(mSpecularCubemapArrayRT[volumeIndex]->GetTexture2D(), D3D11CalcSubresource(mip, cubeI + CUBEMAP_FACES_COUNT * i, SPECULAR_PROBE_MIP_COUNT), 0, 0, 0,
									probes[probeIndex]->GetCubemapTexture2D(), D3D11CalcSubresource(mip, cubeI, SPECULAR_PROBE_MIP_COUNT), NULL);
							}
						}
						mSpecularProbesTexArrayIndicesCPUBuffer[volumeIndex][probeIndex] = CUBEMAP_FACES_COUNT * i;
					}
				}
				mSpecularProbesTexArrayIndicesGPUBuffer[volumeIndex]->Update(context, mSpecularProbesTexArrayIndicesCPUBuffer[volumeIndex], sizeof(mSpecularProbesTexArrayIndicesCPUBuffer[volumeIndex][0]) * mSpecularProbesCountTotal, D3D11_MAP_WRITE_DISCARD);
			}
		}
	}

	void ER_IlluminationProbeManager::UpdateProbes(Game& game)
	{
		//int difProbeCellIndexCamera = GetCellIndex(mMainCamera.Position(), DIFFUSE_PROBE);
		assert(mMaxProbesInVolumeCount > 0);
		
		for (int volumeIndex = 0; volumeIndex < NUM_PROBE_VOLUME_CASCADES; volumeIndex++)
		{
			mDebugDiffuseProbeVolumeGizmo[volumeIndex]->SetPosition(mMainCamera.Position());
			mDebugDiffuseProbeVolumeGizmo[volumeIndex]->Update();	
			mDebugSpecularProbeVolumeGizmo[volumeIndex]->SetPosition(mMainCamera.Position());
			mDebugSpecularProbeVolumeGizmo[volumeIndex]->Update();

			mCurrentDiffuseVolumesMaxBounds[volumeIndex] = XMFLOAT3(
				DiffuseProbesVolumeCascadeSizes[volumeIndex] + mMainCamera.Position().x,
				DiffuseProbesVolumeCascadeSizes[volumeIndex] + mMainCamera.Position().y,
				DiffuseProbesVolumeCascadeSizes[volumeIndex] + mMainCamera.Position().z);

			mCurrentDiffuseVolumesMinBounds[volumeIndex] = XMFLOAT3(
				-DiffuseProbesVolumeCascadeSizes[volumeIndex] + mMainCamera.Position().x,
				-DiffuseProbesVolumeCascadeSizes[volumeIndex] + mMainCamera.Position().y,
				-DiffuseProbesVolumeCascadeSizes[volumeIndex] + mMainCamera.Position().z);

			mCurrentSpecularVolumesMaxBounds[volumeIndex] = XMFLOAT3(
				SpecularProbesVolumeCascadeSizes[volumeIndex] + mMainCamera.Position().x,
				SpecularProbesVolumeCascadeSizes[volumeIndex] + mMainCamera.Position().y,
				SpecularProbesVolumeCascadeSizes[volumeIndex] + mMainCamera.Position().z);

			mCurrentSpecularVolumesMinBounds[volumeIndex] = XMFLOAT3(
				-SpecularProbesVolumeCascadeSizes[volumeIndex] + mMainCamera.Position().x,
				-SpecularProbesVolumeCascadeSizes[volumeIndex] + mMainCamera.Position().y,
				-SpecularProbesVolumeCascadeSizes[volumeIndex] + mMainCamera.Position().z);
		}
		UpdateProbesByType(game, DIFFUSE_PROBE);
		UpdateProbesByType(game, SPECULAR_PROBE);
	}
}

