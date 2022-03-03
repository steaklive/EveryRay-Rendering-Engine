#include "stdafx.h"

#include "TestSceneDemo.h"
#include "..\Library\Systems.inl"
#include "..\Library\Materials.inl"
#include "..\Library\Utility.h"
#include "..\Library\ER_MaterialsCallbacks.h"

namespace Rendering
{
	RTTI_DEFINITIONS(TestSceneDemo)

	TestSceneDemo::TestSceneDemo(Game& game, Camera& camera, Editor& editor)
		: DrawableGameComponent(game, camera, editor)
	{
	}

	TestSceneDemo::~TestSceneDemo()
	{

	}

	bool TestSceneDemo::IsComponent()
	{
		return mGame->IsInGameLevels<TestSceneDemo*>(mGame->levels, this);
	}
	void TestSceneDemo::Create()
	{
		DemoLevel::Initialize(*mGame, *mCamera, Utility::GetFilePath("content\\levels\\testScene\\testScene.json"));
		Initialize();
		mGame->levels.push_back(this);
	}
	void TestSceneDemo::Destroy()
	{
		DemoLevel::Destroy();
		TestSceneDemo::~TestSceneDemo();
		mGame->levels.clear();
	}

	void TestSceneDemo::Initialize()
	{
		SetCurrentDirectory(Utility::ExecutableDirectory().c_str());

		ER_MaterialSystems materialSystems;
		materialSystems.mCamera = mCamera;
		materialSystems.mDirectionalLight = mDirectionalLight;
		materialSystems.mShadowMapper = mShadowMapper;

        for (auto& object : mScene->objects) {
            if (!object.second->IsForwardShading())
				object.second->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::deferredPrepassMaterialName, [&, matSystems = materialSystems](int meshIndex) { Library::ER_MaterialsCallbacks::UpdateDeferredPrepassMaterialVariables(matSystems, object.second, meshIndex); });
            object.second->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::shadowMapMaterialName + " " + std::to_string(0), [&, matSystems = materialSystems](int meshIndex) { Library::ER_MaterialsCallbacks::UpdateShadowMappingMaterialVariables(matSystems, object.second, meshIndex, 0); });
            object.second->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::shadowMapMaterialName + " " + std::to_string(1), [&, matSystems = materialSystems](int meshIndex) { Library::ER_MaterialsCallbacks::UpdateShadowMappingMaterialVariables(matSystems, object.second, meshIndex, 1); });
            object.second->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::shadowMapMaterialName + " " + std::to_string(2), [&, matSystems = materialSystems](int meshIndex) { Library::ER_MaterialsCallbacks::UpdateShadowMappingMaterialVariables(matSystems, object.second, meshIndex, 2); });
        }

		mEditor->LoadScene(mScene);

		//mFoliageSystem->AddFoliage(new Foliage(*mGame, *mCamera, *mDirectionalLight, 3500, Utility::GetFilePath("content\\textures\\foliage\\grass_type1.png"), 2.5f, 100.0f, XMFLOAT3(0.0, 0.0, 0.0), FoliageBillboardType::SINGLE));
		//mFoliageSystem->AddFoliage(new Foliage(*mGame, *mCamera, *mDirectionalLight, 100, Utility::GetFilePath("content\\textures\\foliage\\grass_flower_type1.png"), 3.5f, 100.0f, XMFLOAT3(0.0, 0.0, 0.0), FoliageBillboardType::SINGLE));
		//mFoliageSystem->AddFoliage(new Foliage(*mGame, *mCamera, *mDirectionalLight, 50, Utility::GetFilePath("content\\textures\\foliage\\grass_flower_type3.png"), 2.5f, 100.0f, XMFLOAT3(0.0, 0.0, 0.0), FoliageBillboardType::SINGLE));
		//mFoliageSystem->AddFoliage(new Foliage(*mGame, *mCamera, *mDirectionalLight, 100, Utility::GetFilePath("content\\textures\\foliage\\grass_flower_type10.png"), 3.5f, 100.0f, XMFLOAT3(0.0, 0.0, 0.0), FoliageBillboardType::SINGLE));
		mFoliageSystem->Initialize();

		mIlluminationProbesManager->SetLevelPath(Utility::GetFilePath(L"content\\levels\\testScene\\"));
	}

	void TestSceneDemo::UpdateLevel(const GameTime& gameTime)
	{
		DemoLevel::UpdateLevel(gameTime);
		mFoliageSystem->Update(gameTime, mWindGustDistance, mWindStrength, mWindFrequency);

		mDirectionalLight->UpdateProxyModel(gameTime, mCamera->ViewMatrix4X4(), mCamera->ProjectionMatrix4X4());
		mSkybox->SetUseCustomSkyColor(mEditor->IsSkyboxUsingCustomColor());
		mSkybox->SetSkyColors(mEditor->GetBottomSkyColor(), mEditor->GetTopSkyColor());
		mSkybox->SetSunData(mDirectionalLight->IsSunRendered(),
			mDirectionalLight->DirectionVector(),
			XMVECTOR{ mDirectionalLight->GetDirectionalLightColor().x, mDirectionalLight->GetDirectionalLightColor().y, mDirectionalLight->GetDirectionalLightColor().z, 1.0 },
			mDirectionalLight->GetSunBrightness(), mDirectionalLight->GetSunExponent());
		mSkybox->Update(gameTime);
		mIlluminationProbesManager->UpdateProbes(*mGame);
		mEditor->Update(gameTime);
		
		UpdateImGui();
	}

	void TestSceneDemo::UpdateImGui()
	{
		ImGui::Begin("Test Demo Scene");
		ImGui::SliderFloat("Wind strength", &mWindStrength, 0.0f, 100.0f);
		ImGui::SliderFloat("Wind gust distance", &mWindGustDistance, 0.0f, 100.0f);
		ImGui::SliderFloat("Wind frequency", &mWindFrequency, 0.0f, 100.0f);
		ImGui::End();
	}

	void TestSceneDemo::DrawLevel(const GameTime& gameTime)
	{
		mRenderStateHelper->SaveRasterizerState();

		ID3D11DeviceContext* direct3DDeviceContext = mGame->Direct3DDeviceContext();
		direct3DDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		
		#pragma region GBUFFER_PREPASS
		
		mGBuffer->Start();

		//draw scene objects in gbuffer
		for (auto it = mScene->objects.begin(); it != mScene->objects.end(); it++)
			it->second->Draw(MaterialHelper::deferredPrepassMaterialName, true);

		//draw foliage in gbuffer
		mFoliageSystem->Draw(gameTime, nullptr, FoliageRenderingPass::TO_GBUFFER);

		mGBuffer->End();

#pragma endregion

		#pragma region DRAW_SHADOWS
		mShadowMapper->Draw(mScene);		
		mRenderStateHelper->RestoreRasterizerState();
		
#pragma endregion

		#pragma region DRAW_GI
		
		if (!mIlluminationProbesManager->AreProbesReady())
		{
			mGame->CPUProfiler()->BeginCPUTime("Compute or load light probes");
			mIlluminationProbesManager->ComputeOrLoadProbes(*mGame, gameTime, mScene->objects, mSkybox);
			mGame->CPUProfiler()->EndCPUTime("Compute or load light probes");
		}
		
		mRenderStateHelper->SaveAll();
		mIllumination->DrawGlobalIllumination(mGBuffer, gameTime);
		mRenderStateHelper->RestoreAll();
#pragma endregion

		mPostProcessingStack->Begin(true, mGBuffer->GetDepth());

		#pragma region DRAW_LIGHTING
		mSkybox->Draw();
		mIllumination->DrawLocalIllumination(mGBuffer, mPostProcessingStack->GetMainRenderTarget(), Utility::IsEditorMode);

		if (Utility::IsEditorMode)
		{
			mDirectionalLight->DrawProxyModel(gameTime); //todo move to Illumination() or better to separate debug renderer system
		}
#pragma endregion

		mPostProcessingStack->End();

		#pragma region DRAW_SUN
		mSkybox->DrawSun(nullptr, mPostProcessingStack);
#pragma endregion

		#pragma region DRAW_VOLUMETRIC_CLOUDS
		mVolumetricClouds->Draw(gameTime);
#pragma endregion

		#pragma region DRAW_POSTPROCESSING
		mPostProcessingStack->UpdateCompositeLightingMaterial(mPostProcessingStack->GetMainRenderTarget()->GetSRV(), mIllumination->GetGlobaIlluminationSRV(), mIllumination->GetDebugVoxels(), mIllumination->GetDebugAO());
		mPostProcessingStack->UpdateSSRMaterial(mGBuffer->GetNormals()->GetSRV(), mGBuffer->GetDepth()->getSRV(), mGBuffer->GetExtraBuffer()->GetSRV(), (float)gameTime.TotalGameTime());
		mPostProcessingStack->DrawEffects(gameTime);
#pragma endregion
		
		mRenderStateHelper->SaveAll();

		#pragma region DRAW_IMGUI
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

#pragma endregion
	}
}