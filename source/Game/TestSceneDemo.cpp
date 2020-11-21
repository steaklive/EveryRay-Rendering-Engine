#include "stdafx.h"

#include "TestSceneDemo.h"
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
#include "..\Library\GBuffer.h"
#include "..\Library\FullScreenQuad.h"
#include "..\Library\ShadowMapper.h"
#include "..\Library\Terrain.h"
#include "..\Library\Foliage.h"

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
	RTTI_DEFINITIONS(TestSceneDemo)

	static int selectedObjectIndex = -1;

	TestSceneDemo::TestSceneDemo(Game& game, Camera& camera)
		: DrawableGameComponent(game, camera),
		mWorldMatrix(MatrixHelper::Identity),
		mRenderStateHelper(nullptr),
		mDirectionalLight(nullptr),
		mSkybox(nullptr),
		mIrradianceTextureSRV(nullptr),
		mRadianceTextureSRV(nullptr),
		mIntegrationMapTextureSRV(nullptr),
		mGrid(nullptr),
		mGBuffer(nullptr),
		mSSRQuad(nullptr),
		mShadowMapper(nullptr),
		mFoliageCollection(0, nullptr)
		//mTerrain(nullptr)
	{
	}

	TestSceneDemo::~TestSceneDemo()
	{
		for (auto object : mRenderingObjects)
		{
			object.second->MeshMaterialVariablesUpdateEvent->RemoverAllListeners();
			DeleteObject(object.second);
		}
		mRenderingObjects.clear();

		//DeleteObject(mTerrain)
		DeleteObject(mRenderStateHelper);
		DeleteObject(mDirectionalLight);
		DeleteObject(mSkybox);
		DeleteObject(mGrid);
		DeleteObject(mPostProcessingStack);
		DeleteObject(mGBuffer);
		DeleteObject(mSSRQuad);
		DeleteObject(mShadowMapper);
		DeletePointerCollection(mFoliageCollection);
		ReleaseObject(mIrradianceTextureSRV);
		ReleaseObject(mRadianceTextureSRV);
		ReleaseObject(mIntegrationMapTextureSRV);
	}

#pragma region COMPONENT_METHODS
	/////////////////////////////////////////////////////////////
	// 'DemoLevel' ugly methods...
	bool TestSceneDemo::IsComponent()
	{
		return mGame->IsInGameLevels<TestSceneDemo*>(mGame->levels, this);
	}
	void TestSceneDemo::Create()
	{
		Initialize();
		mGame->levels.push_back(this);
	}
	void TestSceneDemo::Destroy()
	{
		this->~TestSceneDemo();
		mGame->levels.clear();
	}
	/////////////////////////////////////////////////////////////  
#pragma endregion

	void TestSceneDemo::Initialize()
	{
		SetCurrentDirectory(Utility::ExecutableDirectory().c_str());

		mGBuffer = new GBuffer(*mGame, *mCamera, mGame->ScreenWidth(), mGame->ScreenHeight());
		mGBuffer->Initialize();

		// Initialize the material - lighting
		Effect* lightingEffect = new Effect(*mGame);
		lightingEffect->CompileFromFile(Utility::GetFilePath(L"content\\effects\\StandardLighting.fx"));

		// Initialize the material - shadow
		Effect* effectShadow = new Effect(*mGame);
		effectShadow->CompileFromFile(Utility::GetFilePath(L"content\\effects\\DepthMap.fx"));

		Effect* effectDeferredPrepass = new Effect(*mGame);
		effectDeferredPrepass->CompileFromFile(Utility::GetFilePath(L"content\\effects\\DeferredPrepass.fx"));

		//Effect* effectSSR = new Effect(*mGame);
		//effectSSR->CompileFromFile(Utility::GetFilePath(L"content\\effects\\SSR.fx"));


		//mTerrain = new Terrain(*mGame, *mCamera, 128, 1.0f, true);

		/**/
		////
		/**/

		mRenderingObjects.insert(std::pair<std::string, RenderingObject*>("Ground Plane", new RenderingObject("Ground Plane", *mGame, *mCamera, std::unique_ptr<Model>(new Model(*mGame, Utility::GetFilePath("content\\models\\default_plane\\default_plane.fbx"), true)), false)));
		mRenderingObjects["Ground Plane"]->LoadMaterial(new StandardLightingMaterial(), lightingEffect, MaterialHelper::lightingMaterialName);
		for (int i = 0; i < MAX_NUM_OF_CASCADES; i++)
		{
			const std::string name = MaterialHelper::shadowMapMaterialName + " " + std::to_string(i);
			mRenderingObjects["Ground Plane"]->LoadMaterial(new DepthMapMaterial(), effectShadow, name);
		}
		mRenderingObjects["Ground Plane"]->LoadMaterial(new DeferredMaterial(), effectDeferredPrepass, MaterialHelper::deferredPrepassMaterialName);
		mRenderingObjects["Ground Plane"]->LoadRenderBuffers();
		mRenderingObjects["Ground Plane"]->GetMaterials()[MaterialHelper::lightingMaterialName]->SetCurrentTechnique(mRenderingObjects["Ground Plane"]->GetMaterials()[MaterialHelper::lightingMaterialName]->GetEffect()->TechniquesByName().at("standard_lighting_pbr"));
		mRenderingObjects["Ground Plane"]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::lightingMaterialName, [&](int meshIndex) { UpdateStandardLightingPBRMaterialVariables("Ground Plane", meshIndex); });
		mRenderingObjects["Ground Plane"]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::deferredPrepassMaterialName, [&](int meshIndex) { UpdateDeferredPrepassMaterialVariables("Ground Plane", meshIndex); });
		mRenderingObjects["Ground Plane"]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::shadowMapMaterialName + " " + std::to_string(0), [&](int meshIndex) { UpdateShadow0MaterialVariables("Ground Plane", meshIndex); });
		mRenderingObjects["Ground Plane"]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::shadowMapMaterialName + " " + std::to_string(1), [&](int meshIndex) { UpdateShadow1MaterialVariables("Ground Plane", meshIndex); });
		mRenderingObjects["Ground Plane"]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::shadowMapMaterialName + " " + std::to_string(2), [&](int meshIndex) { UpdateShadow2MaterialVariables("Ground Plane", meshIndex); });
		
		/**/
		////
		/**/

		mRenderingObjects.insert(std::pair<std::string, RenderingObject*>("Acer Tree Medium", new RenderingObject("Acer Tree Medium", *mGame, *mCamera, std::unique_ptr<Model>(new Model(*mGame, Utility::GetFilePath("content\\models\\vegetation\\trees\\elm\\elm.fbx"), true)), true, true)));
		mRenderingObjects["Acer Tree Medium"]->LoadMaterial(new StandardLightingMaterial(), lightingEffect, MaterialHelper::lightingMaterialName);
		for (int i = 0; i < MAX_NUM_OF_CASCADES; i++)
		{
			const std::string name = MaterialHelper::shadowMapMaterialName + " " + std::to_string(i);
			mRenderingObjects["Acer Tree Medium"]->LoadMaterial(new DepthMapMaterial(), effectShadow, name);
		}
		mRenderingObjects["Acer Tree Medium"]->LoadMaterial(new DeferredMaterial(), effectDeferredPrepass, MaterialHelper::deferredPrepassMaterialName);

		mRenderingObjects["Acer Tree Medium"]->LoadRenderBuffers();

		mRenderingObjects["Acer Tree Medium"]->GetMaterials()[MaterialHelper::lightingMaterialName]->SetCurrentTechnique(mRenderingObjects["Acer Tree Medium"]->GetMaterials()[MaterialHelper::lightingMaterialName]->GetEffect()->TechniquesByName().at("standard_lighting_pbr_instancing"));
		for (int i = 0; i < MAX_NUM_OF_CASCADES; i++)
		{
			const std::string name = MaterialHelper::shadowMapMaterialName + " " + std::to_string(i);
			mRenderingObjects["Acer Tree Medium"]->GetMaterials()[name]->SetCurrentTechnique(mRenderingObjects["Acer Tree Medium"]->GetMaterials()[name]->GetEffect()->TechniquesByName().at("create_depthmap_w_render_target_instanced"));
		}
		mRenderingObjects["Acer Tree Medium"]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName]->SetCurrentTechnique(mRenderingObjects["Acer Tree Medium"]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName]->GetEffect()->TechniquesByName().at("deferred_instanced"));
		
		mRenderingObjects["Acer Tree Medium"]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::lightingMaterialName, [&](int meshIndex) { UpdateStandardLightingPBRMaterialVariables("Acer Tree Medium", meshIndex); });
		mRenderingObjects["Acer Tree Medium"]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::deferredPrepassMaterialName, [&](int meshIndex) { UpdateDeferredPrepassMaterialVariables("Acer Tree Medium", meshIndex); });
		mRenderingObjects["Acer Tree Medium"]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::shadowMapMaterialName + std::to_string(0), [&](int meshIndex) { UpdateShadow0MaterialVariables("Acer Tree Medium", meshIndex); });
		mRenderingObjects["Acer Tree Medium"]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::shadowMapMaterialName + std::to_string(1), [&](int meshIndex) { UpdateShadow1MaterialVariables("Acer Tree Medium", meshIndex); });
		mRenderingObjects["Acer Tree Medium"]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::shadowMapMaterialName + std::to_string(2), [&](int meshIndex) { UpdateShadow2MaterialVariables("Acer Tree Medium", meshIndex); });
		
		mRenderingObjects["Acer Tree Medium"]->CalculateInstanceObjectsRandomDistribution(10);
		mRenderingObjects["Acer Tree Medium"]->LoadInstanceBuffers();


		mKeyboard = (Keyboard*)mGame->Services().GetService(Keyboard::TypeIdClass());
		assert(mKeyboard != nullptr);

		mSkybox = new Skybox(*mGame, *mCamera, Utility::GetFilePath(L"content\\textures\\Sky_Type_4.dds"), 10000);
		mSkybox->Initialize();

		mGrid = new Grid(*mGame, *mCamera, 200, 56, XMFLOAT4(0.961f, 0.871f, 0.702f, 1.0f));
		mGrid->Initialize();
		mGrid->SetColor((XMFLOAT4)ColorHelper::LightGray);


		//directional light
		mDirectionalLight = new DirectionalLight(*mGame, *mCamera);
		mDirectionalLight->ApplyRotation(XMMatrixRotationAxis(mDirectionalLight->RightVector(), -XMConvertToRadians(70.0f)) * XMMatrixRotationAxis(mDirectionalLight->UpVector(), -XMConvertToRadians(25.0f)));

		mShadowMapper = new ShadowMapper(*mGame, *mCamera, *mDirectionalLight, 4096, 4096);
		mDirectionalLight->RotationUpdateEvent->AddListener("shadow mapper", [&]() {mShadowMapper->ApplyTransform(); });

		mRenderStateHelper = new RenderStateHelper(*mGame);

		//PP
		mPostProcessingStack = new PostProcessingStack(*mGame, *mCamera);
		mPostProcessingStack->Initialize(false, false, true, true, true, false);

		//foliage
		mFoliageCollection.push_back(new Foliage(*mGame, *mCamera, *mDirectionalLight, 1500, Utility::GetFilePath("content\\textures\\foliage\\grass_type1.png"), 2.5f, 100.0f));
		//mFoliageCollection.push_back(new Foliage(*mGame, *mCamera, *mDirectionalLight, 10000, Utility::GetFilePath("content\\textures\\foliage\\grass_type2.png"), 1.3f, 100.0f));
		//mFoliageCollection.push_back(new Foliage(*mGame, *mCamera, *mDirectionalLight, 1000, Utility::GetFilePath("content\\textures\\foliage\\grass_type3.png"), 1.5f, 100.0f));
		mFoliageCollection.push_back(new Foliage(*mGame, *mCamera, *mDirectionalLight, 3500, Utility::GetFilePath("content\\textures\\foliage\\grass_type4.png"), 2.0f, 100.0f));
		mFoliageCollection.push_back(new Foliage(*mGame, *mCamera, *mDirectionalLight, 100, Utility::GetFilePath("content\\textures\\foliage\\grass_flower_type1.png"), 3.5f, 100.0f));
		mFoliageCollection.push_back(new Foliage(*mGame, *mCamera, *mDirectionalLight, 50, Utility::GetFilePath("content\\textures\\foliage\\grass_flower_type3.png"), 2.5f, 100.0f));
		mFoliageCollection.push_back(new Foliage(*mGame, *mCamera, *mDirectionalLight, 100, Utility::GetFilePath("content\\textures\\foliage\\grass_flower_type10.png"), 3.5f, 100.0f));

		for (auto& foliage : mFoliageCollection) {
			foliage->CreateBufferGPU();
		}

		mCamera->SetPosition(XMFLOAT3(0, 8.4f, 60.0f));
		mCamera->SetFarPlaneDistance(100000.0f);
		//mCamera->ApplyRotation(XMMatrixRotationAxis(mCamera->RightVector(), XMConvertToRadians(18.0f)) * XMMatrixRotationAxis(mCamera->UpVector(), -XMConvertToRadians(70.0f)));

		//IBL
		if (FAILED(DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), Utility::GetFilePath(L"content\\textures\\Sky_Type_4_PBRDiffuseHDR.dds").c_str(), nullptr, &mIrradianceTextureSRV)))
			throw GameException("Failed to create Irradiance Map.");

		// Create an IBL Radiance Map from Environment Map
		mIBLRadianceMap.reset(new IBLRadianceMap(*mGame, Utility::GetFilePath(L"content\\textures\\Sky_Type_4.dds")));
		mIBLRadianceMap->Initialize();
		mIBLRadianceMap->Create(*mGame);

		mRadianceTextureSRV = *mIBLRadianceMap->GetShaderResourceViewAddress();
		if (mRadianceTextureSRV == nullptr)
			throw GameException("Failed to create Radiance Map.");
		mIBLRadianceMap.release();
		mIBLRadianceMap.reset(nullptr);

		// Load a pre-computed Integration Map
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), Utility::GetFilePath(L"content\\textures\\PBR\\Skyboxes\\ibl_brdf_lut.png").c_str(), nullptr, &mIntegrationMapTextureSRV)))
			throw GameException("Failed to create Integration Texture.");

	}

	void TestSceneDemo::UpdateLevel(const GameTime& gameTime)
	{
		UpdateImGui();

		mDirectionalLight->UpdateProxyModel(gameTime, mCamera->ViewMatrix4X4(), mCamera->ProjectionMatrix4X4());
		mSkybox->Update(gameTime);
		mGrid->Update(gameTime);
		mPostProcessingStack->Update();
		for (auto object : mFoliageCollection) {
			object->SetWindParams(mWindGustDistance, mWindStrength, mWindFrequency);
			object->Update(gameTime);
		}

		mCamera->Cull(mRenderingObjects);
		mShadowMapper->Update(gameTime);

		for (auto object : mRenderingObjects)
			object.second->Update(gameTime);
	}

	void TestSceneDemo::UpdateImGui()
	{

		ImGui::Begin("Test Demo Scene");

		ImGui::Checkbox("Show Post Processing Stack", &mPostProcessingStack->isWindowOpened);
		if (mPostProcessingStack->isWindowOpened) mPostProcessingStack->ShowPostProcessingWindow();

		ImGui::Separator();

		if (Utility::IsEditorMode)
		{
			ImGui::Begin("Scene Objects");

			const char* listbox_items[] = { "Ground Plane", "Acer Tree Medium" };

			ImGui::PushItemWidth(-1);
			ImGui::ListBox("##empty", &selectedObjectIndex, listbox_items, IM_ARRAYSIZE(listbox_items));

			for (size_t i = 0; i < IM_ARRAYSIZE(listbox_items); i++)
			{
				if (i == selectedObjectIndex)
					mRenderingObjects[listbox_items[selectedObjectIndex]]->Selected(true);
				else
					mRenderingObjects[listbox_items[i]]->Selected(false);
			}

			ImGui::End();
		}

		ImGui::SliderFloat("Wind strength", &mWindStrength, 0.0f, 100.0f);
		ImGui::SliderFloat("Wind gust distance", &mWindGustDistance, 0.0f, 100.0f);
		ImGui::SliderFloat("Wind frequency", &mWindFrequency, 0.0f, 100.0f);

		ImGui::End();
	}

	void TestSceneDemo::DrawLevel(const GameTime& gameTime)
	{
		float clear_color[4] = { 0.0f, 1.0f, 1.0f, 0.0f };

		mRenderStateHelper->SaveRasterizerState();

		ID3D11DeviceContext* direct3DDeviceContext = mGame->Direct3DDeviceContext();
		direct3DDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		
#pragma region DEFERRED_PREPASS
		
		mGBuffer->Start();
		for (auto it = mRenderingObjects.begin(); it != mRenderingObjects.end(); it++) 
			it->second->Draw(MaterialHelper::deferredPrepassMaterialName, true);
		mGBuffer->End();

#pragma endregion

#pragma region DRAW_SHADOWS

		for (int i = 0; i < MAX_NUM_OF_CASCADES; i++)
		{
			mShadowMapper->BeginRenderingToShadowMap(i);
			const std::string name = MaterialHelper::shadowMapMaterialName + " " + std::to_string(i);
		
			XMMATRIX lvp = mShadowMapper->GetViewMatrix(i) * mShadowMapper->GetProjectionMatrix(i);
			int objectIndex = 0;
			for (auto it = mRenderingObjects.begin(); it != mRenderingObjects.end(); it++, objectIndex++)
			{
				static_cast<DepthMapMaterial*>(it->second->GetMaterials()[name])->LightViewProjection() << lvp;
				//static_cast<DepthMapMaterial*>(it->second->GetMaterials()[name])->AlbedoAlphaMap() << it->second->GetTextureData(objectIndex).AlbedoMap;
				it->second->Draw(name, true);
			}
		
			mShadowMapper->StopRenderingToShadowMap(i);
		}

		mRenderStateHelper->RestoreRasterizerState();
		
#pragma endregion

		mPostProcessingStack->Begin();

#pragma region DRAW_LIGHTING

		//skybox
		mSkybox->Draw(gameTime);

		//terrain
		//mTerrain->Draw();


		//grid
		if (Utility::IsEditorMode)
			mGrid->Draw(gameTime);

		//gizmo
		if (Utility::IsEditorMode)
			mDirectionalLight->DrawProxyModel(gameTime);

		//lighting
		for (auto it = mRenderingObjects.begin(); it != mRenderingObjects.end(); it++)
			it->second->Draw(MaterialHelper::lightingMaterialName);

		//foliage 
		for (auto object : mFoliageCollection)
			object->Draw(gameTime);

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

	void TestSceneDemo::UpdateStandardLightingPBRMaterialVariables(const std::string& objectName, int meshIndex)
	{
		XMMATRIX worldMatrix = XMLoadFloat4x4(&(mRenderingObjects[objectName]->GetTransformationMatrix4X4()));
		XMMATRIX vp = /*worldMatrix * */mCamera->ViewMatrix() * mCamera->ProjectionMatrix();
		//XMMATRIX shadowMatrix = /*worldMatrix **/ mShadowMapper->GetViewMatrix() * mShadowMapper->GetProjectionMatrix() /*mShadowMapViewMatrix * mShadowMapProjectionMatrix*//* mShadowProjector->ViewMatrix() * mShadowProjector->ProjectionMatrix()*/ * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix());

		XMMATRIX shadowMatrices[3] = 
		{ 
			mShadowMapper->GetViewMatrix(0) *  mShadowMapper->GetProjectionMatrix(0) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix()) ,
			mShadowMapper->GetViewMatrix(1) *  mShadowMapper->GetProjectionMatrix(1) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix()) ,
			mShadowMapper->GetViewMatrix(2) *  mShadowMapper->GetProjectionMatrix(2) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix()) 
		};

		ID3D11ShaderResourceView* shadowMaps[3] = 
		{
			mShadowMapper->GetShadowTexture(0),
			mShadowMapper->GetShadowTexture(1),
			mShadowMapper->GetShadowTexture(2)
		};

		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::lightingMaterialName])->ViewProjection() << vp;
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::lightingMaterialName])->World() << worldMatrix;
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::lightingMaterialName])->ShadowMatrices().SetMatrixArray(shadowMatrices, 0, MAX_NUM_OF_CASCADES);
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::lightingMaterialName])->CameraPosition() << mCamera->PositionVector();
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::lightingMaterialName])->SunDirection() << XMVectorNegate(mDirectionalLight->DirectionVector());
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::lightingMaterialName])->SunColor() << XMVECTOR{ mDirectionalLight->GetDirectionalLightColor().x,  mDirectionalLight->GetDirectionalLightColor().y, mDirectionalLight->GetDirectionalLightColor().z , 1.0f };
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::lightingMaterialName])->AmbientColor() << XMVECTOR{ mDirectionalLight->GetAmbientLightColor().x,  mDirectionalLight->GetAmbientLightColor().y, mDirectionalLight->GetAmbientLightColor().z , 1.0f };
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::lightingMaterialName])->ShadowTexelSize() << XMVECTOR{ 1.0f / mShadowMapper->GetResolution(), 1.0f, 1.0f , 1.0f };
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::lightingMaterialName])->ShadowCascadeDistances() << XMVECTOR{ mCamera->GetCameraFarCascadeDistance(0), mCamera->GetCameraFarCascadeDistance(1), mCamera->GetCameraFarCascadeDistance(2), 1.0f };
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::lightingMaterialName])->AlbedoTexture() << mRenderingObjects[objectName]->GetTextureData(meshIndex).AlbedoMap;
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::lightingMaterialName])->NormalTexture() << mRenderingObjects[objectName]->GetTextureData(meshIndex).NormalMap;
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::lightingMaterialName])->SpecularTexture() << mRenderingObjects[objectName]->GetTextureData(meshIndex).SpecularMap;
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::lightingMaterialName])->RoughnessTexture() << mRenderingObjects[objectName]->GetTextureData(meshIndex).RoughnessMap;
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::lightingMaterialName])->MetallicTexture() << mRenderingObjects[objectName]->GetTextureData(meshIndex).MetallicMap;
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::lightingMaterialName])->CascadedShadowTextures().SetResourceArray(shadowMaps, 0, MAX_NUM_OF_CASCADES);
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::lightingMaterialName])->IrradianceTexture() << mIrradianceTextureSRV;
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::lightingMaterialName])->RadianceTexture() << mRadianceTextureSRV;
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::lightingMaterialName])->IntegrationTexture() << mIntegrationMapTextureSRV;
	}
	void TestSceneDemo::UpdateDeferredPrepassMaterialVariables(const std::string & objectName, int meshIndex)
	{
		XMMATRIX worldMatrix = XMLoadFloat4x4(&(mRenderingObjects[objectName]->GetTransformationMatrix4X4()));
		XMMATRIX vp = /*worldMatrix * */mCamera->ViewMatrix() * mCamera->ProjectionMatrix();
		static_cast<DeferredMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName])->ViewProjection() << vp;
		static_cast<DeferredMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName])->World() << worldMatrix;
		static_cast<DeferredMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName])->AlbedoMap() << mRenderingObjects[objectName]->GetTextureData(meshIndex).AlbedoMap;
		static_cast<DeferredMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName])->ReflectionMaskFactor() << mRenderingObjects[objectName]->GetMeshReflectionFactor(meshIndex);
	}
	void TestSceneDemo::UpdateShadow0MaterialVariables(const std::string & objectName, int meshIndex)
	{
		const std::string name = MaterialHelper::shadowMapMaterialName + " " + std::to_string(0);
		static_cast<DepthMapMaterial*>(mRenderingObjects[objectName]->GetMaterials()[name])->AlbedoAlphaMap() << mRenderingObjects[objectName]->GetTextureData(meshIndex).AlbedoMap;
	}
	void TestSceneDemo::UpdateShadow1MaterialVariables(const std::string & objectName, int meshIndex)
	{
		const std::string name = MaterialHelper::shadowMapMaterialName + " " + std::to_string(1);
		static_cast<DepthMapMaterial*>(mRenderingObjects[objectName]->GetMaterials()[name])->AlbedoAlphaMap() << mRenderingObjects[objectName]->GetTextureData(meshIndex).AlbedoMap;
	}
	void TestSceneDemo::UpdateShadow2MaterialVariables(const std::string & objectName, int meshIndex)
	{
		const std::string name = MaterialHelper::shadowMapMaterialName + " " + std::to_string(2);
		static_cast<DepthMapMaterial*>(mRenderingObjects[objectName]->GetMaterials()[name])->AlbedoAlphaMap() << mRenderingObjects[objectName]->GetTextureData(meshIndex).AlbedoMap;
	}
}