#include "stdafx.h"

#include "ParallaxMappingDemo.h"
#include "..\Library\Game.h"
#include "..\Library\GameException.h"
#include "..\Library\VectorHelper.h"
#include "..\Library\MatrixHelper.h"
#include "..\Library\ColorHelper.h"
#include "..\Library\MaterialHelper.h"
#include "..\Library\Camera.h"
#include "..\Library\Model.h"
#include "..\Library\Mesh.h"
#include "..\Library\Utility.h"
#include "..\Library\DirectionalLight.h"
#include "..\Library\Keyboard.h"
#include "..\Library\Light.h"
#include "..\Library\Skybox.h"
#include "..\Library\Grid.h"
#include "..\Library\DemoLevel.h"
#include "..\Library\RenderStateHelper.h"
#include "..\Library\RenderingObject.h"
#include "..\Library\DepthMap.h"
#include "..\Library\Frustum.h"
#include "..\Library\StandardLightingMaterial.h"
#include "..\Library\ScreenSpaceReflectionsMaterial.h"
#include "..\Library\DepthMapMaterial.h"
#include "..\Library\PostProcessingStack.h"
#include "..\Library\FullScreenRenderTarget.h"
#include "..\Library\IBLRadianceMap.h"
#include "..\Library\DeferredMaterial.h"
#include "..\Library\ParallaxMappingTestMaterial.h"
#include "..\Library\GBuffer.h"
#include "..\Library\FullScreenQuad.h"
#include "..\Library\ShadowMapper.h"
#include "..\Library\Terrain.h"
#include "..\Library\Foliage.h"
#include "..\Library\Scene.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <WICTextureLoader.h>
#include <DDSTextureLoader.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <sstream>

namespace Rendering
{
	RTTI_DEFINITIONS(ParallaxMappingDemo)

		ParallaxMappingDemo::ParallaxMappingDemo(Game& game, Camera& camera)
		: DrawableGameComponent(game, camera),
		mWorldMatrix(MatrixHelper::Identity),
		mRenderStateHelper(nullptr),
		mSkybox(nullptr),
		mGrid(nullptr),
		mGBuffer(nullptr),
		mScene(nullptr)
		//mTerrain(nullptr)
	{
	}

	ParallaxMappingDemo::~ParallaxMappingDemo()
	{
		DeleteObject(mRenderStateHelper);
		DeleteObject(mSkybox);
		DeleteObject(mGrid);
		DeleteObject(mPostProcessingStack);
		DeleteObject(mGBuffer);
		ReleaseObject(mHeightMap);
		ReleaseObject(mEnvMap);
		DeleteObject(mScene);
	}

#pragma region COMPONENT_METHODS
	/////////////////////////////////////////////////////////////
	// 'DemoLevel' ugly methods...
	bool ParallaxMappingDemo::IsComponent()
	{
		return mGame->IsInGameLevels<ParallaxMappingDemo*>(mGame->levels, this);
	}
	void ParallaxMappingDemo::Create()
	{
		Initialize();
		mGame->levels.push_back(this);
	}
	void ParallaxMappingDemo::Destroy()
	{
		this->~ParallaxMappingDemo();
		mGame->levels.clear();
	}
	/////////////////////////////////////////////////////////////  
#pragma endregion

	void ParallaxMappingDemo::Initialize()
	{
		SetCurrentDirectory(Utility::ExecutableDirectory().c_str());

		mGBuffer = new GBuffer(*mGame, *mCamera, mGame->ScreenWidth(), mGame->ScreenHeight());
		mGBuffer->Initialize();

		if (FAILED(DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), Utility::GetFilePath(L"content\\textures\\pebbles.dds").c_str(), nullptr, &mHeightMap)))
			throw GameException("Failed to load pebbles.dds");

		if (FAILED(DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), Utility::GetFilePath(L"content\\textures\\environment.dds").c_str(), nullptr, &mEnvMap)))
			throw GameException("Failed to load environment.dds");

		mScene = new Scene(*mGame, *mCamera, Utility::GetFilePath("content\\levels\\parallaxScene.json"));
		for (auto& object : mScene->objects) {
			object.second->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::parallaxMaterialName, [&](int meshIndex) { UpdateParallaxMaterialVariables(object.first, meshIndex); });
		}

		mKeyboard = (Keyboard*)mGame->Services().GetService(Keyboard::TypeIdClass());
		assert(mKeyboard != nullptr);

		mSkybox = new Skybox(*mGame, *mCamera, Utility::GetFilePath(Utility::ToWideString(mScene->skyboxPath)), 10000);
		mSkybox->Initialize();

		mGrid = new Grid(*mGame, *mCamera, 200, 56, XMFLOAT4(0.961f, 0.871f, 0.702f, 1.0f));
		mGrid->Initialize();
		mGrid->SetColor((XMFLOAT4)ColorHelper::LightGray);

		mRenderStateHelper = new RenderStateHelper(*mGame);

		//PP
		mPostProcessingStack = new PostProcessingStack(*mGame, *mCamera);
		mPostProcessingStack->Initialize(false, false, true, true, true, false);

		mCamera->SetPosition(mScene->cameraPosition);
		mCamera->SetDirection(mScene->cameraDirection);
		mCamera->SetFarPlaneDistance(100000.0f);
		mCamera->ApplyRotation(XMMatrixRotationAxis(mCamera->RightVector(), XMConvertToRadians(-90.0f)));
	}

	void ParallaxMappingDemo::UpdateLevel(const GameTime& gameTime)
	{
		UpdateImGui();

		mSkybox->Update(gameTime);
		mGrid->Update(gameTime);
		mPostProcessingStack->Update();

		mCamera->Cull(mScene->objects);

		for (auto& object : mScene->objects)
			object.second->Update(gameTime);
	}

	void ParallaxMappingDemo::UpdateImGui()
	{

		ImGui::Begin("Parallax Occlusion Mapping Scene");
		ImGui::Checkbox("Show Post Processing Stack", &mPostProcessingStack->isWindowOpened);
		if (mPostProcessingStack->isWindowOpened) mPostProcessingStack->ShowPostProcessingWindow();
		ImGui::End();

		ImGui::Begin("Object data");
		ImGui::SliderFloat("Metalness", &mMetalness, 0.0f, 1.0f);
		ImGui::SliderFloat("Roughness", &mRoughness, 0.0f, 1.0f);
		ImGui::Checkbox("Use AO", &mUseAO);
		ImGui::End();

		ImGui::Begin("Light data");
		ImGui::SliderFloat3("Light position", mLightPos, -10.0f, 10.0f);
		ImGui::End();

		ImGui::Begin("Other data");
		ImGui::SliderFloat("Parallax Scale", &mParallaxScale, 0.0f, 1.0f);
		ImGui::Checkbox("Use soft shadows", &mParallaxUseSoftShadows);
		ImGui::Checkbox("Demonstrate calculated AO", &mDemonstrateAO);
		ImGui::Checkbox("Demonstrate calculated Normals", &mDemonstrateNormal);
		ImGui::End();
	}

	void ParallaxMappingDemo::DrawLevel(const GameTime& gameTime)
	{
		float clear_color[4] = { 0.0f, 1.0f, 1.0f, 0.0f };

		mRenderStateHelper->SaveRasterizerState();

		ID3D11DeviceContext* direct3DDeviceContext = mGame->Direct3DDeviceContext();
		direct3DDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

#pragma region DEFERRED_PREPASS

		//mGBuffer->Start();
		//for (auto it = mScene->objects.begin(); it != mScene->objects.end(); it++)
		//	it->second->Draw(MaterialHelper::deferredPrepassMaterialName, true);
		//mGBuffer->End();

#pragma endregion

#pragma region DRAW_SHADOWS
		//for (int i = 0; i < MAX_NUM_OF_CASCADES; i++)
		//{
		//	mShadowMapper->BeginRenderingToShadowMap(i);
		//	const std::string name = MaterialHelper::shadowMapMaterialName + " " + std::to_string(i);

		//	XMMATRIX lvp = mShadowMapper->GetViewMatrix(i) * mShadowMapper->GetProjectionMatrix(i);
		//	int objectIndex = 0;
		//	for (auto it = mScene->objects.begin(); it != mScene->objects.end(); it++, objectIndex++)
		//	{
		//		static_cast<DepthMapMaterial*>(it->second->GetMaterials()[name])->LightViewProjection() << lvp;
		//		//static_cast<DepthMapMaterial*>(it->second->GetMaterials()[name])->AlbedoAlphaMap() << it->second->GetTextureData(objectIndex).AlbedoMap;
		//		it->second->Draw(name, true);
		//	}

		//	mShadowMapper->StopRenderingToShadowMap(i);
		//}

		//mRenderStateHelper->RestoreRasterizerState();

#pragma endregion

		mPostProcessingStack->Begin();

#pragma region DRAW_LIGHTING

		//skybox
		mSkybox->Draw(gameTime);

		//grid
		if (Utility::IsEditorMode)
			mGrid->Draw(gameTime);

		//gizmo
		//if (Utility::IsEditorMode)
		//	mDirectionalLight->DrawProxyModel(gameTime);

		//lighting
		//for (auto it = mScene->objects.begin(); it != mScene->objects.end(); it++)
		//	it->second->Draw(MaterialHelper::lightingMaterialName);

		for (auto it = mScene->objects.begin(); it != mScene->objects.end(); it++)
			it->second->Draw(MaterialHelper::parallaxMaterialName);

#pragma endregion

		mPostProcessingStack->End(gameTime);
		mPostProcessingStack->UpdateSSRMaterial(mGBuffer->GetNormals()->getSRV(), mGBuffer->GetDepth()->getSRV(), mGBuffer->GetExtraBuffer()->getSRV(), (float)gameTime.TotalGameTime());
		mPostProcessingStack->DrawEffects(gameTime);

		mRenderStateHelper->SaveAll();

#pragma region DRAW_IMGUI
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

#pragma endregion
	}

	void ParallaxMappingDemo::UpdateParallaxMaterialVariables(const std::string& objectName, int meshIndex)
	{
		if (!mScene->objects[objectName]->GetMaterials()[MaterialHelper::parallaxMaterialName])
			return;

		XMMATRIX worldMatrix = XMLoadFloat4x4(&(mScene->objects[objectName]->GetTransformationMatrix4X4()));
		XMMATRIX vp = mCamera->ViewMatrix() * mCamera->ProjectionMatrix();

		static_cast<ParallaxMappingTestMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::parallaxMaterialName])->ViewProjection() << vp;
		static_cast<ParallaxMappingTestMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::parallaxMaterialName])->World() << worldMatrix;
		static_cast<ParallaxMappingTestMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::parallaxMaterialName])->CameraPosition() << mCamera->PositionVector();
		static_cast<ParallaxMappingTestMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::parallaxMaterialName])->Albedo() << XMVECTOR{ 0.5, 0.5, 0.5, 1.0 };
		static_cast<ParallaxMappingTestMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::parallaxMaterialName])->Metalness() << mMetalness;
		static_cast<ParallaxMappingTestMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::parallaxMaterialName])->Roughness() << mRoughness;
		static_cast<ParallaxMappingTestMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::parallaxMaterialName])->LightPosition() << XMVECTOR{ mLightPos[0], mLightPos[1], mLightPos[2], 1.0f };
		static_cast<ParallaxMappingTestMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::parallaxMaterialName])->LightColor() << XMVECTOR{ 1.0f, 1.0f, 1.0f, 1.0f };
		float ao = (mUseAO) ? 1.0f : 0.0f;
		static_cast<ParallaxMappingTestMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::parallaxMaterialName])->UseAmbientOcclusion() << ao;
		static_cast<ParallaxMappingTestMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::parallaxMaterialName])->POMHeightScale() << mParallaxScale;
		float softshadows = (mParallaxUseSoftShadows) ? 1.0f : 0.0f;
		static_cast<ParallaxMappingTestMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::parallaxMaterialName])->POMSoftShadows() << softshadows;
		float demoAO = (mDemonstrateAO) ? 1.0f : 0.0f;
		static_cast<ParallaxMappingTestMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::parallaxMaterialName])->DemonstrateAO() << demoAO;
		float demoNormal = (mDemonstrateNormal) ? 1.0f : 0.0f;
		static_cast<ParallaxMappingTestMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::parallaxMaterialName])->DemonstrateNormal() << demoNormal;
		static_cast<ParallaxMappingTestMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::parallaxMaterialName])->HeightMap() << mHeightMap;
		static_cast<ParallaxMappingTestMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::parallaxMaterialName])->EnvMap() << mEnvMap;
	}
}