#include "ER_IlluminationProbeManager.h"
#include "Game.h"
#include "GameTime.h"
#include "GameException.h"
#include "Utility.h"
#include "DirectionalLight.h"
#include "MatrixHelper.h"
#include "MaterialHelper.h"
#include "VectorHelper.h"
#include "StandardLightingMaterial.h"
#include "ShaderCompiler.h"
#include "RenderingObject.h"
#include "Model.h"
#include "Scene.h"
#include "Material.h"
#include "Effect.h"
#include "DebugLightProbeMaterial.h"
#include "RenderableAABB.h"
#include "ER_GPUBuffer.h"
#include "ER_LightProbe.h"
#include "QuadRenderer.h"

namespace Library
{
	ER_IlluminationProbeManager::ER_IlluminationProbeManager(Game& game, Camera& camera, Scene* scene, DirectionalLight& light, ShadowMapper& shadowMapper)
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

		mQuadRenderer = new QuadRenderer(game.Direct3DDevice());
		{
			ID3DBlob* blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\IBL\\ProbeConvolution.hlsl").c_str(), "VSMain", "vs_5_0", &blob)))
				throw GameException("Failed to load VSMain from shader: ProbeConvolution.hlsl!");
			if (FAILED(game.Direct3DDevice()->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mConvolutionVS)))
				throw GameException("Failed to create vertex shader from ProbeConvolution.hlsl!");

			//TODO move to QuadRenderer
			{
				D3D11_INPUT_ELEMENT_DESC inputLayoutDesc[2];
				inputLayoutDesc[0].SemanticName = "POSITION";
				inputLayoutDesc[0].SemanticIndex = 0;
				inputLayoutDesc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
				inputLayoutDesc[0].InputSlot = 0;
				inputLayoutDesc[0].AlignedByteOffset = 0;
				inputLayoutDesc[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
				inputLayoutDesc[0].InstanceDataStepRate = 0;

				inputLayoutDesc[1].SemanticName = "TEXCOORD";
				inputLayoutDesc[1].SemanticIndex = 0;
				inputLayoutDesc[1].Format = DXGI_FORMAT_R32G32_FLOAT;
				inputLayoutDesc[1].InputSlot = 0;
				inputLayoutDesc[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
				inputLayoutDesc[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
				inputLayoutDesc[1].InstanceDataStepRate = 0;

				int numElements = sizeof(inputLayoutDesc) / sizeof(inputLayoutDesc[0]);
				game.Direct3DDevice()->CreateInputLayout(inputLayoutDesc, numElements, blob->GetBufferPointer(), blob->GetBufferSize(), &mInputLayout);
			}
			blob->Release();

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
			mDebugProbeVolumeGizmo[volumeIndex] = new RenderableAABB(game, mMainCamera, XMFLOAT4(0.44f, 0.2f, 0.64f, 1.0f));
			mDebugProbeVolumeGizmo[volumeIndex]->Initialize();
			mDebugProbeVolumeGizmo[volumeIndex]->InitializeGeometry({
				XMFLOAT3(-ProbesVolumeCascadeSizes[volumeIndex], -ProbesVolumeCascadeSizes[volumeIndex], -ProbesVolumeCascadeSizes[volumeIndex]),
				XMFLOAT3(ProbesVolumeCascadeSizes[volumeIndex], ProbesVolumeCascadeSizes[volumeIndex], ProbesVolumeCascadeSizes[volumeIndex]) }, XMMatrixScaling(1, 1, 1));
			mDebugProbeVolumeGizmo[volumeIndex]->SetPosition(mMainCamera.Position());
		}
	
		// TODO Load a pre-computed Integration Map
		if (FAILED(DirectX::CreateDDSTextureFromFile(game.Direct3DDevice(), game.Direct3DDeviceContext(),
			Utility::GetFilePath(L"content\\textures\\skyboxes\\textureBrdf.dds").c_str(), nullptr, &mIntegrationMapTextureSRV)))
			throw GameException("Failed to create Integration Texture.");

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

		DeleteObject(mQuadRenderer);
		ReleaseObject(mConvolutionVS);
		ReleaseObject(mConvolutionPS);
		ReleaseObject(mInputLayout);
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
			DeleteObject(mDebugProbeVolumeGizmo[i]);

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

	void ER_IlluminationProbeManager::SetupDiffuseProbes(Game& game, Camera& camera, Scene* scene, DirectionalLight& light, ShadowMapper& shadowMapper)
	{
		XMFLOAT3& minBounds = mSceneProbesMinBounds;
		XMFLOAT3& maxBounds = mSceneProbesMaxBounds;

		//global probe
		mGlobalDiffuseProbe = new ER_LightProbe(game, light, shadowMapper, DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE);
		mGlobalDiffuseProbe->SetIndex(-1);
		mGlobalDiffuseProbe->SetPosition(Vector3Helper::Zero);
		mGlobalDiffuseProbe->SetShaderInfoForConvolution(mConvolutionVS, mConvolutionPS, mInputLayout, mLinearSamplerState);

		//TODO max allowed amount of cubemap textures in an array (DX)
		mMaxNonCulledDiffuseProbesCountPerAxis = std::cbrt(2048 / 6);
		mMaxNonCulledDiffuseProbesCount = mMaxNonCulledDiffuseProbesCountPerAxis * mMaxNonCulledDiffuseProbesCountPerAxis * mMaxNonCulledDiffuseProbesCountPerAxis;

		mDiffuseProbesCountX = (maxBounds.x - minBounds.x) / DISTANCE_BETWEEN_DIFFUSE_PROBES + 1;
		mDiffuseProbesCountY = (maxBounds.y - minBounds.y) / DISTANCE_BETWEEN_DIFFUSE_PROBES + 1;
		mDiffuseProbesCountZ = (maxBounds.z - minBounds.z) / DISTANCE_BETWEEN_DIFFUSE_PROBES + 1;
		mDiffuseProbesCountTotal = mDiffuseProbesCountX * mDiffuseProbesCountY * mDiffuseProbesCountZ;
		assert(mDiffuseProbesCountTotal);
		mDiffuseProbes.reserve(mDiffuseProbesCountTotal);

		// cells setup
		for (int volumeIndex = 0; volumeIndex < NUM_PROBE_VOLUME_CASCADES; volumeIndex++)
		{
			assert((mDiffuseProbesCountX - 1) % mDiffuseProbeIndexSkip[volumeIndex] == 0);
			assert((mDiffuseProbesCountY - 1) % mDiffuseProbeIndexSkip[volumeIndex] == 0);
			assert((mDiffuseProbesCountZ - 1) % mDiffuseProbeIndexSkip[volumeIndex] == 0);

			mDiffuseProbesCellsCountX[volumeIndex] = (mDiffuseProbesCountX - 1) / mDiffuseProbeIndexSkip[volumeIndex];
			mDiffuseProbesCellsCountY[volumeIndex] = (mDiffuseProbesCountY - 1) / mDiffuseProbeIndexSkip[volumeIndex];
			mDiffuseProbesCellsCountZ[volumeIndex] = (mDiffuseProbesCountZ - 1) / mDiffuseProbeIndexSkip[volumeIndex];
			mDiffuseProbesCellsCountTotal[volumeIndex] = mDiffuseProbesCellsCountX[volumeIndex] * mDiffuseProbesCellsCountY[volumeIndex] * mDiffuseProbesCellsCountZ[volumeIndex];
			mDiffuseProbesCells[volumeIndex].resize(mDiffuseProbesCellsCountTotal[volumeIndex], {});

			assert(mDiffuseProbesCellsCountTotal[volumeIndex]);

			float probeCellPositionOffset = static_cast<float>(DISTANCE_BETWEEN_DIFFUSE_PROBES * mDiffuseProbeIndexSkip[volumeIndex]) / 2.0f;
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
								minBounds.x + probeCellPositionOffset + cellsX * DISTANCE_BETWEEN_DIFFUSE_PROBES * mDiffuseProbeIndexSkip[volumeIndex],
								minBounds.y + probeCellPositionOffset + cellsY * DISTANCE_BETWEEN_DIFFUSE_PROBES * mDiffuseProbeIndexSkip[volumeIndex],
								minBounds.z + probeCellPositionOffset + cellsZ * DISTANCE_BETWEEN_DIFFUSE_PROBES * mDiffuseProbeIndexSkip[volumeIndex]);

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
					mDiffuseProbes[index]->SetShaderInfoForConvolution(mConvolutionVS, mConvolutionPS, mInputLayout, mLinearSamplerState);
					for (int volumeIndex = 0; volumeIndex < NUM_PROBE_VOLUME_CASCADES; volumeIndex++)
					{
						if (probesY % mDiffuseProbeIndexSkip[volumeIndex] == 0 &&
							probesX % mDiffuseProbeIndexSkip[volumeIndex] == 0 &&
							probesZ % mDiffuseProbeIndexSkip[volumeIndex] == 0)
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
				std::pair<std::string, Rendering::RenderingObject*>(name, new Rendering::RenderingObject(name, scene->objects.size(), game, camera,
					std::unique_ptr<Model>(new Model(game, Utility::GetFilePath("content\\models\\sphere_lowpoly.fbx"), true)), false, true))
			);

			if (!result.second)
				throw GameException("Could not add a diffuse probe global object into scene");

			Effect* diffuseLightProbeEffect = new Effect(game); // deleted when material is deleted
			diffuseLightProbeEffect->CompileFromFile(Utility::GetFilePath(Utility::ToWideString("content\\effects\\DebugLightProbe.fx")));

			mDiffuseProbeRenderingObject[volumeIndex] = result.first->second;
			mDiffuseProbeRenderingObject[volumeIndex]->LoadMaterial(new Rendering::DebugLightProbeMaterial(), diffuseLightProbeEffect, MaterialHelper::debugLightProbeMaterialName);
			mDiffuseProbeRenderingObject[volumeIndex]->LoadRenderBuffers();
			mDiffuseProbeRenderingObject[volumeIndex]->LoadInstanceBuffers();
			mDiffuseProbeRenderingObject[volumeIndex]->ResetInstanceData(mDiffuseProbesCountTotal, true);
			for (int i = 0; i < mDiffuseProbesCountTotal; i++) {
				XMMATRIX worldT = XMMatrixTranslation(mDiffuseProbes[i]->GetPosition().x, mDiffuseProbes[i]->GetPosition().y, mDiffuseProbes[i]->GetPosition().z);
				mDiffuseProbeRenderingObject[volumeIndex]->AddInstanceData(worldT);
			}
			mDiffuseProbeRenderingObject[volumeIndex]->UpdateInstanceBuffer(mDiffuseProbeRenderingObject[volumeIndex]->GetInstancesData());
			mDiffuseProbeRenderingObject[volumeIndex]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::debugLightProbeMaterialName,
				[&, _volumeIndex = volumeIndex](int meshIndex) { UpdateDebugLightProbeMaterialVariables(mDiffuseProbeRenderingObject[_volumeIndex], meshIndex, DIFFUSE_PROBE, _volumeIndex); });
		}

		mTempDiffuseCubemapFacesRT = new ER_GPUTexture(game.Direct3DDevice(), DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE_SIZE, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 1, -1, CUBEMAP_FACES_COUNT, true);
		mTempDiffuseCubemapFacesConvolutedRT = new ER_GPUTexture(game.Direct3DDevice(), DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE_SIZE, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 1, -1, CUBEMAP_FACES_COUNT, true);

		for (int i = 0; i < NUM_PROBE_VOLUME_CASCADES; i++)
		{
			mDiffuseCubemapArrayRT[i] = new ER_GPUTexture(game.Direct3DDevice(), DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE_SIZE, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
				D3D11_BIND_SHADER_RESOURCE, 1, -1, CUBEMAP_FACES_COUNT, true, mMaxNonCulledDiffuseProbesCount);
		}
	}

	void ER_IlluminationProbeManager::SetupSpecularProbes(Game& game, Camera& camera, Scene* scene, DirectionalLight& light, ShadowMapper& shadowMapper)
	{
		XMFLOAT3& minBounds = mSceneProbesMinBounds;
		XMFLOAT3& maxBounds = mSceneProbesMaxBounds;

		//TODO max allowed amount of cubemap textures in an array (DX)
		mMaxNonCulledSpecularProbesCountPerAxis = std::cbrt(2048 / 6);
		mMaxNonCulledSpecularProbesCount = mMaxNonCulledSpecularProbesCountPerAxis * mMaxNonCulledSpecularProbesCountPerAxis * mMaxNonCulledSpecularProbesCountPerAxis;

		mSpecularProbesCountX = (maxBounds.x - minBounds.x) / DISTANCE_BETWEEN_SPECULAR_PROBES + 1;
		mSpecularProbesCountY = (maxBounds.y - minBounds.y) / DISTANCE_BETWEEN_SPECULAR_PROBES + 1;
		mSpecularProbesCountZ = (maxBounds.z - minBounds.z) / DISTANCE_BETWEEN_SPECULAR_PROBES + 1;
		mSpecularProbesCountTotal = mSpecularProbesCountX * mSpecularProbesCountY * mSpecularProbesCountZ;
		assert(mSpecularProbesCountTotal);
		mSpecularProbes.reserve(mSpecularProbesCountTotal);

		// cells setup
		for (int volumeIndex = 0; volumeIndex < NUM_PROBE_VOLUME_CASCADES; volumeIndex++)
		{
			assert((mSpecularProbesCountX - 1) % mSpecularProbeIndexSkip[volumeIndex] == 0);
			assert((mSpecularProbesCountY - 1) % mSpecularProbeIndexSkip[volumeIndex] == 0);
			assert((mSpecularProbesCountZ - 1) % mSpecularProbeIndexSkip[volumeIndex] == 0);

			mSpecularProbesCellsCountX[volumeIndex] = (mSpecularProbesCountX - 1) / mSpecularProbeIndexSkip[volumeIndex];
			mSpecularProbesCellsCountY[volumeIndex] = (mSpecularProbesCountY - 1) / mSpecularProbeIndexSkip[volumeIndex];
			mSpecularProbesCellsCountZ[volumeIndex] = (mSpecularProbesCountZ - 1) / mSpecularProbeIndexSkip[volumeIndex];
			mSpecularProbesCellsCountTotal[volumeIndex] = mSpecularProbesCellsCountX[volumeIndex] * mSpecularProbesCellsCountY[volumeIndex] * mSpecularProbesCellsCountZ[volumeIndex];
			mSpecularProbesCells[volumeIndex].resize(mSpecularProbesCellsCountTotal[volumeIndex], {});

			assert(mSpecularProbesCellsCountTotal[volumeIndex]);

			float probeCellPositionOffset = static_cast<float>(DISTANCE_BETWEEN_SPECULAR_PROBES * mSpecularProbeIndexSkip[volumeIndex]) / 2.0f;
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
								minBounds.x + probeCellPositionOffset + cellsX * DISTANCE_BETWEEN_SPECULAR_PROBES * mSpecularProbeIndexSkip[volumeIndex],
								minBounds.y + probeCellPositionOffset + cellsY * DISTANCE_BETWEEN_SPECULAR_PROBES * mSpecularProbeIndexSkip[volumeIndex],
								minBounds.z + probeCellPositionOffset + cellsZ * DISTANCE_BETWEEN_SPECULAR_PROBES * mSpecularProbeIndexSkip[volumeIndex]);

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
					mSpecularProbes[index]->SetShaderInfoForConvolution(mConvolutionVS, mConvolutionPS, mInputLayout, mLinearSamplerState);
					for (int volumeIndex = 0; volumeIndex < NUM_PROBE_VOLUME_CASCADES; volumeIndex++)
					{
						if (probesY % mSpecularProbeIndexSkip[volumeIndex] == 0 &&
							probesX % mSpecularProbeIndexSkip[volumeIndex] == 0 &&
							probesZ % mSpecularProbeIndexSkip[volumeIndex] == 0)
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
				std::pair<std::string, Rendering::RenderingObject*>(name, new Rendering::RenderingObject(name, scene->objects.size(), game, camera,
					std::unique_ptr<Model>(new Model(game, Utility::GetFilePath("content\\models\\sphere_lowpoly.fbx"), true)), false, true))
			);

			if (!result.second)
				throw GameException("Could not add a specular probe global object into scene");

			Effect* specularLightProbeEffect = new Effect(game); // deleted when material is deleted
			specularLightProbeEffect->CompileFromFile(Utility::GetFilePath(Utility::ToWideString("content\\effects\\DebugLightProbe.fx")));

			mSpecularProbeRenderingObject[volumeIndex] = result.first->second;
			mSpecularProbeRenderingObject[volumeIndex]->LoadMaterial(new Rendering::DebugLightProbeMaterial(), specularLightProbeEffect, MaterialHelper::debugLightProbeMaterialName);
			mSpecularProbeRenderingObject[volumeIndex]->LoadRenderBuffers();
			mSpecularProbeRenderingObject[volumeIndex]->LoadInstanceBuffers();
			mSpecularProbeRenderingObject[volumeIndex]->ResetInstanceData(mSpecularProbesCountTotal, true);
			for (int i = 0; i < mSpecularProbesCountTotal; i++) {
				XMMATRIX worldT = XMMatrixTranslation(mSpecularProbes[i]->GetPosition().x, mSpecularProbes[i]->GetPosition().y, mSpecularProbes[i]->GetPosition().z);
				mSpecularProbeRenderingObject[volumeIndex]->AddInstanceData(worldT);
			}
			mSpecularProbeRenderingObject[volumeIndex]->UpdateInstanceBuffer(mSpecularProbeRenderingObject[volumeIndex]->GetInstancesData());
			mSpecularProbeRenderingObject[volumeIndex]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::debugLightProbeMaterialName,
				[&, _volumeIndex = volumeIndex](int meshIndex) { UpdateDebugLightProbeMaterialVariables(mSpecularProbeRenderingObject[_volumeIndex], meshIndex, SPECULAR_PROBE, _volumeIndex); });
		}

		mTempSpecularCubemapFacesRT = new ER_GPUTexture(game.Direct3DDevice(), SPECULAR_PROBE_SIZE, SPECULAR_PROBE_SIZE, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, SPECULAR_PROBE_MIP_COUNT, -1, CUBEMAP_FACES_COUNT, true);
		mTempSpecularCubemapFacesConvolutedRT = new ER_GPUTexture(game.Direct3DDevice(), SPECULAR_PROBE_SIZE, SPECULAR_PROBE_SIZE, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, SPECULAR_PROBE_MIP_COUNT, -1, CUBEMAP_FACES_COUNT, true);

		for (int i = 0; i < NUM_PROBE_VOLUME_CASCADES; i++)
		{
			mSpecularCubemapArrayRT[i] = new ER_GPUTexture(game.Direct3DDevice(), SPECULAR_PROBE_SIZE, SPECULAR_PROBE_SIZE, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
				D3D11_BIND_SHADER_RESOURCE, SPECULAR_PROBE_MIP_COUNT, -1, CUBEMAP_FACES_COUNT, true, mMaxNonCulledSpecularProbesCount);
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
			int xIndex = (pos.x - mSceneProbesMinBounds.x) / (DISTANCE_BETWEEN_DIFFUSE_PROBES * mDiffuseProbeIndexSkip[volumeIndex]);
			int yIndex = (pos.y - mSceneProbesMinBounds.y) / (DISTANCE_BETWEEN_DIFFUSE_PROBES * mDiffuseProbeIndexSkip[volumeIndex]);
			int zIndex = (pos.z - mSceneProbesMinBounds.z) / (DISTANCE_BETWEEN_DIFFUSE_PROBES * mDiffuseProbeIndexSkip[volumeIndex]);

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
			int xIndex = (pos.x - mSceneProbesMinBounds.x) / (DISTANCE_BETWEEN_SPECULAR_PROBES * mSpecularProbeIndexSkip[volumeIndex]);
			int yIndex = (pos.y - mSceneProbesMinBounds.y) / (DISTANCE_BETWEEN_SPECULAR_PROBES * mSpecularProbeIndexSkip[volumeIndex]);
			int zIndex = (pos.z - mSceneProbesMinBounds.z) / (DISTANCE_BETWEEN_SPECULAR_PROBES * mSpecularProbeIndexSkip[volumeIndex]);

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

	float ER_IlluminationProbeManager::GetProbesIndexSkip(ER_ProbeType aType, int volumeIndex)
	{
		if (aType == DIFFUSE_PROBE)
			return static_cast<float>(mDiffuseProbeIndexSkip[volumeIndex]);
		else
			return static_cast<float>(mSpecularProbeIndexSkip[volumeIndex]);
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
		if (aType == DIFFUSE_PROBE) {
			if (mDiffuseProbeRenderingObject[volumeIndex] && mDiffuseProbesReady)
				mDiffuseProbeRenderingObject[volumeIndex]->Draw(MaterialHelper::debugLightProbeMaterialName);
		}
		else
		{
			if (mSpecularProbeRenderingObject[volumeIndex] && mSpecularProbesReady)
				mSpecularProbeRenderingObject[volumeIndex]->Draw(MaterialHelper::debugLightProbeMaterialName);
		}
	}

	void ER_IlluminationProbeManager::DrawDebugProbesVolumeGizmo(int volumeIndex)
	{
		if (mDebugProbeVolumeGizmo[volumeIndex])
			mDebugProbeVolumeGizmo[volumeIndex]->Draw();
	}

	void ER_IlluminationProbeManager::UpdateProbesByType(Game& game, ER_ProbeType aType)
	{
		std::vector<ER_LightProbe*>& probes = (aType == DIFFUSE_PROBE) ? mDiffuseProbes : mSpecularProbes;
		
		for (int volumeIndex = 0; volumeIndex < NUM_PROBE_VOLUME_CASCADES; volumeIndex++)
		{
			Rendering::RenderingObject* probeRenderingObject = (aType == DIFFUSE_PROBE) ? mDiffuseProbeRenderingObject[volumeIndex] : mSpecularProbeRenderingObject[volumeIndex];
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
				lightProbe->CPUCullAgainstProbeBoundingVolume(volumeIndex, mCurrentVolumesMinBounds[volumeIndex], mCurrentVolumesMaxBounds[volumeIndex]);

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
			else
			{
				//TODO output to LOG
			}

			auto context = game.Direct3DDeviceContext();
			if (aType == DIFFUSE_PROBE)
			{
				for (int i = 0; i < mDiffuseProbesCountTotal; i++)
					mDiffuseProbesTexArrayIndicesCPUBuffer[volumeIndex][i] = -1;

				for (int i = 0; i < mMaxNonCulledDiffuseProbesCount; i++)
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

				for (int i = 0; i < mMaxNonCulledSpecularProbesCount; i++)
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

		for (int volumeIndex = 0; volumeIndex < NUM_PROBE_VOLUME_CASCADES; volumeIndex++)
		{
			mDebugProbeVolumeGizmo[volumeIndex]->SetPosition(mMainCamera.Position());
			mDebugProbeVolumeGizmo[volumeIndex]->Update();

			mCurrentVolumesMaxBounds[volumeIndex] = XMFLOAT3(
				ProbesVolumeCascadeSizes[volumeIndex] + mMainCamera.Position().x,
				ProbesVolumeCascadeSizes[volumeIndex] + mMainCamera.Position().y,
				ProbesVolumeCascadeSizes[volumeIndex] + mMainCamera.Position().z);

			mCurrentVolumesMinBounds[volumeIndex] = XMFLOAT3(
				-ProbesVolumeCascadeSizes[volumeIndex] + mMainCamera.Position().x,
				-ProbesVolumeCascadeSizes[volumeIndex] + mMainCamera.Position().y,
				-ProbesVolumeCascadeSizes[volumeIndex] + mMainCamera.Position().z);
		}
		UpdateProbesByType(game, DIFFUSE_PROBE);
		UpdateProbesByType(game, SPECULAR_PROBE);
	}

	void ER_IlluminationProbeManager::UpdateDebugLightProbeMaterialVariables(Rendering::RenderingObject* obj, int meshIndex, ER_ProbeType aType, int volumeIndex)
	{
		if (!obj)
			return;

		XMMATRIX vp = mMainCamera.ViewMatrix() * mMainCamera.ProjectionMatrix();
		auto material = static_cast<Rendering::DebugLightProbeMaterial*>(obj->GetMaterials()[MaterialHelper::debugLightProbeMaterialName]);
		if (material)
		{
			material->ViewProjection() << vp;
			material->World() << XMMatrixIdentity();
			material->CameraPosition() << mMainCamera.PositionVector();
			material->DiscardCulledProbe() << mDebugDiscardCulledProbes;
			if (aType == DIFFUSE_PROBE)
				material->CubemapTexture() << mDiffuseCubemapArrayRT[volumeIndex]->GetSRV();
			else
				material->CubemapTexture() << mSpecularCubemapArrayRT[volumeIndex]->GetSRV();
		}
	}
}

