#include "ER_Sandbox.h"
#include "ER_Utility.h"
#include "ER_MaterialsCallbacks.h"
#include "ER_Core.h"
#include "ER_CoreTime.h"
#include "ER_CoreException.h"
#include "ER_Editor.h"
#include "ER_QuadRenderer.h"
#include "ER_Camera.h"
#include "ER_DirectionalLight.h"
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
#include "ER_Illumination.h"
#include "ER_LightProbesManager.h"

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
		game.CPUProfiler()->EndCPUTime("Destroying scene: " + mName);
	}

    void ER_Sandbox::Initialize(ER_Core& game, ER_Camera& camera, const std::string& sceneName, const std::string& sceneFolderPath)
    {
		mName = sceneName;

		ER_RHI* rhi = game.GetRHI();
		assert(rhi);

		rhi->BeginGraphicsCommandList(rhi->GetPrepareGraphicsCommandListIndex()); // for texture loading etc. (everything before the first frame starts)

		#pragma region INIT_SCENE
		game.CPUProfiler()->BeginCPUTime("Scene init: " + sceneName);
        mScene = new ER_Scene(game, camera, sceneFolderPath + sceneName + ".json");
		//TODO move to scene
        camera.SetPosition(mScene->cameraPosition);
        camera.SetDirection(mScene->cameraDirection);
        camera.SetFarPlaneDistance(100000.0f);
		game.CPUProfiler()->EndCPUTime("Scene init: " + sceneName);
#pragma endregion

		#pragma region INIT_EDITOR
		mEditor = (ER_Editor*)game.GetServices().FindService(ER_Editor::TypeIdClass());
		assert(mEditor);
		mEditor->LoadScene(mScene);
#pragma endregion

		#pragma region INIT_QUAD_RENDERER
		mQuadRenderer = (ER_QuadRenderer*)game.GetServices().FindService(ER_QuadRenderer::TypeIdClass());
		assert(mQuadRenderer);
		mQuadRenderer->Setup();
#pragma endregion

		#pragma region INIT_GBUFFER
        game.CPUProfiler()->BeginCPUTime("Gbuffer init");
        mGBuffer = new ER_GBuffer(game, camera, game.ScreenWidth(), game.ScreenHeight());
        mGBuffer->Initialize();
        game.CPUProfiler()->EndCPUTime("Gbuffer init");
#pragma endregion

		#pragma region INIT_CONTROLS
        mKeyboard = (ER_Keyboard*)game.GetServices().FindService(ER_Keyboard::TypeIdClass());
        assert(mKeyboard);
#pragma endregion

		#pragma region INIT_SKYBOX
		game.CPUProfiler()->BeginCPUTime("Skybox init");
        mSkybox = new ER_Skybox(game, camera, 10000);
        mSkybox->Initialize();
		game.CPUProfiler()->EndCPUTime("Skybox init");
#pragma endregion

		#pragma region INIT_DIRECTIONAL_LIGHT
        mDirectionalLight = new ER_DirectionalLight(game, camera);
        mDirectionalLight->ApplyRotation(XMMatrixRotationAxis(mDirectionalLight->RightVector(), -XMConvertToRadians(70.0f)) * XMMatrixRotationAxis(mDirectionalLight->UpVector(), -XMConvertToRadians(25.0f)));
        mDirectionalLight->SetAmbientColor(mScene->ambientColor);
        mDirectionalLight->SetSunColor(mScene->sunColor);
#pragma endregion

		#pragma region INIT_SHADOWMAPPER
		game.CPUProfiler()->BeginCPUTime("Shadow mapper init");
        mShadowMapper = new ER_ShadowMapper(game, camera, *mDirectionalLight, 2048, 2048);
        mDirectionalLight->RotationUpdateEvent->AddListener("shadow mapper", [&]() { mShadowMapper->ApplyTransform(); });
		game.CPUProfiler()->EndCPUTime("Shadow mapper init");
#pragma endregion

		#pragma region INIT_POST_PROCESSING
		game.CPUProfiler()->BeginCPUTime("Post processing stack init");
        mPostProcessingStack = new ER_PostProcessingStack(game, camera);
        mPostProcessingStack->Initialize(false, false, true, true, true, false, false, false);
        mPostProcessingStack->SetDirectionalLight(mDirectionalLight);
		game.CPUProfiler()->EndCPUTime("Post processing stack init");
#pragma endregion

		#pragma region INIT_ILLUMINATION
		game.CPUProfiler()->BeginCPUTime("Illumination init");
        mIllumination = new ER_Illumination(game, camera, *mDirectionalLight, *mShadowMapper, mScene);
		game.CPUProfiler()->EndCPUTime("Illumination init");
#pragma endregion

		#pragma region INIT_VOLUMETRIC_CLOUDS
		game.CPUProfiler()->BeginCPUTime("Volumetric Clouds init");
        mVolumetricClouds = new ER_VolumetricClouds(game, camera, *mDirectionalLight, *mSkybox);
		mVolumetricClouds->Initialize(mGBuffer->GetDepth());
		game.CPUProfiler()->EndCPUTime("Volumetric Clouds init");
#pragma endregion	

		#pragma region INIT_VOLUMETRIC_FOG
		game.CPUProfiler()->BeginCPUTime("Volumetric Fog init");
		mVolumetricFog = new ER_VolumetricFog(game, *mDirectionalLight, *mShadowMapper);
		mVolumetricFog->Initialize();
		mVolumetricFog->SetEnabled(mScene->HasVolumetricFog());
		game.CPUProfiler()->EndCPUTime("Volumetric Fog init");
#pragma endregion

		#pragma region INIT_LIGHTPROBES_MANAGER
		game.CPUProfiler()->BeginCPUTime("Light probes manager init");
		mLightProbesManager = new ER_LightProbesManager(game, camera, mScene, *mDirectionalLight, *mShadowMapper);
		mLightProbesManager->SetLevelPath(ER_Utility::ToWideString(sceneFolderPath));
		mIllumination->SetProbesManager(mLightProbesManager);
		game.CPUProfiler()->EndCPUTime("Light probes manager init");
#pragma endregion

		#pragma region INIT_TERRAIN
		game.CPUProfiler()->BeginCPUTime("Terrain init");
		mTerrain = new ER_Terrain(game, *mDirectionalLight);
		mTerrain->SetLevelPath(ER_Utility::ToWideString(sceneFolderPath));
		mTerrain->LoadTerrainData(mScene);
		game.CPUProfiler()->EndCPUTime("Terrain init");
#pragma endregion

		#pragma region INIT_FOLIAGE_MANAGER
		game.CPUProfiler()->BeginCPUTime("Foliage init");
		mFoliageSystem = new ER_FoliageManager(game, mScene, *mDirectionalLight);
		mFoliageSystem->FoliageSystemInitializedEvent->AddListener("foliage initialized for GI",  [&]() { mIllumination->SetFoliageSystemForGI(mFoliageSystem); });
		mFoliageSystem->Initialize();
		game.CPUProfiler()->EndCPUTime("Foliage init");
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
						[&, matSystems = materialSystems](int meshIndex) { 
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
    }

	void ER_Sandbox::Update(ER_Core& game, const ER_CoreTime& gameTime)
	{
		//TODO refactor to updates for elements of ER_CoreComponent type

		//TODO refactor skybox updates
		mSkybox->SetUseCustomSkyColor(mEditor->IsSkyboxUsingCustomColor());
		mSkybox->SetSkyColors(mEditor->GetBottomSkyColor(), mEditor->GetTopSkyColor());
		mSkybox->SetSunData(mDirectionalLight->IsSunRendered(),
			XMFLOAT4(mDirectionalLight->Direction().x, mDirectionalLight->Direction().y, mDirectionalLight->Direction().z, 1.0),
			XMFLOAT4(mDirectionalLight->GetDirectionalLightColor().x, mDirectionalLight->GetDirectionalLightColor().y, mDirectionalLight->GetDirectionalLightColor().z, 1.0),
			mDirectionalLight->GetSunBrightness(), mDirectionalLight->GetSunExponent());
		mSkybox->Update();
		mSkybox->UpdateSun(gameTime);
		mPostProcessingStack->Update();
		mVolumetricClouds->Update(gameTime);
		mVolumetricFog->Update(gameTime);
		mTerrain->Update(gameTime);
		mIllumination->Update(gameTime, mScene);
		if (mScene->HasLightProbesSupport() && mLightProbesManager->IsEnabled())
			mLightProbesManager->UpdateProbes(game);
		mShadowMapper->Update(gameTime);
		mFoliageSystem->Update(gameTime, mWindGustDistance, mWindStrength, mWindFrequency);
		mDirectionalLight->UpdateProxyModel(gameTime, 
			((ER_Camera*)game.GetServices().FindService(ER_Camera::TypeIdClass()))->ViewMatrix4X4(),
			((ER_Camera*)game.GetServices().FindService(ER_Camera::TypeIdClass()))->ProjectionMatrix4X4()); //TODO refactor to DebugRenderer

		for (auto& object : mScene->objects)
			object.second->Update(gameTime);

        UpdateImGui();
	}

    void ER_Sandbox::UpdateImGui()
    {
        ImGui::Begin("Systems Config");

        if (ImGui::Button("Post Processing Stack")) 
            mPostProcessingStack->Config();

		if (ImGui::Button("Volumetric Clouds"))
			mVolumetricClouds->Config();

		if (ImGui::Button("Volumetric Fog"))
			mVolumetricFog->Config();

        if (ImGui::Button("Global Illumination"))
			mIllumination->Config();

		if (ImGui::Button("Foliage"))
			mFoliageSystem->Config();

		if (ImGui::Button("Terrain"))
			mTerrain->Config();

		if (ImGui::CollapsingHeader("Wind"))
		{
			ImGui::SliderFloat("Wind strength", &mWindStrength, 0.0f, 100.0f);
			ImGui::SliderFloat("Wind gust distance", &mWindGustDistance, 0.0f, 100.0f);
			ImGui::SliderFloat("Wind frequency", &mWindFrequency, 0.0f, 100.0f);
		}

		//TODO shadow mapper config
		//TODO skybox config

        ImGui::End();
    }

	void ER_Sandbox::Draw(ER_Core& game, const ER_CoreTime& gameTime)
	{
		ER_RHI* rhi = game.GetRHI();
		rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		
		#pragma region DRAW_GBUFFER
		mGBuffer->Start();
		mGBuffer->Draw(mScene);
		if (mTerrain)
		{
			mTerrain->Draw(TerrainRenderPass::TERRAIN_GBUFFER, 
				{ mGBuffer->GetAlbedo(), mGBuffer->GetNormals(), mGBuffer->GetPositions(), mGBuffer->GetExtraBuffer(), mGBuffer->GetExtra2Buffer() }, mGBuffer->GetDepth());
		}
		if (mFoliageSystem)
		{
			mFoliageSystem->Draw(gameTime, nullptr, FoliageRenderingPass::FOLIAGE_GBUFFER,
				{ mGBuffer->GetAlbedo(), mGBuffer->GetNormals(), mGBuffer->GetPositions(), mGBuffer->GetExtraBuffer(), mGBuffer->GetExtra2Buffer() }, mGBuffer->GetDepth());
		}
		mGBuffer->End();
#pragma endregion
		
		#pragma region DRAW_SHADOWS
		mShadowMapper->Draw(mScene, mTerrain);
#pragma endregion
		
		#pragma region DRAW_GLOBAL_ILLUMINATION
		
		// compute static GI (load probes if they exist on disk, otherwise - compute them)
		{
			if (mScene->HasLightProbesSupport() && !mLightProbesManager->AreProbesReady())
			{
				game.CPUProfiler()->BeginCPUTime("Compute or load light probes");
				mLightProbesManager->ComputeOrLoadLocalProbes(game, mScene->objects, mSkybox);
				mLightProbesManager->ComputeOrLoadGlobalProbes(game, mScene->objects, mSkybox);
				game.CPUProfiler()->EndCPUTime("Compute or load light probes");
			}
			else if (!mLightProbesManager->IsEnabled() && !mLightProbesManager->AreGlobalProbesReady())
				mLightProbesManager->ComputeOrLoadGlobalProbes(game, mScene->objects, mSkybox);
		}

		// compute dynamic GI
		{
			//mIllumination->DrawDynamicGlobalIllumination(mGBuffer, gameTime);
		}
#pragma endregion

		#pragma region DRAW_VOLUMETRIC_FOG
		mVolumetricFog->Draw();
#pragma endregion

		#pragma region DRAW_LOCAL_ILLUMINATION
		
		mIllumination->DrawLocalIllumination(mGBuffer, mSkybox);
		ER_RHI_GPUTexture* localRT = mIllumination->GetLocalIlluminationRT();

		#pragma region DRAW_STANDARD_MATERIALS
		// Passes for all other materials (which are called "standard") that are rendered in "Forward" way into local illumination RT.
		// This can be used for all kinds of materials that are layered onto each other (transparent ones can also be rendered here).
		// TODO: We'd better render objects in batches per material in order to reduce SetRootSignature()/SetPSO() calls etc. Code below is not optimal
		for (auto& it = mScene->objects.begin(); it != mScene->objects.end(); it++)
		{
			for (auto& mat : it->second->GetMaterials())
			{
				if (mat.second->IsStandard())
				{
					it->second->Draw(mat.first);
					rhi->UnsetPSO();
				}
			}
		}
#pragma endregion

//		Terrain rendering is now in deferred; uncomment code below if you want to render in forward
//		#pragma region DRAW_TERRAIN_FORWARD
//		if (mTerrain)
//			mTerrain->Draw(TerrainRenderPass::FORWARD, localRT, mShadowMapper, mLightProbesManager);
//		#pragma endregion

		#pragma region DRAW_DEBUG_GIZMOS
		// TODO: consider moving all debug gizmos to a separate debug renderer system
		if (ER_Utility::IsEditorMode)
		{
			mIllumination->DrawDebugProbes(localRT);

			rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_LINELIST);
			ER_RHI_GPURootSignature* debugGizmoRootSignature = mScene->GetStandardMaterialRootSignature(ER_MaterialHelper::basicColorMaterialName);
			rhi->SetRootSignature(debugGizmoRootSignature);
			{
				mIllumination->DrawDebugGizmos(localRT, debugGizmoRootSignature);
				mDirectionalLight->DrawProxyModel(localRT, gameTime, debugGizmoRootSignature);
				mTerrain->DrawDebugGizmos(localRT, debugGizmoRootSignature);
				mFoliageSystem->DrawDebugGizmos(localRT, debugGizmoRootSignature);
				for (auto& it = mScene->objects.begin(); it != mScene->objects.end(); it++)
					it->second->DrawAABB(localRT, debugGizmoRootSignature);
			}
			rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		}
#pragma endregion

#pragma endregion
		
		// combine the results of local and global illumination
		mIllumination->CompositeTotalIllumination();

		#pragma region DRAW_VOLUMETRIC_CLOUDS
		mVolumetricClouds->Draw(gameTime);
#pragma endregion	

		#pragma region DRAW_POSTPROCESSING
		auto quad = (ER_QuadRenderer*)game.GetServices().FindService(ER_QuadRenderer::TypeIdClass());
		mPostProcessingStack->Begin(mIllumination->GetFinalIlluminationRT(), mGBuffer->GetDepth());
		mPostProcessingStack->DrawEffects(gameTime, quad, mGBuffer, mVolumetricClouds, mVolumetricFog);
		mPostProcessingStack->End();
#pragma endregion

		// reset back to main RT before UI rendering
		rhi->SetMainRenderTargets();

		#pragma region DRAW_IMGUI
		rhi->SetGPUDescriptorHeapImGui(rhi->GetCurrentGraphicsCommandListIndex());

		ImGui::Render();
		rhi->RenderDrawDataImGui();
#pragma endregion
	}
}
