#include "ER_Sandbox.h"
#include "ER_Utility.h"
#include "Systems.inl"
#include "ER_MaterialsCallbacks.h"

namespace Library {

	ER_Sandbox::ER_Sandbox()
	{
	}
	ER_Sandbox::~ER_Sandbox()
	{
	}
	
	void ER_Sandbox::Destroy(Game& game)
	{
		game.CPUProfiler()->BeginCPUTime("Destroying scene: " + mName);
		DeleteObject(mRenderStateHelper);
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

    void ER_Sandbox::Initialize(Game& game, ER_Camera& camera, const std::string& sceneName, const std::string& sceneFolderPath)
    {
        mRenderStateHelper = new RenderStateHelper(game);
		mName = sceneName;

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
		mEditor = (ER_Editor*)game.Services().GetService(ER_Editor::TypeIdClass());
		assert(mEditor);
		mEditor->LoadScene(mScene);
#pragma endregion

		#pragma region INIT_GBUFFER
        game.CPUProfiler()->BeginCPUTime("Gbuffer init");
        mGBuffer = new ER_GBuffer(game, camera, game.ScreenWidth(), game.ScreenHeight());
        mGBuffer->Initialize();
        game.CPUProfiler()->EndCPUTime("Gbuffer init");
#pragma endregion

		#pragma region INIT_CONTROLS
        mKeyboard = (ER_Keyboard*)game.Services().GetService(ER_Keyboard::TypeIdClass());
        assert(mKeyboard);
#pragma endregion

		#pragma region INIT_SKYBOX
		game.CPUProfiler()->BeginCPUTime("Skybox init");
        mSkybox = new ER_Skybox(game, camera, 10000);
        mSkybox->Initialize();
		game.CPUProfiler()->EndCPUTime("Skybox init");
#pragma endregion

		#pragma region INIT_DIRECTIONAL_LIGHT
        mDirectionalLight = new DirectionalLight(game, camera);
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

		for (auto& object : mScene->objects) {
			for (auto& layeredMaterial : object.second->GetMaterials())
			{
				// assign prepare callbacks to non-special materials (special ones are processed and rendered from their own systems, i.e., ShadowMapper)
				if (!layeredMaterial.second->IsSpecial())
					object.second->MeshMaterialVariablesUpdateEvent->AddListener(layeredMaterial.first, [&, matSystems = materialSystems](int meshIndex) { layeredMaterial.second->PrepareForRendering(matSystems, object.second, meshIndex); });

				// gbuffer material (special, but since its draws are not processed in 
				if (layeredMaterial.first == ER_MaterialHelper::gbufferMaterialName)
					object.second->MeshMaterialVariablesUpdateEvent->AddListener(layeredMaterial.first, [&, matSystems = materialSystems](int meshIndex) { layeredMaterial.second->PrepareForRendering(matSystems, object.second, meshIndex); });
			}
		}
		game.CPUProfiler()->EndCPUTime("Material callbacks init");
#pragma endregion

    }

	void ER_Sandbox::Update(Game& game, const ER_CoreTime& gameTime)
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
			((ER_Camera*)game.Services().GetService(ER_Camera::TypeIdClass()))->ViewMatrix4X4(),
			((ER_Camera*)game.Services().GetService(ER_Camera::TypeIdClass()))->ProjectionMatrix4X4()); //TODO refactor to DebugRenderer

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

	void ER_Sandbox::Draw(Game& game, const ER_CoreTime& gameTime)
	{
		//TODO set proper RS
		//TODO set proper DS

		if (mRenderStateHelper)
			mRenderStateHelper->SaveRasterizerState();

		ID3D11DeviceContext* context = game.Direct3DDeviceContext();
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		#pragma region DRAW_GBUFFER
		mGBuffer->Start();
		mGBuffer->Draw(mScene);
		if (mFoliageSystem)
			mFoliageSystem->Draw(gameTime, nullptr, FoliageRenderingPass::TO_GBUFFER);
		mGBuffer->End();
#pragma endregion

		#pragma region DRAW_SHADOWS
		mShadowMapper->Draw(mScene, mTerrain);
#pragma endregion

		#pragma region DRAW_GLOBAL_ILLUMINATION
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

		mRenderStateHelper->SaveAll();
		mIllumination->DrawGlobalIllumination(mGBuffer, gameTime);
		mRenderStateHelper->RestoreAll();
#pragma endregion

		#pragma region DRAW_VOLUMETRIC_FOG
		mVolumetricFog->Draw();
#pragma endregion

		#pragma region DRAW_LOCAL_ILLUMINATION
		
		mIllumination->DrawLocalIllumination(mGBuffer, mSkybox);

		#pragma region DRAW_LAYERED_MATERIALS
		for (auto& it = mScene->objects.begin(); it != mScene->objects.end(); it++)
		{
			for (auto& mat : it->second->GetMaterials())
			{
				if (!mat.second->IsSpecial())
					it->second->Draw(mat.first);
			}
		}
#pragma endregion

		#pragma region DRAW_TERRAIN
		if (mTerrain)
			mTerrain->Draw(mShadowMapper, mLightProbesManager);
#pragma endregion

		#pragma region DRAW_DEBUG_GIZMOS
		// TODO: consider moving all debug gizmos to a separate debug renderer system
		if (ER_Utility::IsEditorMode)
		{
			mDirectionalLight->DrawProxyModel(gameTime);
			mIllumination->DrawDebugGizmos();
			mTerrain->DrawDebugGizmos();
			mFoliageSystem->DrawDebugGizmos();
			for (auto& it = mScene->objects.begin(); it != mScene->objects.end(); it++)
				it->second->DrawAABB();
		}
#pragma endregion

#pragma endregion
		
		mIllumination->CompositeTotalIllumination();

		#pragma region DRAW_VOLUMETRIC_CLOUDS
		mVolumetricClouds->Draw(gameTime);
#pragma endregion	

		#pragma region DRAW_POSTPROCESSING
		auto quad = (ER_QuadRenderer*)game.Services().GetService(ER_QuadRenderer::TypeIdClass());
		mPostProcessingStack->Begin(mIllumination->GetFinalIlluminationRT(), mGBuffer->GetDepth());
		mPostProcessingStack->DrawEffects(gameTime, quad, mGBuffer, mVolumetricClouds, mVolumetricFog);
		mPostProcessingStack->End();
#pragma endregion

		mRenderStateHelper->SaveAll();

		#pragma region DRAW_IMGUI
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

#pragma endregion
	}
}
