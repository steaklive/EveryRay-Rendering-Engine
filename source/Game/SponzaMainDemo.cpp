#include "stdafx.h"

#include "SponzaMainDemo.h"
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
#include "..\Library\ProxyModel.h"
#include "..\Library\Keyboard.h"
#include "..\Library\Light.h"
#include "..\Library\Skybox.h"
#include "..\Library\Grid.h"
#include "..\Library\DemoLevel.h"
#include "..\Library\RenderStateHelper.h"
#include "..\Library\RenderingObject.h"
#include "..\Library\DepthMap.h"
#include "..\Library\Projector.h"
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
	RTTI_DEFINITIONS(SponzaMainDemo)

	static int selectedObjectIndex = -1;
	
	SponzaMainDemo::SponzaMainDemo(Game& game, Camera& camera)
		: DrawableGameComponent(game, camera),
		mWorldMatrix(MatrixHelper::Identity),
		mRenderStateHelper(nullptr), 
		mShadowMapper(nullptr),
		mDirectionalLight(nullptr),
		mSkybox(nullptr),
		mGrid(nullptr),
		mGBuffer(nullptr),
		mSSRQuad(nullptr),
		mPostProcessingStack(nullptr)
	{
	}

	SponzaMainDemo::~SponzaMainDemo()
	{
		for (auto object : mRenderingObjects)
		{
			object.second->MeshMaterialVariablesUpdateEvent->RemoveAllListeners();
			DeleteObject(object.second);
		}
		mRenderingObjects.clear();

		DeleteObject(mRenderStateHelper);
		DeleteObject(mDirectionalLight);
		DeleteObject(mSkybox);
		DeleteObject(mGrid);
		DeleteObject(mPostProcessingStack);
		DeleteObject(mGBuffer);
		DeleteObject(mSSRQuad);
		DeleteObject(mShadowMapper);

		ReleaseObject(mIrradianceDiffuseTextureSRV);
		ReleaseObject(mIrradianceSpecularTextureSRV);
		ReleaseObject(mIntegrationMapTextureSRV);
	}

	#pragma region COMPONENT_METHODS
	/////////////////////////////////////////////////////////////
	// 'DemoLevel' ugly methods...
	bool SponzaMainDemo::IsComponent()
	{
		return mGame->IsInGameLevels<SponzaMainDemo*>(mGame->levels, this);
	}
	void SponzaMainDemo::Create()
	{
		Initialize();
		mGame->levels.push_back(this);
	}
	void SponzaMainDemo::Destroy()
	{
		this->~SponzaMainDemo();
		mGame->levels.clear();
	}
	/////////////////////////////////////////////////////////////  
#pragma endregion
	
	void SponzaMainDemo::Initialize()
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
		
		Effect* effectSSR = new Effect(*mGame);
		effectSSR->CompileFromFile(Utility::GetFilePath(L"content\\effects\\SSR.fx"));

		
		/**/
		////
		/**/
		
		mRenderingObjects.insert(std::pair<std::string, RenderingObject*>("Sponza", new RenderingObject("Sponza", -1, *mGame, *mCamera, std::unique_ptr<Model>(new Model(*mGame, Utility::GetFilePath("content\\models\\Sponza\\sponza.fbx"), true)), false)));
		mRenderingObjects["Sponza"]->LoadMaterial(new StandardLightingMaterial(), lightingEffect, MaterialHelper::forwardLightingMaterialName);
		mRenderingObjects["Sponza"]->LoadMaterial(new DepthMapMaterial(), effectShadow, MaterialHelper::shadowMapMaterialName);
		mRenderingObjects["Sponza"]->LoadMaterial(new DeferredMaterial(), effectDeferredPrepass, MaterialHelper::deferredPrepassMaterialName);
		//mRenderingObjects["Sponza"]->LoadMaterial(new ScreenSpaceReflectionsMaterial(), effectSSR, MaterialHelper::ssrMaterialName);
		mRenderingObjects["Sponza"]->LoadRenderBuffers();
		mRenderingObjects["Sponza"]->GetMaterials()[MaterialHelper::forwardLightingMaterialName]->SetCurrentTechnique(mRenderingObjects["Sponza"]->GetMaterials()[MaterialHelper::forwardLightingMaterialName]->GetEffect()->TechniquesByName().at("standard_lighting_pbr"));
		mRenderingObjects["Sponza"]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::forwardLightingMaterialName,		[&](int meshIndex) { UpdateStandardLightingPBRMaterialVariables("Sponza", meshIndex); });
		mRenderingObjects["Sponza"]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::deferredPrepassMaterialName, [&](int meshIndex) { UpdateDeferredPrepassMaterialVariables("Sponza", meshIndex); });
		mRenderingObjects["Sponza"]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::shadowMapMaterialName, [&](int meshIndex) { UpdateShadowMaterialVariables("Sponza", meshIndex); });
		mRenderingObjects["Sponza"]->SetMeshReflectionFactor(5, 1.0f);
		
		/**/
		////
		/**/
		
		mRenderingObjects.insert(std::pair<std::string, RenderingObject*>("PBR Sphere 1", new RenderingObject("PBR Sphere 1", -1, *mGame, *mCamera, std::unique_ptr<Model>(new Model(*mGame, Utility::GetFilePath("content\\models\\sphere.fbx"), true)), true)));
		mRenderingObjects["PBR Sphere 1"]->LoadMaterial(new StandardLightingMaterial(), lightingEffect, MaterialHelper::forwardLightingMaterialName);
		mRenderingObjects["PBR Sphere 1"]->LoadMaterial(new DepthMapMaterial(), effectShadow, MaterialHelper::shadowMapMaterialName);
		mRenderingObjects["PBR Sphere 1"]->LoadMaterial(new DeferredMaterial(), effectDeferredPrepass, MaterialHelper::deferredPrepassMaterialName);
		mRenderingObjects["PBR Sphere 1"]->LoadRenderBuffers();
		mRenderingObjects["PBR Sphere 1"]->GetMaterials()[MaterialHelper::forwardLightingMaterialName]->SetCurrentTechnique(mRenderingObjects["PBR Sphere 1"]->GetMaterials()[MaterialHelper::forwardLightingMaterialName]->GetEffect()->TechniquesByName().at("standard_lighting_pbr"));
		mRenderingObjects["PBR Sphere 1"]->LoadCustomMeshTextures(0,
			Utility::GetFilePath(L"content\\textures\\PBR\\Gold\\GoldMetal_albedo.jpg"),
			Utility::GetFilePath(L"content\\textures\\PBR\\Gold\\GoldMetal_nrm.jpg"),
			Utility::GetFilePath(L""),
			Utility::GetFilePath(L"content\\textures\\PBR\\Gold\\GoldMetal_rgh.jpg"),
			Utility::GetFilePath(L"content\\textures\\PBR\\Gold\\GoldMetal_mtl.jpg"),
			Utility::GetFilePath(L""),
			Utility::GetFilePath(L""),
			Utility::GetFilePath(L"")
		);
		mRenderingObjects["PBR Sphere 1"]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::forwardLightingMaterialName,			[&](int meshIndex) { UpdateStandardLightingPBRMaterialVariables("PBR Sphere 1", meshIndex); });
		mRenderingObjects["PBR Sphere 1"]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::deferredPrepassMaterialName,	[&](int meshIndex) { UpdateDeferredPrepassMaterialVariables("PBR Sphere 1", meshIndex); });
		mRenderingObjects["PBR Sphere 1"]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::shadowMapMaterialName,	[&](int meshIndex) { UpdateShadowMaterialVariables("PBR Sphere 1", meshIndex); });
		mRenderingObjects["PBR Sphere 1"]->SetMeshReflectionFactor(0, 1.0f);

		/**/
		////
		/**/

		mRenderingObjects.insert(std::pair<std::string, RenderingObject*>("PBR Dragon", new RenderingObject("PBR Dragon", -1, *mGame, *mCamera, std::unique_ptr<Model>(new Model(*mGame, Utility::GetFilePath("content\\models\\dragon\\dragon.fbx"), true)), true)));
		mRenderingObjects["PBR Dragon"]->LoadMaterial(new StandardLightingMaterial(), lightingEffect, MaterialHelper::forwardLightingMaterialName);
		mRenderingObjects["PBR Dragon"]->LoadMaterial(new DepthMapMaterial(), effectShadow, MaterialHelper::shadowMapMaterialName);
		mRenderingObjects["PBR Dragon"]->LoadMaterial(new DeferredMaterial(), effectDeferredPrepass, MaterialHelper::deferredPrepassMaterialName);
		mRenderingObjects["PBR Dragon"]->LoadRenderBuffers();
		mRenderingObjects["PBR Dragon"]->GetMaterials()[MaterialHelper::forwardLightingMaterialName]->SetCurrentTechnique(mRenderingObjects["PBR Dragon"]->GetMaterials()[MaterialHelper::forwardLightingMaterialName]->GetEffect()->TechniquesByName().at("standard_lighting_pbr"));
		mRenderingObjects["PBR Dragon"]->LoadCustomMeshTextures(0,
			Utility::GetFilePath(L"content\\textures\\PBR\\Silver\\SilverMetal_col.jpg"),
			Utility::GetFilePath(L"content\\textures\\PBR\\Silver\\SilverMetal_nrm.jpg"),
			Utility::GetFilePath(L""),
			Utility::GetFilePath(L"content\\textures\\PBR\\Silver\\SilverMetal_rgh.jpg"),
			Utility::GetFilePath(L"content\\textures\\PBR\\Silver\\EmptyMetalMap.png"),
			Utility::GetFilePath(L""),
			Utility::GetFilePath(L""),
			Utility::GetFilePath(L"")
		);
		mRenderingObjects["PBR Dragon"]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::forwardLightingMaterialName,		[&](int meshIndex) { UpdateStandardLightingPBRMaterialVariables("PBR Dragon", meshIndex); });
		mRenderingObjects["PBR Dragon"]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::deferredPrepassMaterialName, [&](int meshIndex) { UpdateDeferredPrepassMaterialVariables("PBR Dragon", meshIndex); });
		mRenderingObjects["PBR Dragon"]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::shadowMapMaterialName, [&](int meshIndex) { UpdateShadowMaterialVariables("PBR Dragon", meshIndex); });
		mRenderingObjects["PBR Dragon"]->SetTranslation(-35.0f, 0.0, -15.0);
		mRenderingObjects["PBR Dragon"]->SetScale(0.6f,0.6f,0.6f);

		mSSRQuad = new FullScreenQuad(*mGame, *(mRenderingObjects["Sponza"]->GetMaterials()[MaterialHelper::ssrMaterialName]));
		mSSRQuad->Initialize();

		mSkybox = new Skybox(*mGame, *mCamera, Utility::GetFilePath(L"content\\textures\\skyboxes\\Sky_4\\textureEnv.dds"), 100);
		mSkybox->Initialize();

		mGrid = new Grid(*mGame, *mCamera, 100, 64, XMFLOAT4(0.961f, 0.871f, 0.702f, 1.0f));
		mGrid->Initialize();
		mGrid->SetColor((XMFLOAT4)ColorHelper::LightGray);

		//directional light
		mDirectionalLight = new DirectionalLight(*mGame, *mCamera);
		mDirectionalLight->ApplyRotation(XMMatrixRotationAxis(mDirectionalLight->RightVector(), -XMConvertToRadians(80.0f)) * XMMatrixRotationAxis(mDirectionalLight->UpVector(), -XMConvertToRadians(25.0f)) *  XMMatrixRotationAxis(XMVECTOR{0.0, 0.0, 1.0f, 1.0f}, XMConvertToRadians(10.0f)));

		mRenderStateHelper = new RenderStateHelper(*mGame);

		//PP
		mPostProcessingStack = new PostProcessingStack(*mGame, *mCamera);
		mPostProcessingStack->Initialize(false, false, true, true, true, true);

		//shadows
		mShadowMapper = new ShadowMapper(*mGame, *mCamera, *mDirectionalLight, 4096, 4096, false);
		mDirectionalLight->RotationUpdateEvent->AddListener("shadow mapper", [&]() {mShadowMapper->ApplyTransform(); });

		//camera
		mCamera->SetPosition(XMFLOAT3(-76.6f, 8.4f, 8.8f));
		mCamera->ApplyRotation(XMMatrixRotationAxis(mCamera->RightVector(), XMConvertToRadians(18.0f)) * XMMatrixRotationAxis(mCamera->UpVector(), -XMConvertToRadians(70.0f)));
		mCamera->SetFarPlaneDistance(600.0f);

		//IBL
		if (FAILED(DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), Utility::GetFilePath(L"content\\textures\\skyboxes\\Sky_4\\textureDiffuseMDR.dds").c_str(), nullptr, &mIrradianceDiffuseTextureSRV)))
			throw GameException("Failed to create Diffuse Irradiance Map.");

		mIBLRadianceMap.reset(new IBLRadianceMap(*mGame, Utility::GetFilePath(L"content\\textures\\skyboxes\\Sky_4\\textureEnv.dds")));
		mIBLRadianceMap->Initialize();
		mIBLRadianceMap->Create(*mGame);

		mIrradianceSpecularTextureSRV = *mIBLRadianceMap->GetShaderResourceViewAddress();
		if (mIrradianceSpecularTextureSRV == nullptr)
			throw GameException("Failed to create Specular Irradiance Map.");
		mIBLRadianceMap.release();
		mIBLRadianceMap.reset(nullptr);

		// Load a pre-computed Integration Map
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), Utility::GetFilePath(L"content\\textures\\PBR\\Skyboxes\\ibl_brdf_lut.png").c_str(), nullptr, &mIntegrationMapTextureSRV)))
			throw GameException("Failed to create Integration Texture.");

	}

	void SponzaMainDemo::UpdateLevel(const GameTime& gameTime)
	{
		UpdateImGui();

		mDirectionalLight->UpdateProxyModel(gameTime, mCamera->ViewMatrix4X4(), mCamera->ProjectionMatrix4X4());
		mSkybox->Update(gameTime);
		mGrid->Update(gameTime);
		mPostProcessingStack->Update();

		mCamera->Cull(mRenderingObjects);
		mShadowMapper->Update(gameTime);

		for (auto object : mRenderingObjects)
			object.second->Update(gameTime);
	}


	void SponzaMainDemo::UpdateImGui()
	{

		ImGui::Begin("Sponza Demo Scene");

		ImGui::Checkbox("Show Post Processing Stack", &mPostProcessingStack->isWindowOpened);
		//if (mPostProcessingStack->isWindowOpened) mPostProcessingStack->ShowPostProcessingWindow();

		ImGui::Separator();

		if (Utility::IsEditorMode)
		{
			ImGui::Begin("Scene Objects");

			const char* listbox_items[] = { "Sponza", "PBR Sphere 1", "PBR Dragon"};

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

		for (auto object : mRenderingObjects)
			if (object.second->IsAvailableInEditor() && object.second->IsSelected())
				object.second->UpdateGizmos();

		ImGui::End();
	}

	void SponzaMainDemo::DrawLevel(const GameTime& gameTime)
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

		#pragma region SHADOWS_PASS
		
		//shadow
		mShadowMapper->BeginRenderingToShadowMap();

		XMMATRIX lvp = mShadowMapper->GetViewMatrix() * mShadowMapper->GetProjectionMatrix();
		int objectIndex = 0;
		for (auto it = mRenderingObjects.begin(); it != mRenderingObjects.end(); it++, objectIndex++)
		{
			XMMATRIX worldMatrix = XMLoadFloat4x4(&(it->second->GetTransformationMatrix4X4()));
			if (it->second->IsInstanced())
				static_cast<DepthMapMaterial*>(it->second->GetMaterials()[MaterialHelper::shadowMapMaterialName])->LightViewProjection() << lvp;
			else
				static_cast<DepthMapMaterial*>(it->second->GetMaterials()[MaterialHelper::shadowMapMaterialName])->WorldLightViewProjection() << worldMatrix * lvp;
			//static_cast<DepthMapMaterial*>(it->second->GetMaterials()[MaterialHelper::shadowMapMaterialName])->AlbedoAlphaMap() << it->second->GetTextureData(objectIndex).AlbedoMap;

			it->second->Draw(MaterialHelper::shadowMapMaterialName, true);
		}
		
		mShadowMapper->StopRenderingToShadowMap();

		mRenderStateHelper->RestoreRasterizerState();
		#pragma endregion

		mPostProcessingStack->Begin(true);

		//gizmo
		if (Utility::IsEditorMode)
			mDirectionalLight->DrawProxyModel(gameTime);

		#pragma region LIGHTING_PASS
		
		//skybox
		mSkybox->Draw();

		//grid
		if (Utility::IsEditorMode)
			mGrid->Draw(gameTime);

		//lighting
		for (auto it = mRenderingObjects.begin(); it != mRenderingObjects.end(); it++)
			it->second->Draw(MaterialHelper::forwardLightingMaterialName);
		
		#pragma endregion

		mPostProcessingStack->End();
		mPostProcessingStack->UpdateSSRMaterial(mGBuffer->GetNormals()->getSRV(), mGBuffer->GetDepth()->getSRV(), mGBuffer->GetExtraBuffer()->getSRV(), (float)gameTime.TotalGameTime());
		mPostProcessingStack->DrawEffects(gameTime);

		mRenderStateHelper->SaveAll();

		#pragma region DRAW_IMGUI
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		#pragma endregion
	}

	void SponzaMainDemo::UpdateShadowMaterialVariables(const std::string & objectName, int meshIndex)
	{
		static_cast<DepthMapMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::shadowMapMaterialName])->AlbedoAlphaMap() << mRenderingObjects[objectName]->GetTextureData(meshIndex).AlbedoMap;
	}
	void SponzaMainDemo::UpdateStandardLightingPBRMaterialVariables(const std::string& objectName, int meshIndex)
	{
		XMMATRIX worldMatrix = XMLoadFloat4x4(&(mRenderingObjects[objectName]->GetTransformationMatrix4X4()));
		XMMATRIX vp = mCamera->ViewMatrix() * mCamera->ProjectionMatrix();

		// only one cascade
		XMMATRIX shadowMatrices[3] =
		{
			mShadowMapper->GetViewMatrix(0) *  mShadowMapper->GetProjectionMatrix(0) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix()) ,
			mShadowMapper->GetViewMatrix(0) *  mShadowMapper->GetProjectionMatrix(0) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix()) ,
			mShadowMapper->GetViewMatrix(0) *  mShadowMapper->GetProjectionMatrix(0) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix()) 
		};

		ID3D11ShaderResourceView* shadowMaps[3] =
		{
			mShadowMapper->GetShadowTexture(0),
			mShadowMapper->GetShadowTexture(1),
			mShadowMapper->GetShadowTexture(2)
		};

		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->ViewProjection() << vp;
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->World() << worldMatrix;
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->ShadowMatrices().SetMatrixArray(shadowMatrices, 0, NUM_SHADOW_CASCADES);;
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->CameraPosition() << mCamera->PositionVector();
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->SunDirection() << XMVectorNegate(mDirectionalLight->DirectionVector());
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->SunColor() << XMVECTOR{ mDirectionalLight->GetDirectionalLightColor().x,  mDirectionalLight->GetDirectionalLightColor().y, mDirectionalLight->GetDirectionalLightColor().z , 1.0f };
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->AmbientColor() << XMVECTOR{ mDirectionalLight->GetAmbientLightColor().x,  mDirectionalLight->GetAmbientLightColor().y, mDirectionalLight->GetAmbientLightColor().z , 1.0f };
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->ShadowCascadeDistances() << XMVECTOR{ mCamera->FarPlaneDistance(), 0.0f, 0.0f, 1.0f };
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->ShadowTexelSize() << XMVECTOR{ 1.0f / mShadowMapper->GetResolution(), 1.0f, 1.0f , 1.0f };
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->AlbedoTexture() << mRenderingObjects[objectName]->GetTextureData(meshIndex).AlbedoMap;
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->NormalTexture() << mRenderingObjects[objectName]->GetTextureData(meshIndex).NormalMap;
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->SpecularTexture() << mRenderingObjects[objectName]->GetTextureData(meshIndex).SpecularMap;
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->RoughnessTexture() << mRenderingObjects[objectName]->GetTextureData(meshIndex).RoughnessMap;
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->MetallicTexture() << mRenderingObjects[objectName]->GetTextureData(meshIndex).MetallicMap;
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->CascadedShadowTextures().SetResourceArray(shadowMaps, 0, NUM_SHADOW_CASCADES);
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->IrradianceDiffuseTexture() << mIrradianceDiffuseTextureSRV;
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->IrradianceSpecularTexture() << mIrradianceSpecularTextureSRV;
		static_cast<StandardLightingMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->IntegrationTexture() << mIntegrationMapTextureSRV;
	}

	void SponzaMainDemo::UpdateDeferredPrepassMaterialVariables(const std::string & objectName, int meshIndex)
	{
		XMMATRIX worldMatrix = XMLoadFloat4x4(&(mRenderingObjects[objectName]->GetTransformationMatrix4X4()));
		XMMATRIX vp = /*worldMatrix * */mCamera->ViewMatrix() * mCamera->ProjectionMatrix();
		static_cast<DeferredMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName])->ViewProjection() << vp;
		static_cast<DeferredMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName])->World() << worldMatrix;
		static_cast<DeferredMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName])->AlbedoMap() << mRenderingObjects[objectName]->GetTextureData(meshIndex).AlbedoMap;
		static_cast<DeferredMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName])->RoughnessMap() << mRenderingObjects[objectName]->GetTextureData(meshIndex).RoughnessMap;
		static_cast<DeferredMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName])->ReflectionMaskFactor() << mRenderingObjects[objectName]->GetMeshReflectionFactor(meshIndex);
	}	
		
	//void SponzaMainDemo::CheckMouseIntersections()
	//{
	//	float pointX, pointY;
	//	XMMATRIX viewMatrix, inverseViewMatrix, worldMatrix, translateMatrix, inverseWorldMatrix;
	//	XMFLOAT3 direction, origin, rayOrigin, rayDirection;
	//	bool intersect, result;
	//
	//	POINT cursorPos;
	//	GetCursorPos(&cursorPos);
	//	float mouseX = 0;
	//	mouseX = cursorPos.x;
	//	float mouseY = 0;
	//	mouseY = cursorPos.y;
	//
	//
	//	// Move the mouse cursor coordinates into the -1 to +1 range.
	//	pointX = ((2.0f * (float)mouseX) / (float)mGame->ScreenWidth()) - 1.0f;
	//	pointY = (((2.0f * (float)mouseY) / (float)mGame->ScreenHeight()) - 1.0f) * -1.0f;
	//
	//	// Adjust the points using the projection matrix to account for the aspect ratio of the viewport.
	//	pointX = pointX / mCamera->ProjectionMatrix4X4()._11;
	//	pointY = pointY / mCamera->ProjectionMatrix4X4()._22;
	//
	//	// Get the inverse of the view matrix.
	//	viewMatrix = mCamera->ViewMatrix();
	//	inverseViewMatrix = XMMatrixInverse(NULL, viewMatrix);
	//
	//	// Calculate the direction of the picking ray in view space.
	//	direction.x = (pointX * inverseViewMatrix._11) + (pointY * inverseViewMatrix._21) + inverseViewMatrix._31;
	//	direction.y = (pointX * inverseViewMatrix._12) + (pointY * inverseViewMatrix._22) + inverseViewMatrix._32;
	//	direction.z = (pointX * inverseViewMatrix._13) + (pointY * inverseViewMatrix._23) + inverseViewMatrix._33;
	//
	//	// Get the origin of the picking ray which is the position of the camera.
	//	origin = mCamera->Position();
	//
	//	// Get the world matrix and translate to the location of the sphere.
	//	m_D3D->GetWorldMatrix(worldMatrix);
	//	D3DXMatrixTranslation(&translateMatrix, -5.0f, 1.0f, 5.0f);
	//	D3DXMatrixMultiply(&worldMatrix, &worldMatrix, &translateMatrix);
	//
	//	// Now get the inverse of the translated world matrix.
	//	D3DXMatrixInverse(&inverseWorldMatrix, NULL, &worldMatrix);
	//
	//	// Now transform the ray origin and the ray direction from view space to world space.
	//	D3DXVec3TransformCoord(&rayOrigin, &origin, &inverseWorldMatrix);
	//	D3DXVec3TransformNormal(&rayDirection, &direction, &inverseWorldMatrix);
	//
	//	// Normalize the ray direction.
	//	D3DXVec3Normalize(&rayDirection, &rayDirection);
	//
	//	// Now perform the ray-sphere intersection test.
	//	for (size_t i = 0; i < length; i++)
	//	{
	//		intersect = RaySphereIntersect(rayOrigin, rayDirection, 1.0f);
	//
	//		if (intersect == true)
	//		{
	//			// If it does intersect then set the intersection to "yes" in the text string that is displayed to the screen.
	//			result = m_Text->SetIntersection(true, m_D3D->GetDeviceContext());
	//		}
	//		else
	//		{
	//			// If not then set the intersection to "No".
	//			result = m_Text->SetIntersection(false, m_D3D->GetDeviceContext());
	//		}
	//	}
	//}
}