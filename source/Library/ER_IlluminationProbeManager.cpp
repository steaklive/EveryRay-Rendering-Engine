#include "ER_IlluminationProbeManager.h"
#include "Game.h"
#include "GameTime.h"
#include "GameException.h"
#include "Utility.h"
#include "ShadowMapper.h"
#include "DirectionalLight.h"
#include "MatrixHelper.h"
#include "MaterialHelper.h"
#include "VectorHelper.h"
#include "StandardLightingMaterial.h"
#include "Skybox.h"
#include "QuadRenderer.h"
#include "ShaderCompiler.h"
#include "RenderingObject.h"
#include "Model.h"
#include "Scene.h"
#include "Material.h"
#include "Effect.h"
#include "DebugLightProbeMaterial.h"
#include "RenderableAABB.h"

namespace Library
{
	//X+, X-, Y+, Y-, Z+, Z-
	static const XMFLOAT3 cubemapFacesDirections[CUBEMAP_FACES_COUNT] = {
		Vector3Helper::Right,
		Vector3Helper::Left,
		Vector3Helper::Up,
		Vector3Helper::Down,
		Vector3Helper::Backward,
		Vector3Helper::Forward
	};
	static const XMFLOAT3 cubemapUpDirections[CUBEMAP_FACES_COUNT] = {
		Vector3Helper::Up,
		Vector3Helper::Up,
		Vector3Helper::Forward,
		Vector3Helper::Backward,
		Vector3Helper::Up,
		Vector3Helper::Up
	};

	ER_IlluminationProbeManager::ER_IlluminationProbeManager(Game& game, Camera& camera, Scene* scene, DirectionalLight& light, ShadowMapper& shadowMapper)
		: mMainCamera(camera)
	{
		//TODO temp

		if (!scene)
			throw GameException("No scene to load light probes for!");

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

		XMFLOAT3 minBounds = scene->GetLightProbesVolumeMinBounds();
		XMFLOAT3 maxBounds = scene->GetLightProbesVolumeMaxBounds();

		mDebugProbeVolumeGizmo = new RenderableAABB(game, mMainCamera, XMFLOAT4(0.44f, 0.2f, 0.64f, 1.0f));
		mDebugProbeVolumeGizmo->Initialize();
		mDebugProbeVolumeGizmo->InitializeGeometry({
			XMFLOAT3(-MAIN_CAMERA_PROBE_VOLUME_SIZE, -MAIN_CAMERA_PROBE_VOLUME_SIZE, -MAIN_CAMERA_PROBE_VOLUME_SIZE),
			XMFLOAT3(MAIN_CAMERA_PROBE_VOLUME_SIZE, MAIN_CAMERA_PROBE_VOLUME_SIZE, MAIN_CAMERA_PROBE_VOLUME_SIZE) }, XMMatrixScaling(1, 1, 1));
		mDebugProbeVolumeGizmo->SetPosition(mMainCamera.Position());

		// diffuse probes setup
		{
			mDiffuseProbesCountX = (maxBounds.x - minBounds.x) / DISTANCE_BETWEEN_DIFFUSE_PROBES;
			mDiffuseProbesCountY = (maxBounds.y - minBounds.y) / DISTANCE_BETWEEN_DIFFUSE_PROBES;
			mDiffuseProbesCountZ = (maxBounds.z - minBounds.z) / DISTANCE_BETWEEN_DIFFUSE_PROBES;
			mDiffuseProbesCountTotal = mDiffuseProbesCountX * mDiffuseProbesCountY * mDiffuseProbesCountZ;
			mDiffuseProbes.reserve(mDiffuseProbesCountTotal);
			
			for (size_t i = 0; i < mDiffuseProbesCountTotal; i++)
				mDiffuseProbes.emplace_back(new ER_LightProbe(game, light, shadowMapper, DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE));

			//simple 3D grid distribution
			{
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
						}
					}
				}
			}

			auto result = scene->objects.insert(
				std::pair<std::string, Rendering::RenderingObject*>(
					"Debug diffuse lightprobes",
					new Rendering::RenderingObject("Debug diffuse lightprobes", scene->objects.size(), game, camera,
						std::unique_ptr<Model>(new Model(game, Utility::GetFilePath("content\\models\\sphere_lowpoly.fbx"), true)), true, true)
					)
			);

			Effect* diffuseLightProbeEffect = new Effect(game); // deleted when material is deleted
			diffuseLightProbeEffect->CompileFromFile(Utility::GetFilePath(Utility::ToWideString("content\\effects\\DebugLightProbe.fx")));

			mDiffuseProbeRenderingObject = result.first->second;
			mDiffuseProbeRenderingObject->LoadMaterial(new Rendering::DebugLightProbeMaterial(), diffuseLightProbeEffect, MaterialHelper::debugLightProbeMaterialName);
			mDiffuseProbeRenderingObject->LoadRenderBuffers();
			mDiffuseProbeRenderingObject->LoadInstanceBuffers();
			mDiffuseProbeRenderingObject->ResetInstanceData(mDiffuseProbesCountTotal, true);
			for (int i = 0; i < mDiffuseProbesCountTotal; i++) {
				XMMATRIX worldT = XMMatrixTranslation(mDiffuseProbes[i]->GetPosition().x, mDiffuseProbes[i]->GetPosition().y, mDiffuseProbes[i]->GetPosition().z);
				mDiffuseProbeRenderingObject->AddInstanceData(worldT);
			}
			mDiffuseProbeRenderingObject->UpdateInstanceBuffer(mDiffuseProbeRenderingObject->GetInstancesData());
			mDiffuseProbeRenderingObject->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::debugLightProbeMaterialName,
				[&](int meshIndex) { UpdateDebugLightProbeMaterialVariables(mDiffuseProbeRenderingObject, meshIndex, DIFFUSE_PROBE); });

			mDiffuseCubemapFacesRT = new CustomRenderTarget(game.Direct3DDevice(), DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE_SIZE, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
				D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 1, -1, CUBEMAP_FACES_COUNT, true);
			mDiffuseCubemapFacesConvolutedRT = new CustomRenderTarget(game.Direct3DDevice(), DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE_SIZE, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
				D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 1, -1, CUBEMAP_FACES_COUNT, true);
			mDiffuseCubemapArrayRT = new CustomRenderTarget(game.Direct3DDevice(), DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE_SIZE, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
				D3D11_BIND_SHADER_RESOURCE, 1, -1, CUBEMAP_FACES_COUNT, true, MaxNonCulledDiffuseProbesCount);
		}
		
		// specular probes setup
		{
			mSpecularProbesCountX = (maxBounds.x - minBounds.x) / DISTANCE_BETWEEN_SPECULAR_PROBES;
			mSpecularProbesCountY = (maxBounds.y - minBounds.y) / DISTANCE_BETWEEN_SPECULAR_PROBES;
			mSpecularProbesCountZ = (maxBounds.z - minBounds.z) / DISTANCE_BETWEEN_SPECULAR_PROBES;
			mSpecularProbesCountTotal = mSpecularProbesCountX * mSpecularProbesCountY * mSpecularProbesCountZ;
			mSpecularProbes.reserve(mSpecularProbesCountTotal);
			for (size_t i = 0; i < mSpecularProbesCountTotal; i++)
				mSpecularProbes.emplace_back(new ER_LightProbe(game, light, shadowMapper, SPECULAR_PROBE_SIZE, SPECULAR_PROBE));

			//simple 3D grid distribution
			{
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
						}
					}
				}
			}

			auto result = scene->objects.insert(
				std::pair<std::string, Rendering::RenderingObject*>(
					"Debug specular lightprobes",
					new Rendering::RenderingObject("Debug specular lightprobes", scene->objects.size(), game, camera,
						std::unique_ptr<Model>(new Model(game, Utility::GetFilePath("content\\models\\sphere_lowpoly.fbx"), true)), true, true)
					)
			);

			Effect* specularLightProbeEffect = new Effect(game); // deleted when material is deleted
			specularLightProbeEffect->CompileFromFile(Utility::GetFilePath(Utility::ToWideString("content\\effects\\DebugLightProbe.fx")));

			mSpecularProbeRenderingObject = result.first->second;
			mSpecularProbeRenderingObject->LoadMaterial(new Rendering::DebugLightProbeMaterial(), specularLightProbeEffect, MaterialHelper::debugLightProbeMaterialName);
			mSpecularProbeRenderingObject->LoadRenderBuffers();
			mSpecularProbeRenderingObject->LoadInstanceBuffers();
			mSpecularProbeRenderingObject->ResetInstanceData(mSpecularProbesCountTotal, true);
			for (int i = 0; i < mSpecularProbesCountTotal; i++) {
				XMMATRIX worldT = XMMatrixTranslation(mSpecularProbes[i]->GetPosition().x, mSpecularProbes[i]->GetPosition().y, mSpecularProbes[i]->GetPosition().z);
				mSpecularProbeRenderingObject->AddInstanceData(worldT);
			}
			mSpecularProbeRenderingObject->UpdateInstanceBuffer(mSpecularProbeRenderingObject->GetInstancesData());
			mSpecularProbeRenderingObject->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::debugLightProbeMaterialName,
				[&](int meshIndex) { UpdateDebugLightProbeMaterialVariables(mSpecularProbeRenderingObject, meshIndex, SPECULAR_PROBE); });

			mSpecularCubemapFacesRT = new CustomRenderTarget(game.Direct3DDevice(), SPECULAR_PROBE_SIZE, SPECULAR_PROBE_SIZE, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
				D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, SPECULAR_PROBE_MIP_COUNT, -1, CUBEMAP_FACES_COUNT, true);
			mSpecularCubemapFacesConvolutedRT = new CustomRenderTarget(game.Direct3DDevice(), SPECULAR_PROBE_SIZE, SPECULAR_PROBE_SIZE, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
				D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, SPECULAR_PROBE_MIP_COUNT, -1, CUBEMAP_FACES_COUNT, true);
			mSpecularCubemapArrayRT = new CustomRenderTarget(game.Direct3DDevice(), SPECULAR_PROBE_SIZE, SPECULAR_PROBE_SIZE, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
				D3D11_BIND_SHADER_RESOURCE, SPECULAR_PROBE_MIP_COUNT, -1, CUBEMAP_FACES_COUNT, true, MaxNonCulledSpecularProbesCount);
		}

		// TODO Load a pre-computed Integration Map
		if (FAILED(DirectX::CreateDDSTextureFromFile(game.Direct3DDevice(), game.Direct3DDeviceContext(),
			Utility::GetFilePath(L"content\\textures\\skyboxes\\textureBrdf.dds").c_str(), nullptr, &mIntegrationMapTextureSRV)))
			throw GameException("Failed to create Integration Texture.");


		for (int i = 0; i < CUBEMAP_FACES_COUNT; i++)
		{
			mDiffuseCubemapDepthBuffers[i] = DepthTarget::Create(game.Direct3DDevice(), DIFFUSE_PROBE_SIZE, DIFFUSE_PROBE_SIZE, 1u, DXGI_FORMAT_D24_UNORM_S8_UINT);
			mSpecularCubemapDepthBuffers[i] = DepthTarget::Create(game.Direct3DDevice(), SPECULAR_PROBE_SIZE, SPECULAR_PROBE_SIZE, 1u, DXGI_FORMAT_D24_UNORM_S8_UINT);
		}
	}

	ER_IlluminationProbeManager::~ER_IlluminationProbeManager()
	{
		DeletePointerCollection(mDiffuseProbes);
		DeletePointerCollection(mSpecularProbes);
		DeleteObject(mDiffuseCubemapArrayRT);
		DeleteObject(mSpecularCubemapArrayRT);
		DeleteObject(mDebugProbeVolumeGizmo);
		DeleteObject(mQuadRenderer);
		ReleaseObject(mConvolutionVS);
		ReleaseObject(mConvolutionPS);
		ReleaseObject(mInputLayout);
		ReleaseObject(mLinearSamplerState);
		DeleteObject(mDiffuseCubemapFacesRT);
		DeleteObject(mDiffuseCubemapFacesConvolutedRT);
		DeleteObject(mSpecularCubemapFacesRT);
		DeleteObject(mSpecularCubemapFacesConvolutedRT);
		for (int i = 0; i < CUBEMAP_FACES_COUNT; i++)
		{	
			DeleteObject(mDiffuseCubemapDepthBuffers[i]);
			DeleteObject(mSpecularCubemapDepthBuffers[i]);
		}
		ReleaseObject(mIntegrationMapTextureSRV);

	}

	void ER_IlluminationProbeManager::ComputeOrLoadProbes(Game& game, const GameTime& gameTime, ProbesRenderingObjectsInfo& aObjects, Skybox* skybox)
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
					probe->Compute(game, gameTime, mDiffuseCubemapFacesRT, mDiffuseCubemapFacesConvolutedRT, mDiffuseCubemapDepthBuffers, diffuseProbesPath, aObjects, mQuadRenderer, skybox);
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
					probe->Compute(game, gameTime, mSpecularCubemapFacesRT, mSpecularCubemapFacesConvolutedRT, mSpecularCubemapDepthBuffers, specularProbesPath, aObjects, mQuadRenderer, skybox);
			}
			mSpecularProbesReady = true;
		}
	}

	void ER_IlluminationProbeManager::DrawDebugProbes(ER_ProbeType aType)
	{
		if (aType == DIFFUSE_PROBE) {
			if (mDiffuseProbeRenderingObject && mDiffuseProbesReady)
				mDiffuseProbeRenderingObject->Draw(MaterialHelper::debugLightProbeMaterialName);
		}
		else
		{
			if (mSpecularProbeRenderingObject && mSpecularProbesReady)
				mSpecularProbeRenderingObject->Draw(MaterialHelper::debugLightProbeMaterialName);
		}
	}

	void ER_IlluminationProbeManager::DrawDebugProbesVolumeGizmo()
	{
		mDebugProbeVolumeGizmo->Draw();
	}

	void ER_IlluminationProbeManager::UpdateProbesByType(Game& game, ER_ProbeType aType, const XMFLOAT3& minBounds, const XMFLOAT3& maxBounds)
	{
		std::vector<ER_LightProbe*>& probes = (aType == DIFFUSE_PROBE) ? mDiffuseProbes : mSpecularProbes;
		Rendering::RenderingObject* probeRenderingObject = (aType == DIFFUSE_PROBE) ? mDiffuseProbeRenderingObject : mSpecularProbeRenderingObject;

		if (aType == DIFFUSE_PROBE)
		{
			if (!mDiffuseProbesReady)
				return;

			mNonCulledDiffuseProbesCount = 0;
			mNonCulledDiffuseProbesIndices.clear();
		}
		else
		{
			if (!mSpecularProbesReady)
				return;

			mNonCulledSpecularProbesCount = 0;
			mNonCulledSpecularProbesIndices.clear();
		}

		for (auto& lightProbe : probes)
			lightProbe->CPUCullAgainstProbeBoundingVolume(minBounds, maxBounds);

		if (probeRenderingObject)
		{
			auto& oldInstancedData = probeRenderingObject->GetInstancesData();
			assert(oldInstancedData.size() == probes.size());

			for (int i = 0; i < oldInstancedData.size(); i++)
			{
				//writing culling flag to [4][4] of world instanced matrix
				oldInstancedData[i].World._44 = probes[i]->IsCulled() ? 1.0f : 0.0f;
				//writing cubemap index to [0][0] of world instanced matrix (since we dont need scale)
				oldInstancedData[i].World._11 = -1.0f;
				if (!probes[i]->IsCulled())
				{
					if (aType == DIFFUSE_PROBE)
					{
						oldInstancedData[i].World._11 = static_cast<float>(mNonCulledDiffuseProbesCount);
						mNonCulledDiffuseProbesCount++;
						mNonCulledDiffuseProbesIndices.push_back(i);
					}
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
		else
		{
			//TODO output to LOG
		}

		auto context = game.Direct3DDeviceContext();
		if (aType == DIFFUSE_PROBE)
		{
			for (int i = 0; i < MaxNonCulledDiffuseProbesCount; i++)
			{
				if (i < mNonCulledDiffuseProbesIndices.size() && i < probes.size())
				{
					for (int cubeI = 0; cubeI < CUBEMAP_FACES_COUNT; cubeI++)
						context->CopySubresourceRegion(mDiffuseCubemapArrayRT->getTexture2D(), D3D11CalcSubresource(0, cubeI + CUBEMAP_FACES_COUNT * i, 1), 0, 0, 0,
							probes[mNonCulledDiffuseProbesIndices[i]]->GetCubemapTexture2D(), D3D11CalcSubresource(0, cubeI, 1), NULL);
				}
				//TODO clear remaining texture subregions with solid color (PINK)
			}
		}
		else
		{
			for (int i = 0; i < MaxNonCulledSpecularProbesCount; i++)
			{
				if (i < mNonCulledSpecularProbesIndices.size() && i < probes.size())
				{
					for (int cubeI = 0; cubeI < CUBEMAP_FACES_COUNT; cubeI++)
					{
						for (int mip = 0; mip < SPECULAR_PROBE_MIP_COUNT; mip++)
						{
							context->CopySubresourceRegion(mSpecularCubemapArrayRT->getTexture2D(), D3D11CalcSubresource(mip, cubeI + CUBEMAP_FACES_COUNT * i, SPECULAR_PROBE_MIP_COUNT), 0, 0, 0,
								probes[mNonCulledSpecularProbesIndices[i]]->GetCubemapTexture2D(), D3D11CalcSubresource(mip, cubeI, SPECULAR_PROBE_MIP_COUNT), NULL);
						}
					}

				}
				//TODO clear remaining texture subregions with solid color (PINK)
			}
		}
	}

	void ER_IlluminationProbeManager::UpdateProbes(Game& game)
	{
		mDebugProbeVolumeGizmo->SetPosition(mMainCamera.Position());
		mDebugProbeVolumeGizmo->Update();

		XMFLOAT3 maxBounds = XMFLOAT3(
			MAIN_CAMERA_PROBE_VOLUME_SIZE + mMainCamera.Position().x,
			MAIN_CAMERA_PROBE_VOLUME_SIZE + mMainCamera.Position().y,
			MAIN_CAMERA_PROBE_VOLUME_SIZE + mMainCamera.Position().z);

		XMFLOAT3 minBounds = XMFLOAT3(
			-MAIN_CAMERA_PROBE_VOLUME_SIZE + mMainCamera.Position().x,
			-MAIN_CAMERA_PROBE_VOLUME_SIZE + mMainCamera.Position().y,
			-MAIN_CAMERA_PROBE_VOLUME_SIZE + mMainCamera.Position().z);

		UpdateProbesByType(game, DIFFUSE_PROBE, minBounds, maxBounds);
		UpdateProbesByType(game, SPECULAR_PROBE, minBounds, maxBounds);
	}

	void ER_IlluminationProbeManager::UpdateDebugLightProbeMaterialVariables(Rendering::RenderingObject* obj, int meshIndex, ER_ProbeType aType)
	{
		XMMATRIX vp = mMainCamera.ViewMatrix() * mMainCamera.ProjectionMatrix();

		auto material = static_cast<Rendering::DebugLightProbeMaterial*>(obj->GetMaterials()[MaterialHelper::debugLightProbeMaterialName]);
		if (material)
		{
			material->ViewProjection() << vp;
			material->World() << XMMatrixIdentity();
			material->CameraPosition() << mMainCamera.PositionVector();
			if (aType == DIFFUSE_PROBE)
				material->CubemapTexture() << mDiffuseCubemapArrayRT->getSRV();
			else
				material->CubemapTexture() << mSpecularCubemapArrayRT->getSRV();
		}
	}

	ER_LightProbe::ER_LightProbe(Game& game, DirectionalLight& light, ShadowMapper& shadowMapper, int size, ER_ProbeType aType)
		: mSize(size)
		, mDirectionalLight(light)
		, mShadowMapper(shadowMapper)
		, mProbeType(aType)
	{
		mCubemapTexture = new CustomRenderTarget(game.Direct3DDevice(), size, size, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 0,
			(aType == DIFFUSE_PROBE) ? 1 : SPECULAR_PROBE_MIP_COUNT, -1, CUBEMAP_FACES_COUNT, true);
		
		for (int i = 0; i < CUBEMAP_FACES_COUNT; i++)
		{
			mCubemapCameras[i] = new Camera(game, XM_PIDIV2, 1.0f, 0.1f, 100000.0f);
			mCubemapCameras[i]->Initialize();
			mCubemapCameras[i]->SetDirection(cubemapFacesDirections[i]);
			mCubemapCameras[i]->SetUp(cubemapUpDirections[i]);
			mCubemapCameras[i]->UpdateViewMatrix(true); //fix for mirrored result due to right-hand coordinate system
			mCubemapCameras[i]->UpdateProjectionMatrix(true); //fix for mirrored result due to right-hand coordinate system
		}

		mConvolutionCB.Initialize(game.Direct3DDevice());
	}

	ER_LightProbe::~ER_LightProbe()
	{
		ReleaseObject(mLinearSamplerState);
		DeleteObject(mCubemapTexture);
		for (int i = 0; i < CUBEMAP_FACES_COUNT; i++)
		{
			DeleteObject(mCubemapCameras[i]);
		}
		mConvolutionCB.Release();
	}

	void ER_LightProbe::SetPosition(const XMFLOAT3& pos)
	{
		mPosition = pos;
		for (int i = 0; i < CUBEMAP_FACES_COUNT; i++)
		{
			mCubemapCameras[i]->SetPosition(pos);
			mCubemapCameras[i]->UpdateViewMatrix(true); //fix for mirrored result due to right-hand coordinate system
			mCubemapCameras[i]->UpdateProjectionMatrix(true); //fix for mirrored result due to right-hand coordinate system
		}
	}

	void ER_LightProbe::CPUCullAgainstProbeBoundingVolume(const XMFLOAT3& aMin, const XMFLOAT3& aMax)
	{
		mIsCulled = 
			(mPosition.x > aMax.x || mPosition.x < aMin.x) ||
			(mPosition.y > aMax.y || mPosition.y < aMin.y) ||
			(mPosition.z > aMax.z || mPosition.z < aMin.z);
	}

	void ER_LightProbe::Compute(Game& game, const GameTime& gameTime, CustomRenderTarget* aTextureNonConvoluted, CustomRenderTarget* aTextureConvoluted,
		DepthTarget** aDepthBuffers, const std::wstring& levelPath, const LightProbeRenderingObjectsInfo& objectsToRender, QuadRenderer* quadRenderer, Skybox* skybox)
	{
		if (mIsProbeLoadedFromDisk)
			return;

		std::string materialListenerName = ((mProbeType == DIFFUSE_PROBE) ? "diffuse_" : "specular_") + MaterialHelper::forwardLightingForProbesMaterialName;

		for (int cubemapFaceIndex = 0; cubemapFaceIndex < CUBEMAP_FACES_COUNT; cubemapFaceIndex++)
			for (auto& object : objectsToRender)
				object.second->MeshMaterialVariablesUpdateEvent->AddListener(materialListenerName + "_" + std::to_string(cubemapFaceIndex),
					[&, cubemapFaceIndex](int meshIndex) { UpdateStandardLightingPBRProbeMaterialVariables(object.second, meshIndex, cubemapFaceIndex); });

		auto context = game.Direct3DDeviceContext();
		
		UINT viewportsCount = 1;
		CD3D11_VIEWPORT oldViewPort;
		context->RSGetViewports(&viewportsCount, &oldViewPort);
		CD3D11_VIEWPORT newViewPort(0.0f, 0.0f, static_cast<float>(mSize), static_cast<float>(mSize));
		context->RSSetViewports(1, &newViewPort);
		
		//sadly, we can't combine or multi-thread these two functions, because of the artifacts on edges of the convoluted faces of the cubemap...
		DrawGeometryToProbe(game, gameTime, aTextureNonConvoluted, aDepthBuffers, objectsToRender, skybox);
		ConvoluteProbe(game, quadRenderer, aTextureNonConvoluted, aTextureConvoluted);

		context->RSSetViewports(1, &oldViewPort);

		for (int cubeMapFaceIndex = 0; cubeMapFaceIndex < CUBEMAP_FACES_COUNT; cubeMapFaceIndex++)
			for (auto& object : objectsToRender)
				object.second->MeshMaterialVariablesUpdateEvent->RemoveListener(materialListenerName + "_" + std::to_string(cubeMapFaceIndex));

		SaveProbeOnDisk(game, levelPath, aTextureConvoluted);
		mIsProbeLoadedFromDisk = true;
	}

	void ER_LightProbe::DrawGeometryToProbe(Game& game, const GameTime& gameTime, CustomRenderTarget* aTextureNonConvoluted, DepthTarget** aDepthBuffers,
		const LightProbeRenderingObjectsInfo& objectsToRender, Skybox* skybox)
	{
		auto context = game.Direct3DDeviceContext();
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		std::string materialListenerName = ((mProbeType == DIFFUSE_PROBE) ? "diffuse_" : "specular_") + MaterialHelper::forwardLightingForProbesMaterialName;

		//draw world to probe
		for (int cubeMapFace = 0; cubeMapFace < CUBEMAP_FACES_COUNT; cubeMapFace++)
		{
			// Set the render target and clear it.
			int rtvShift = (mProbeType == DIFFUSE_PROBE) ? 1 : SPECULAR_PROBE_MIP_COUNT;
			context->OMSetRenderTargets(1, &aTextureNonConvoluted->getRTVs()[cubeMapFace * rtvShift], aDepthBuffers[cubeMapFace]->getDSV());
			context->ClearRenderTargetView(aTextureNonConvoluted->getRTVs()[cubeMapFace], clearColor);
			context->ClearDepthStencilView(aDepthBuffers[cubeMapFace]->getDSV(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

			//rendering objects and sky
			{
				if (skybox)
				{
					skybox->Update(gameTime, mCubemapCameras[cubeMapFace]);
					skybox->Draw(mCubemapCameras[cubeMapFace]);
					//TODO draw sun
					//...
					//skybox->UpdateSun(gameTime, mCubemapCameras[cubeMapFace]);
				}

				// We don't do lodding because it is bound to main camera... We force pick lod 0.
				// This is incorrect and might cause issues like: 
				// Probe P is next to object A, but object A is far from main camera => A does not have lod 0, probe P can not render A.
				const int lod = 0;

				//TODO change to culled objects per face (not a priority since we compute probes once)
				for (auto& object : objectsToRender)
				{
					if (object.second->IsInLightProbe())
						object.second->DrawLOD(materialListenerName + "_" + std::to_string(cubeMapFace), false, -1, lod);
				}
			}
		}

		if (mProbeType == SPECULAR_PROBE)
			context->GenerateMips(aTextureNonConvoluted->getSRV());
	}

	void ER_LightProbe::ConvoluteProbe(Game& game, QuadRenderer* quadRenderer, CustomRenderTarget* aTextureNonConvoluted, CustomRenderTarget* aTextureConvoluted)
	{
		int mipCount = -1;
		if (mProbeType == SPECULAR_PROBE)
			mipCount = SPECULAR_PROBE_MIP_COUNT;

		auto context = game.Direct3DDeviceContext();
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		int totalMips = (mipCount == -1) ? 1 : mipCount;
		int rtvShift = (mProbeType == DIFFUSE_PROBE) ? 1 : SPECULAR_PROBE_MIP_COUNT;

		ID3D11ShaderResourceView* SRVs[1] = { aTextureNonConvoluted->getSRV() };
		ID3D11SamplerState* SSs[1] = { mLinearSamplerState };

		for (int cubeMapFace = 0; cubeMapFace < CUBEMAP_FACES_COUNT; cubeMapFace++)
		{
			int currentSize = mSize;

			for (int mip = 0; mip < totalMips; mip++)
			{
				mConvolutionCB.Data.FaceIndex = cubeMapFace;
				mConvolutionCB.Data.MipIndex = (mProbeType == DIFFUSE_PROBE) ? -1 : mip;
				mConvolutionCB.ApplyChanges(context);

				CD3D11_VIEWPORT newViewPort(0.0f, 0.0f, static_cast<float>(currentSize), static_cast<float>(currentSize));
				context->RSSetViewports(1, &newViewPort);

				context->OMSetRenderTargets(1, &aTextureConvoluted->getRTVs()[cubeMapFace * rtvShift + mip], NULL);
				context->ClearRenderTargetView(aTextureConvoluted->getRTVs()[cubeMapFace * rtvShift + mip], clearColor);

				ID3D11Buffer* CBs[1] = { mConvolutionCB.Buffer() };

				context->IASetInputLayout(mInputLayout);
				context->VSSetShader(mConvolutionVS, NULL, NULL);
				context->VSSetConstantBuffers(0, 1, CBs);
				
				context->PSSetShader(mConvolutionPS, NULL, NULL);
				context->PSSetSamplers(0, 1, SSs);
				context->PSSetShaderResources(0, 1, SRVs);
				context->PSSetConstantBuffers(0, 1, CBs);

				if(quadRenderer)
					quadRenderer->Draw(context);

				currentSize >>= 1;
			}
		}
	}

	bool ER_LightProbe::LoadProbeFromDisk(Game& game, const std::wstring& levelPath)
	{
		std::wstring probeName = GetConstructedProbeName(levelPath);

		ID3D11Texture2D* tex2d = NULL;
		ID3D11ShaderResourceView* srv = NULL;
		ID3D11Resource* resourceTex = NULL;

		if (FAILED(DirectX::CreateDDSTextureFromFile(game.Direct3DDevice(), probeName.c_str(), &resourceTex, &srv)))
		{
			//std::wstring status = L"Failed to load DDS probe texture: " + probeName;
			//TODO output to LOG (not exception)
			mIsProbeLoadedFromDisk = false;
		}
		else
		{
			
			if (FAILED(resourceTex->QueryInterface(IID_ID3D11Texture2D, (void**)&tex2d))) {
				mIsProbeLoadedFromDisk = false;
				return false;
			}

			// Copying loaded resource into the convoluted resource of the probe
			//int mips = (mProbeType == DIFFUSE_PROBE) ? 1 : SPECULAR_PROBE_MIP_COUNT;
			//const int srcDiffuseAutogeneratedMipCount = 6;
			//for (int i = 0; i < CUBEMAP_FACES_COUNT; i++)
			//{
			//	for (int mip = 0; mip < mips; mip++)
			//	{
			//		game.Direct3DDeviceContext()->CopySubresourceRegion(mCubemapTexture->getTexture2D(),
			//			D3D11CalcSubresource(mip, i, mips), 0, 0, 0, resourceTex,
			//			D3D11CalcSubresource(mip, i, (mProbeType == DIFFUSE_PROBE) ? srcDiffuseAutogeneratedMipCount : mips), NULL);
			//	}
			//}

			mCubemapTexture->SetSRV(srv);
			mCubemapTexture->SetTexture2D(tex2d);

			resourceTex->Release();

			mIsProbeLoadedFromDisk = true;
		}
		return mIsProbeLoadedFromDisk;
	}

	void ER_LightProbe::SaveProbeOnDisk(Game& game, const std::wstring& levelPath, CustomRenderTarget* aTextureConvoluted)
	{
		DirectX::ScratchImage tempImage;
		HRESULT res = DirectX::CaptureTexture(game.Direct3DDevice(), game.Direct3DDeviceContext(), aTextureConvoluted->getTexture2D(), tempImage);
		if (FAILED(res))
			throw GameException("Failed to capture a probe texture when saving!", res);

		std::wstring fileName = GetConstructedProbeName(levelPath);

		res = DirectX::SaveToDDSFile(tempImage.GetImages(), tempImage.GetImageCount(), tempImage.GetMetadata(), DDS_FLAGS_NONE, fileName.c_str());
		if (FAILED(res))
		{
			std::string str(fileName.begin(), fileName.end());
			std::string msg = "Failed to save a probe texture: " + str;
			throw GameException(msg.c_str());
		}
	}

	std::wstring ER_LightProbe::GetConstructedProbeName(const std::wstring& levelPath)
	{
		std::wstring fileName = levelPath;
		if (mProbeType == DIFFUSE_PROBE)
			fileName += L"diffuse_probe";
		else if (mProbeType == SPECULAR_PROBE)
			fileName += L"specular_probe";

		fileName += L"_"
			+ std::to_wstring(static_cast<int>(mPosition.x)) + L"_"
			+ std::to_wstring(static_cast<int>(mPosition.y)) + L"_"
			+ std::to_wstring(static_cast<int>(mPosition.z)) + L".dds";

		return fileName;
	}

	void ER_LightProbe::UpdateStandardLightingPBRProbeMaterialVariables(Rendering::RenderingObject* obj, int meshIndex, int cubeFaceIndex)
	{
		assert(mCubemapCameras);

		XMMATRIX worldMatrix = XMLoadFloat4x4(&(obj->GetTransformationMatrix4X4()));
		XMMATRIX vp = mCubemapCameras[cubeFaceIndex]->ViewMatrix() * mCubemapCameras[cubeFaceIndex]->ProjectionMatrix();

		XMMATRIX shadowMatrices[3] =
		{
			mShadowMapper.GetViewMatrix(0) * mShadowMapper.GetProjectionMatrix(0) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix()) ,
			mShadowMapper.GetViewMatrix(1) * mShadowMapper.GetProjectionMatrix(1) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix()) ,
			mShadowMapper.GetViewMatrix(2) * mShadowMapper.GetProjectionMatrix(2) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix())
		};

		ID3D11ShaderResourceView* shadowMaps[3] =
		{
			mShadowMapper.GetShadowTexture(0),
			mShadowMapper.GetShadowTexture(1),
			mShadowMapper.GetShadowTexture(2)
		};

		std::string materialName = ((mProbeType == DIFFUSE_PROBE) ? "diffuse_" : "specular_") + MaterialHelper::forwardLightingForProbesMaterialName + "_" + std::to_string(cubeFaceIndex);
		auto material = static_cast<Rendering::StandardLightingMaterial*>(obj->GetMaterials()[materialName]);
		
		if (material)
		{
			if (mProbeType == DIFFUSE_PROBE) 
			{
				if (obj->IsInstanced())
					material->SetCurrentTechnique(material->GetEffect()->TechniquesByName().at("standard_lighting_pbr_diffuse_probes_instancing"));
				else
					material->SetCurrentTechnique(material->GetEffect()->TechniquesByName().at("standard_lighting_pbr_diffuse_probes"));
			}
			else
			{
				if (obj->IsInstanced())
					material->SetCurrentTechnique(material->GetEffect()->TechniquesByName().at("standard_lighting_pbr_specular_probes_instancing"));
				else
					material->SetCurrentTechnique(material->GetEffect()->TechniquesByName().at("standard_lighting_pbr_specular_probes"));
			}

			material->ViewProjection() << vp;
			material->World() << worldMatrix;
			material->ShadowMatrices().SetMatrixArray(shadowMatrices, 0, NUM_SHADOW_CASCADES);
			material->CameraPosition() << mCubemapCameras[cubeFaceIndex]->PositionVector();
			material->SunDirection() << XMVectorNegate(mDirectionalLight.DirectionVector());
			material->SunColor() << XMVECTOR{ mDirectionalLight.GetDirectionalLightColor().x, mDirectionalLight.GetDirectionalLightColor().y, mDirectionalLight.GetDirectionalLightColor().z, 5.0f };
			material->AmbientColor() << XMVECTOR{ mDirectionalLight.GetAmbientLightColor().x,  mDirectionalLight.GetAmbientLightColor().y, mDirectionalLight.GetAmbientLightColor().z , 1.0f };
			material->ShadowTexelSize() << XMVECTOR{ 1.0f / mShadowMapper.GetResolution(), 1.0f, 1.0f , 1.0f };
			material->ShadowCascadeDistances() << XMVECTOR{ 
				mCubemapCameras[cubeFaceIndex]->GetCameraFarCascadeDistance(0),
				mCubemapCameras[cubeFaceIndex]->GetCameraFarCascadeDistance(1),
				mCubemapCameras[cubeFaceIndex]->GetCameraFarCascadeDistance(2), 1.0f };
			material->AlbedoTexture() << obj->GetTextureData(meshIndex).AlbedoMap;
			material->NormalTexture() << obj->GetTextureData(meshIndex).NormalMap;
			material->SpecularTexture() << obj->GetTextureData(meshIndex).SpecularMap;
			material->RoughnessTexture() << obj->GetTextureData(meshIndex).RoughnessMap;
			material->MetallicTexture() << obj->GetTextureData(meshIndex).MetallicMap;
			material->CascadedShadowTextures().SetResourceArray(shadowMaps, 0, NUM_SHADOW_CASCADES);
		}
	}
}

