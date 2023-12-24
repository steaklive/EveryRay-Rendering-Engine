#include "ER_Sandbox.h"
#include "ER_Utility.h"
#include "ER_Settings.h"
#include "ER_MaterialsCallbacks.h"
#include "ER_Core.h"
#include "ER_CoreTime.h"
#include "ER_CoreException.h"
#include "ER_Editor.h"
#include "ER_QuadRenderer.h"
#include "ER_Camera.h"
#include "ER_DirectionalLight.h"
#include "ER_PointLight.h"
#include "ER_Keyboard.h"
#include "ER_Skybox.h"
#include "ER_PostProcessingStack.h"
#include "ER_GBuffer.h"
#include "ER_ShadowMapper.h"
#include "ER_Terrain.h"
#include "ER_FoliageManager.h"
#include "ER_Scene.h"
#include "ER_VolumetricClouds.h"
#include "ER_VolumetricFog.h"
#include "ER_Wind.h"
#include "ER_Illumination.h"
#include "ER_LightProbesManager.h"
#include "ER_GPUCuller.h"

#include "RHI/ER_RHI.h"

namespace EveryRay_Core {

	ER_Sandbox::ER_Sandbox()
	{
	}
	ER_Sandbox::~ER_Sandbox()
	{
	}
	
	void ER_Sandbox::Destroy(ER_Core& game)
	{
		game.CPUProfiler()->BeginCPUTime("Destroying scene: " + mName);

		ER_RHI* rhi = game.GetRHI();
		rhi->WaitForGpuOnGraphicsFence();

		DeleteObject(mDirectionalLight);
		DeleteObject(mSkybox);
		DeleteObject(mPostProcessingStack);
		DeleteObject(mGBuffer);
		DeleteObject(mShadowMapper);
		DeleteObject(mFoliageSystem);
		DeleteObject(mVolumetricClouds);
		DeleteObject(mVolumetricFog);
		DeleteObject(mIllumination);
		DeleteObject(mScene);
		DeleteObject(mLightProbesManager);
		DeleteObject(mTerrain);
		DeleteObject(mWind);
		DeleteObject(mGPUCuller);
		DeletePointerCollection(mPointLights);

		game.CPUProfiler()->EndCPUTime("Destroying scene: " + mName);
	}

    void ER_Sandbox::Initialize(ER_Core& game, ER_Camera& camera, const std::string& sceneName, const std::string& sceneFolderPath)
    {
		mName = sceneName;

		ER_RHI* rhi = game.GetRHI();
		assert(rhi);

		rhi->BeginGraphicsCommandList(rhi->GetPrepareGraphicsCommandListIndex()); // for texture loading etc. (everything before the first frame starts)
		rhi->SetGPUDescriptorHeap(ER_RHI_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

		#pragma region INIT_CONTROLS
        mKeyboard = (ER_Keyboard*)game.GetServices().FindService(ER_Keyboard::TypeIdClass());
        assert(mKeyboard);
#pragma endregion

		#pragma region INIT_SCENE
		game.CPUProfiler()->BeginCPUTime("Scene init: " + sceneName);
        mScene = new ER_Scene(game, camera, sceneFolderPath + sceneName + ".json");
		mScene->LoadPointLightsData();
		game.CPUProfiler()->EndCPUTime("Scene init: " + sceneName);
#pragma endregion

		#pragma region INIT_EDITOR
		mEditor = (ER_Editor*)game.GetServices().FindService(ER_Editor::TypeIdClass());
		assert(mEditor);
		mEditor->Initialize(mScene);
#pragma endregion

		#pragma region INIT_QUAD_RENDERER
		mQuadRenderer = (ER_QuadRenderer*)game.GetServices().FindService(ER_QuadRenderer::TypeIdClass());
		assert(mQuadRenderer);
		mQuadRenderer->Init();
#pragma endregion

		#pragma region INIT_GBUFFER
        game.CPUProfiler()->BeginCPUTime("Gbuffer init");
        mGBuffer = new ER_GBuffer(game, camera, game.ScreenWidth(), game.ScreenHeight());
        mGBuffer->Initialize();
        game.CPUProfiler()->EndCPUTime("Gbuffer init");
#pragma endregion

		#pragma region INIT_DIRECTIONAL_LIGHT
        mDirectionalLight = new ER_DirectionalLight(game, camera);
		XMFLOAT3 sunDirection = mScene->GetValueFromSceneRoot<XMFLOAT3>("sun_direction");
		if (abs(sunDirection.x) < std::numeric_limits<float>::epsilon() &&
			abs(sunDirection.y) < std::numeric_limits<float>::epsilon() &&
			abs(sunDirection.z) < std::numeric_limits<float>::epsilon())
		{
			XMMATRIX defaultSunRotationMatrix =
				XMMatrixRotationAxis(mDirectionalLight->RightVector(), -XMConvertToRadians(70.0f)) *
				XMMatrixRotationAxis(mDirectionalLight->UpVector(), -XMConvertToRadians(25.0f));
			mDirectionalLight->ApplyRotation(defaultSunRotationMatrix);
		}
		else
			mDirectionalLight->ApplyRotation(
				XMMatrixRotationAxis(mDirectionalLight->RightVector(), XMConvertToRadians(sunDirection.x)) *
				XMMatrixRotationAxis(mDirectionalLight->UpVector(), XMConvertToRadians(sunDirection.y)) *
				XMMatrixRotationAxis(mDirectionalLight->DirectionVector(), -XMConvertToRadians(sunDirection.z))
			);

		if (mScene->IsValueInSceneRoot("sun_color"))
			mDirectionalLight->SetColor(mScene->GetValueFromSceneRoot<XMFLOAT3>("sun_color"));
		else
			mDirectionalLight->SetColor(XMFLOAT3(1.0, 0.0, 0.0));

		if (mScene->IsValueInSceneRoot("sun_intensity"))
			mDirectionalLight->mLightIntensity = mScene->GetValueFromSceneRoot<float>("sun_intensity");

#pragma endregion

		#pragma region INIT_SKYBOX
		game.CPUProfiler()->BeginCPUTime("Skybox init");
        mSkybox = new ER_Skybox(game, camera, *mDirectionalLight, 10000);
        mSkybox->Initialize();
		game.CPUProfiler()->EndCPUTime("Skybox init");
#pragma endregion

		#pragma region INIT_SHADOWMAPPER
		game.CPUProfiler()->BeginCPUTime("Shadow mapper init");
        mShadowMapper = new ER_ShadowMapper(game, camera, *mDirectionalLight, (ShadowQuality)ER_Settings::ShadowsQuality);
        mDirectionalLight->RotationUpdateEvent->AddListener("shadow mapper", [&]() { mShadowMapper->ApplyTransform(); });
		game.CPUProfiler()->EndCPUTime("Shadow mapper init");
#pragma endregion

		#pragma region INIT_POST_PROCESSING
		game.CPUProfiler()->BeginCPUTime("Post processing stack init");
        mPostProcessingStack = new ER_PostProcessingStack(game, camera);
		mScene->LoadPostProcessingVolumesData();
        mPostProcessingStack->Initialize();
		game.CPUProfiler()->EndCPUTime("Post processing stack init");
#pragma endregion

		#pragma region INIT_ILLUMINATION
		game.CPUProfiler()->BeginCPUTime("Illumination init");
        mIllumination = new ER_Illumination(game, camera, *mDirectionalLight, *mShadowMapper, mScene, (GIQuality)ER_Settings::GlobalIlluminationQuality);
		game.CPUProfiler()->EndCPUTime("Illumination init");
#pragma endregion

		#pragma region INIT_VOLUMETRIC_CLOUDS
		game.CPUProfiler()->BeginCPUTime("Volumetric Clouds init");
        mVolumetricClouds = new ER_VolumetricClouds(game, camera, *mDirectionalLight, *mSkybox, (VolumetricCloudsQuality)ER_Settings::VolumetricCloudsQuality);
		mVolumetricClouds->Initialize(mGBuffer->GetDepth());
		game.CPUProfiler()->EndCPUTime("Volumetric Clouds init");
#pragma endregion	

		#pragma region INIT_VOLUMETRIC_FOG
		game.CPUProfiler()->BeginCPUTime("Volumetric Fog init");
		mVolumetricFog = new ER_VolumetricFog(game, *mDirectionalLight, *mShadowMapper, (VolumetricFogQuality)ER_Settings::VolumetricFogQuality);
		mVolumetricFog->Initialize();
		mVolumetricFog->SetEnabled(mScene->GetValueFromSceneRoot<bool>("volumetric_fog_enabled"));
		game.CPUProfiler()->EndCPUTime("Volumetric Fog init");
#pragma endregion

		#pragma region INIT_LIGHTPROBES_MANAGER
		game.CPUProfiler()->BeginCPUTime("Light probes manager init");
		mLightProbesManager = new ER_LightProbesManager(game, camera, mScene, mDirectionalLight, mShadowMapper);
		mLightProbesManager->SetLevelPath(ER_Utility::ToWideString(sceneFolderPath));
		mIllumination->SetProbesManager(mLightProbesManager);
		game.CPUProfiler()->EndCPUTime("Light probes manager init");
#pragma endregion

		#pragma region INIT_TERRAIN
		if (mScene->IsValueInSceneRoot("terrain_num_tiles"))
		{
			game.CPUProfiler()->BeginCPUTime("Terrain init");
			mTerrain = new ER_Terrain(game, *mDirectionalLight);
			mTerrain->SetLevelPath(ER_Utility::ToWideString(sceneFolderPath));
			mTerrain->LoadTerrainData(mScene);
			game.CPUProfiler()->EndCPUTime("Terrain init");

			//place ER_RenderingObjects on terrain (if needed)
			for (auto& object : mScene->objects)
			{
				object.second->PlaceProcedurallyOnTerrain(true);
			}
		}
#pragma endregion

		#pragma region INIT_FOLIAGE_MANAGER
		if (mScene->IsValueInSceneRoot("foliage_zones"))
		{
			game.CPUProfiler()->BeginCPUTime("Foliage init");
			mFoliageSystem = new ER_FoliageManager(game, mScene, *mDirectionalLight, (FoliageQuality)ER_Settings::FoliageQuality);
			mFoliageSystem->FoliageSystemInitializedEvent->AddListener("foliage initialized for GI", [&]() { mIllumination->SetFoliageSystemForGI(mFoliageSystem); });
			mFoliageSystem->Initialize();
			game.CPUProfiler()->EndCPUTime("Foliage init");
		}
#pragma endregion

		#pragma region INIT_WIND
		game.CPUProfiler()->BeginCPUTime("Wind init");
		mWind = new ER_Wind(game, camera);
		game.CPUProfiler()->EndCPUTime("Wind init");
#pragma endregion

		#pragma region INIT_GPU_CULLER
		game.CPUProfiler()->BeginCPUTime("GPU Culler init");
		mGPUCuller = new ER_GPUCuller(game, camera);
		mGPUCuller->Initialize();
		game.CPUProfiler()->EndCPUTime("GPU Culler init");
#pragma endregion

		#pragma region INIT_MATERIAL_CALLBACKS
		game.CPUProfiler()->BeginCPUTime("Material callbacks init");
		ER_MaterialSystems materialSystems;
		materialSystems.mCamera = &camera;
		materialSystems.mDirectionalLight = mDirectionalLight;
		materialSystems.mShadowMapper = mShadowMapper;
		materialSystems.mProbesManager = mLightProbesManager;
		materialSystems.mIllumination = mIllumination;

		for (auto& object : mScene->objects) 
		{
			for (auto& layeredMaterial : object.second->GetMaterials())
			{
				// assign prepare callbacks to standard materials (non-standard ones are processed from their own systems)
				if (layeredMaterial.second->IsStandard())
				{
					object.second->MeshMaterialVariablesUpdateEvent->AddListener(layeredMaterial.first,
						[&, matSystems = materialSystems](int meshIndex, int lodIndex) { 
							layeredMaterial.second->PrepareResourcesForStandardMaterial(matSystems, object.second, meshIndex, mScene->GetStandardMaterialRootSignature(layeredMaterial.first));
						}
					);
				}
			}
		}
		game.CPUProfiler()->EndCPUTime("Material callbacks init");
#pragma endregion

		rhi->EndGraphicsCommandList(rhi->GetPrepareGraphicsCommandListIndex());
		rhi->ExecuteCommandLists(rhi->GetPrepareGraphicsCommandListIndex());

		rhi->WaitForGpuOnGraphicsFence(); // we need to wait for the GPU to finish before running any callbacks (i.e., terrain, mip generation replacement, etc)
		
		rhi->ReplaceOriginalTexturesWithMipped();

		if (mTerrain)
		{
			for (auto listener : mTerrain->ReadbackPlacedPositionsOnInitEvent->GetListeners())
				listener(mTerrain);
			mTerrain->ReadbackPlacedPositionsOnInitEvent->RemoveAllListeners();
		}
    }

	void ER_Sandbox::Update(ER_Core& game, const ER_CoreTime& gameTime)
	{
		mSkybox->Update(gameTime);
		mSkybox->UpdateSun(gameTime);

		mGBuffer->Update(gameTime);

		mPostProcessingStack->Update();

		mVolumetricClouds->Update(gameTime);

		mVolumetricFog->Update(gameTime);

		if (mTerrain)
			mTerrain->Update(gameTime);

		mIllumination->Update(gameTime, mScene);

		if (mLightProbesManager->IsEnabled())
			mLightProbesManager->UpdateProbes(game);

		mShadowMapper->Update(gameTime);

		if (mFoliageSystem && mFoliageSystem->HasFoliage())
			mFoliageSystem->Update(gameTime);

		// TODO: consider moving all debug gizmos to a separate debug renderer system
		mWind->UpdateProxyModel(gameTime,
			((ER_Camera*)game.GetServices().FindService(ER_Camera::TypeIdClass()))->ViewMatrix4X4(),
			((ER_Camera*)game.GetServices().FindService(ER_Camera::TypeIdClass()))->ProjectionMatrix4X4());
		// TODO: consider moving all debug gizmos to a separate debug renderer system
		mDirectionalLight->UpdateProxyModel(gameTime, 
			((ER_Camera*)game.GetServices().FindService(ER_Camera::TypeIdClass()))->ViewMatrix4X4(),
			((ER_Camera*)game.GetServices().FindService(ER_Camera::TypeIdClass()))->ProjectionMatrix4X4());
		
		for (auto& pointLight : mPointLights)
			pointLight->Update(gameTime);

		for (auto& object : mScene->objects)
			object.second->Update(gameTime);

        UpdateImGui();
	}

    void ER_Sandbox::UpdateImGui()
    {
        ImGui::Begin("Gfx Systems Config");

        if (ImGui::Button("Post Processing Stack") && mPostProcessingStack)
            mPostProcessingStack->Config();

		if (ImGui::Button("Volumetric Clouds") && mVolumetricClouds)
			mVolumetricClouds->Config();

		if (ImGui::Button("Volumetric Fog") && mVolumetricFog)
			mVolumetricFog->Config();

		if (ImGui::Button("GBuffer") && mGBuffer)
			mGBuffer->Config();

        if (ImGui::Button("Illumination") && mIllumination)
			mIllumination->Config();

		if (ImGui::Button("Foliage") && mFoliageSystem)
			mFoliageSystem->Config();

		if (ImGui::Button("Terrain") && mTerrain)
			mTerrain->Config();

        ImGui::End();
    }

	void ER_Sandbox::Draw(ER_Core& game, const ER_CoreTime& gameTime)
	{
		ER_RHI* rhi = game.GetRHI();
		rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		
		#pragma region GPU_CULLING
		rhi->BeginEventTag("EveryRay: GPU Culling");
		mGPUCuller->PerformCull(mScene);
		rhi->EndEventTag();
#pragma endregion

		#pragma region DRAW_GBUFFER
		rhi->BeginEventTag("EveryRay: GBuffer");
		{
			mGBuffer->Start();

			rhi->BeginEventTag("EveryRay: GBuffer (objects)");
			mGBuffer->Draw(mScene);
			rhi->EndEventTag();

			rhi->BeginEventTag("EveryRay: GBuffer (terrain)");
			if (mTerrain)
			{
				mTerrain->Draw(TerrainRenderPass::TERRAIN_GBUFFER,
					{ mGBuffer->GetAlbedo(), mGBuffer->GetNormals(), mGBuffer->GetPositions(), mGBuffer->GetExtraBuffer(), mGBuffer->GetExtra2Buffer() }, mGBuffer->GetDepth());
			}
			rhi->EndEventTag();

			rhi->BeginEventTag("EveryRay: GBuffer (foliage)");
			if (mFoliageSystem)
			{
				mFoliageSystem->Draw(gameTime, nullptr, FoliageRenderingPass::FOLIAGE_GBUFFER,
					{ mGBuffer->GetAlbedo(), mGBuffer->GetNormals(), mGBuffer->GetPositions(), mGBuffer->GetExtraBuffer(), mGBuffer->GetExtra2Buffer() }, mGBuffer->GetDepth());
			}
			rhi->EndEventTag();

			mGBuffer->End();
		}
		rhi->EndEventTag();
#pragma endregion
		
		#pragma region DRAW_SHADOWS
		rhi->BeginEventTag("EveryRay: Shadow Maps");
		{
			mShadowMapper->Draw(mScene, mTerrain);
		}
		rhi->EndEventTag();
#pragma endregion
		
		#pragma region DRAW_GLOBAL_ILLUMINATION
		rhi->BeginEventTag("EveryRay: Compute/load light probes");
		{
			// compute static GI (load probes if they exist on disk, otherwise - compute and save them)
			{
				if (mLightProbesManager->IsEnabled() && !mLightProbesManager->AreProbesReady())
				{
					game.CPUProfiler()->BeginCPUTime("Compute or load light probes");
					mLightProbesManager->ComputeOrLoadLocalProbes(game, mScene->objects, mSkybox);
					mLightProbesManager->ComputeOrLoadGlobalProbes(game, mScene->objects, mSkybox);
					game.CPUProfiler()->EndCPUTime("Compute or load light probes");
				}
				else if (!mLightProbesManager->IsEnabled() && !mLightProbesManager->AreGlobalProbesReady())
					mLightProbesManager->ComputeOrLoadGlobalProbes(game, mScene->objects, mSkybox);
			}
		}
		rhi->EndEventTag();

		// compute dynamic GI
		rhi->BeginEventTag("EveryRay: Dynamic Global Illumination");
		{
			mIllumination->DrawDynamicGlobalIllumination(mGBuffer, gameTime);
		}
		rhi->EndEventTag();
#pragma endregion

		#pragma region DRAW_LOCAL_ILLUMINATION
		rhi->BeginEventTag("EveryRay: Local Illumination");
		{
			mIllumination->DrawLocalIllumination(mGBuffer, mSkybox);
			ER_RHI_GPUTexture* localRT = mIllumination->GetLocalIlluminationRT();

			//Terrain rendering is now in deferred; uncomment code below if you want to render in forward
			// rhi->BeginEventTag("EveryRay: Forward Lighting (terrain)");
			// 
			//#pragma region DRAW_TERRAIN_FORWARD
			//if (mTerrain)
			//	mTerrain->Draw(TerrainRenderPass::FORWARD, localRT, mShadowMapper, mLightProbesManager);
			// rhi->EndEventTag();
			//#pragma endregion
		}
		rhi->EndEventTag();
#pragma endregion

		#pragma region DRAW_DEBUG_GIZMOS
		// TODO: consider moving all debug gizmos to a separate debug renderer system
		if (ER_Utility::IsEditorMode)
		{
			rhi->BeginEventTag("EveryRay: Debug gizmos");
			{
				ER_RHI_GPUTexture* localRT = mIllumination->GetLocalIlluminationRT();

				mIllumination->DrawDebugProbes(localRT, mGBuffer->GetDepth());

				rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_LINELIST);
				ER_RHI_GPURootSignature* debugGizmoRootSignature = mScene->GetStandardMaterialRootSignature(ER_MaterialHelper::basicColorMaterialName);
				rhi->SetRootSignature(debugGizmoRootSignature);
				{
					mIllumination->DrawDebugGizmos(localRT, mGBuffer->GetDepth(), debugGizmoRootSignature);
					mDirectionalLight->DrawProxyModel(localRT, mGBuffer->GetDepth(), gameTime, debugGizmoRootSignature);
					mWind->DrawProxyModel(localRT, mGBuffer->GetDepth(), gameTime, debugGizmoRootSignature);
					if (mTerrain)
						mTerrain->DrawDebugGizmos(localRT, mGBuffer->GetDepth(), debugGizmoRootSignature);
					if (mFoliageSystem)
						mFoliageSystem->DrawDebugGizmos(localRT, mGBuffer->GetDepth(), debugGizmoRootSignature);
					if (mPostProcessingStack)
						mPostProcessingStack->DrawPostEffectsVolumesDebugGizmos(localRT, mGBuffer->GetDepth(), debugGizmoRootSignature);
					for (auto& it = mScene->objects.begin(); it != mScene->objects.end(); it++)
						it->second->DrawAABB(localRT, mGBuffer->GetDepth(), debugGizmoRootSignature);
				}
				rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			}
			rhi->EndEventTag();
		}
#pragma endregion
		
		#pragma region DRAW_COMPOSITE
		// combine the results of local and global illumination
		rhi->BeginEventTag("EveryRay: Composite Illumination");
		{
			mIllumination->CompositeTotalIllumination(mGBuffer);
		}
		rhi->EndEventTag();
#pragma endregion
		
		#pragma region DRAW_VOLUMETRIC_FOG
		rhi->BeginEventTag("EveryRay: Volumetric Fog");
		{
			mVolumetricFog->Draw();
		}
		rhi->EndEventTag();
#pragma endregion

		#pragma region DRAW_VOLUMETRIC_CLOUDS
		rhi->BeginEventTag("EveryRay: Volumetric Clouds");
		{
			mVolumetricClouds->Draw(gameTime);
		}
		rhi->EndEventTag();
#pragma endregion	

		#pragma region DRAW_POSTPROCESSING
		rhi->BeginEventTag("EveryRay: Post Processing");
		{
			auto quad = (ER_QuadRenderer*)game.GetServices().FindService(ER_QuadRenderer::TypeIdClass());
			mPostProcessingStack->Begin(mIllumination->GetFinalIlluminationRT(), mGBuffer->GetDepth());
			mPostProcessingStack->DrawEffects(gameTime, quad, mGBuffer, mVolumetricClouds, mVolumetricFog);
			mPostProcessingStack->End();
		}
		rhi->EndEventTag();
#pragma endregion

		// reset back to main RT before UI rendering
		rhi->SetMainRenderTargets();

		#pragma region DRAW_IMGUI
		rhi->BeginEventTag("EveryRay: ImGui");
		{
			rhi->SetGPUDescriptorHeapImGui(rhi->GetCurrentGraphicsCommandListIndex());

			ImGui::Render();
			rhi->RenderDrawDataImGui();
		}
		rhi->EndEventTag();
#pragma endregion
	}
}
