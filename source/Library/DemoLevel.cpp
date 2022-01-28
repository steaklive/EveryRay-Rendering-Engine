#include "DemoLevel.h"
#include "Utility.h"
#include "Systems.inl"

namespace Library {

	void DemoLevel::Destroy()
	{
        DeleteObject(mRenderStateHelper);
        DeleteObject(mDirectionalLight);
        DeleteObject(mSkybox);
        DeleteObject(mGrid);
        DeleteObject(mPostProcessingStack);
        DeleteObject(mGBuffer);
        DeleteObject(mShadowMapper);
        DeleteObject(mFoliageSystem);
        DeleteObject(mVolumetricClouds);
        DeleteObject(mIllumination);
        DeleteObject(mScene);
        DeleteObject(mIlluminationProbesManager);
	}

	void DemoLevel::Create()
	{
	}

	void DemoLevel::UpdateLevel(const GameTime& gameTime)
	{
		mSkybox->Update(gameTime);
		mSkybox->UpdateSun(gameTime);
		mGrid->Update(gameTime);
		mPostProcessingStack->Update();
		mVolumetricClouds->Update(gameTime);
		mIllumination->Update(gameTime, mScene);
		mShadowMapper->Update(gameTime);

		mScene->GetCamera().Cull(mScene->objects);
		for (auto& object : mScene->objects)
			object.second->Update(gameTime);

        UpdateImGui();
	}

    void DemoLevel::UpdateImGui()
    {
        ImGui::Begin("Systems Config");

        if (ImGui::Button("Post Processing Stack")) 
            mPostProcessingStack->Config();

		if (ImGui::Button("Volumetric Clouds"))
			mVolumetricClouds->Config();

        if (ImGui::Button("Global Illumination"))
			mIllumination->Config();

        ImGui::End();
    }

	void DemoLevel::DrawLevel(const GameTime& time)
	{

	}

	bool DemoLevel::IsComponent()
	{
		return false;
	}

    void DemoLevel::Initialize(Game& game, Camera& camera, const std::string& sceneName)
    {
        game.CPUProfiler()->BeginCPUTime("Gbuffer init");
        mGBuffer = new GBuffer(game, camera, game.ScreenWidth(), game.ScreenHeight());
        mGBuffer->Initialize();
        game.CPUProfiler()->EndCPUTime("Gbuffer init");

		game.CPUProfiler()->BeginCPUTime("Scene init: " + sceneName);
        mScene = new Scene(game, camera, sceneName);
		game.CPUProfiler()->EndCPUTime("Scene init: " + sceneName);

        camera.SetPosition(mScene->cameraPosition);
        camera.SetDirection(mScene->cameraDirection);
        camera.SetFarPlaneDistance(100000.0f);

        mKeyboard = (Keyboard*)game.Services().GetService(Keyboard::TypeIdClass());
        assert(mKeyboard != nullptr);

		game.CPUProfiler()->BeginCPUTime("Skybox init");
        mSkybox = new Skybox(game, camera, Utility::GetFilePath(Utility::ToWideString(mScene->skyboxPath)), 10000);
        mSkybox->Initialize();
		game.CPUProfiler()->EndCPUTime("Skybox init");

        mGrid = new Grid(game, camera, 200, 56, XMFLOAT4(0.961f, 0.871f, 0.702f, 1.0f));
        mGrid->Initialize();
        mGrid->SetColor((XMFLOAT4)ColorHelper::LightGray);

        mDirectionalLight = new DirectionalLight(game, camera);
        mDirectionalLight->ApplyRotation(XMMatrixRotationAxis(mDirectionalLight->RightVector(), -XMConvertToRadians(70.0f)) * XMMatrixRotationAxis(mDirectionalLight->UpVector(), -XMConvertToRadians(25.0f)));
        mDirectionalLight->SetAmbientColor(mScene->ambientColor);
        mDirectionalLight->SetSunColor(mScene->sunColor);

		game.CPUProfiler()->BeginCPUTime("Shadow mapper init");
        mShadowMapper = new ShadowMapper(game, camera, *mDirectionalLight, 4096, 4096);
        mDirectionalLight->RotationUpdateEvent->AddListener("shadow mapper", [&]() { mShadowMapper->ApplyTransform(); });
		game.CPUProfiler()->EndCPUTime("Shadow mapper init");

        mRenderStateHelper = new RenderStateHelper(game);

		game.CPUProfiler()->BeginCPUTime("Post processing stack init");
        mPostProcessingStack = new Rendering::PostProcessingStack(game, camera);
        mPostProcessingStack->Initialize(false, false, true, true, true, false, false, false);
        mPostProcessingStack->SetDirectionalLight(mDirectionalLight);
        mPostProcessingStack->SetSunOcclusionSRV(mSkybox->GetSunOcclusionOutputTexture());
		game.CPUProfiler()->EndCPUTime("Post processing stack init");

		game.CPUProfiler()->BeginCPUTime("Volumetric Clouds init");
        mVolumetricClouds = new VolumetricClouds(game, camera, *mDirectionalLight, *mPostProcessingStack, *mSkybox);
		game.CPUProfiler()->EndCPUTime("Volumetric Clouds init");

		game.CPUProfiler()->BeginCPUTime("Illumination init");
        mIllumination = new Illumination(game, camera, *mDirectionalLight, *mShadowMapper, mScene);
		game.CPUProfiler()->EndCPUTime("Illumination init");

		game.CPUProfiler()->BeginCPUTime("Light probes manager init");
        mIlluminationProbesManager = new ER_IlluminationProbeManager(game, camera, mScene, *mDirectionalLight, *mShadowMapper);
		mIllumination->SetProbesManager(mIlluminationProbesManager);
		game.CPUProfiler()->EndCPUTime("Light probes manager init");

		game.CPUProfiler()->BeginCPUTime("Foliage init");
        mFoliageSystem = new FoliageSystem();
        mFoliageSystem->FoliageSystemInitializedEvent->AddListener("foliage initialized for GI",  [&]() { mIllumination->SetFoliageSystemForGI(mFoliageSystem); });
		game.CPUProfiler()->EndCPUTime("Foliage init");
    }

}
