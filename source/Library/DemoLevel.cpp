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
        DeleteObject(mGI);
        DeleteObject(mScene);
	}

	void DemoLevel::Create()
	{
	}

	void DemoLevel::UpdateLevel(const GameTime& time)
	{

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
        mGBuffer = new GBuffer(game, camera, game.ScreenWidth(), game.ScreenHeight());
        mGBuffer->Initialize();

        mScene = new Scene(game, camera, sceneName);

        mKeyboard = (Keyboard*)game.Services().GetService(Keyboard::TypeIdClass());
        assert(mKeyboard != nullptr);

        mSkybox = new Skybox(game, camera, Utility::GetFilePath(Utility::ToWideString(mScene->skyboxPath)), 10000);
        mSkybox->Initialize();

        mGrid = new Grid(game, camera, 200, 56, XMFLOAT4(0.961f, 0.871f, 0.702f, 1.0f));
        mGrid->Initialize();
        mGrid->SetColor((XMFLOAT4)ColorHelper::LightGray);

        mDirectionalLight = new DirectionalLight(game, camera);
        mDirectionalLight->ApplyRotation(XMMatrixRotationAxis(mDirectionalLight->RightVector(), -XMConvertToRadians(70.0f)) * XMMatrixRotationAxis(mDirectionalLight->UpVector(), -XMConvertToRadians(25.0f)));
        mDirectionalLight->SetAmbientColor(mScene->ambientColor);
        mDirectionalLight->SetSunColor(mScene->sunColor);

        mShadowMapper = new ShadowMapper(game, camera, *mDirectionalLight, 4096, 4096);
        mDirectionalLight->RotationUpdateEvent->AddListener("shadow mapper", [&]() {mShadowMapper->ApplyTransform(); });

        mRenderStateHelper = new RenderStateHelper(game);

        mPostProcessingStack = new Rendering::PostProcessingStack(game, camera);
        mPostProcessingStack->Initialize(false, false, true, true, true, false, true, false);
        mPostProcessingStack->SetDirectionalLight(mDirectionalLight);
        mPostProcessingStack->SetSunOcclusionSRV(mSkybox->GetSunOcclusionOutputTexture());

        mFoliageSystem = new FoliageSystem();

        camera.SetPosition(mScene->cameraPosition);
        camera.SetDirection(mScene->cameraDirection);
        camera.SetFarPlaneDistance(100000.0f);

        mVolumetricClouds = new VolumetricClouds(game, camera, *mDirectionalLight, *mPostProcessingStack, *mSkybox);

        mGI = new Illumination(game, camera, *mDirectionalLight, *mShadowMapper, mScene);
        mGI->SetFoliageSystem(mFoliageSystem);
    }

}
