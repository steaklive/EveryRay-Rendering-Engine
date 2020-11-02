#include "stdafx.h"

#include "TerrainDemo.h"
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
	RTTI_DEFINITIONS(TerrainDemo)

	static int selectedObjectIndex = -1;

	TerrainDemo::TerrainDemo(Game& game, Camera& camera)
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
		mTerrain(nullptr),
		mPostProcessingStack(nullptr)
	{
	}

	TerrainDemo::~TerrainDemo()
	{
		#pragma region DELETE_RENDEROBJECTS
		for (auto object : mRenderingObjects)
		{
			object.second->MeshMaterialVariablesUpdateEvent->RemoverAllListeners();
			DeleteObject(object.second);
		}
		mRenderingObjects.clear();
#pragma endregion
		
		#pragma region DELETE_FOLIAGE
		for (auto foliageZone : mFoliageZonesCollections)
		{
			for (auto object : foliageZone)
			{
				DeleteObject(object.second);
			}
			foliageZone.clear();
		}
		mFoliageZonesCollections.clear();
#pragma endregion
		
		DeleteObject(mTerrain)
		DeleteObject(mRenderStateHelper);
		DeleteObject(mDirectionalLight);
		DeleteObject(mSkybox);
		DeleteObject(mGrid);
		DeleteObject(mPostProcessingStack);
		DeleteObject(mGBuffer);
		DeleteObject(mSSRQuad);
		DeleteObject(mShadowMapper);

		ReleaseObject(mIrradianceTextureSRV);
		ReleaseObject(mRadianceTextureSRV);
		ReleaseObject(mIntegrationMapTextureSRV);
	}

	#pragma region COMPONENT_METHODS
	/////////////////////////////////////////////////////////////
	// 'DemoLevel' ugly methods...
	bool TerrainDemo::IsComponent()
	{
		return mGame->IsInGameLevels<TerrainDemo*>(mGame->levels, this);
	}
	void TerrainDemo::Create()
	{
		Initialize();
		mGame->levels.push_back(this);
	}
	void TerrainDemo::Destroy()
	{
		this->~TerrainDemo();
		mGame->levels.clear();
	}
	/////////////////////////////////////////////////////////////  
#pragma endregion

	void TerrainDemo::Initialize()
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

		/**/
		////
		/**/

		// test sphere for foliage zones
		mRenderingObjects.insert(std::pair<std::string, RenderingObject*>("Sphere", new RenderingObject("Sphere", *mGame, *mCamera, std::unique_ptr<Model>(new Model(*mGame, Utility::GetFilePath("content\\models\\sphere_lowpoly.fbx"), true)), true, true)));
		mRenderingObjects["Sphere"]->LoadMaterial(new StandardLightingMaterial(), lightingEffect, MaterialHelper::lightingMaterialName);
		mRenderingObjects["Sphere"]->LoadMaterial(new DeferredMaterial(), effectDeferredPrepass, MaterialHelper::deferredPrepassMaterialName);
		mRenderingObjects["Sphere"]->LoadRenderBuffers();
		mRenderingObjects["Sphere"]->GetMaterials()[MaterialHelper::lightingMaterialName]->SetCurrentTechnique(mRenderingObjects["Sphere"]->GetMaterials()[MaterialHelper::lightingMaterialName]->GetEffect()->TechniquesByName().at("standard_lighting_pbr_instancing"));
		mRenderingObjects["Sphere"]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName]->SetCurrentTechnique(mRenderingObjects["Sphere"]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName]->GetEffect()->TechniquesByName().at("deferred_instanced"));
		mRenderingObjects["Sphere"]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::lightingMaterialName, [&](int meshIndex) { UpdateStandardLightingPBRMaterialVariables("Sphere", meshIndex); });
		mRenderingObjects["Sphere"]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::deferredPrepassMaterialName, [&](int meshIndex) { UpdateDeferredPrepassMaterialVariables("Sphere", meshIndex); });
		DistributeObjectAcrossTerrainGrid(mRenderingObjects["Sphere"], mFoliageZonesCount);
		mRenderingObjects["Sphere"]->LoadInstanceBuffers();

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
		
		mTerrain = new Terrain(Utility::GetFilePath("content\\terrain\\terrain"), *mGame, *mCamera, *mDirectionalLight, *mPostProcessingStack, false);

		mCamera->SetPosition(XMFLOAT3(0, 0.0f, 0.0f));
		mCamera->SetFarPlaneDistance(100000.0f);
		//mCamera->ApplyRotation(XMMatrixRotationAxis(mCamera->RightVector(), XMConvertToRadians(18.0f)) * XMMatrixRotationAxis(mCamera->UpVector(), -XMConvertToRadians(70.0f)));

		GenerateFoliageZones(mFoliageZonesCount);

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

	void TerrainDemo::GenerateFoliageZones(int count)
	{
		if (mFoliageZonesCenters.size() != count)
			throw GameException("Failed to foliage zones! Centers collections size is not equal to zones count");

		for (int i = 0; i < count; i++)
		{
			mFoliageZonesCollections.push_back(
				std::map<std::string, Foliage*>
				(
					{
						{ "content\\textures\\foliage\\grass_type1.png", new Foliage(*mGame, *mCamera, *mDirectionalLight, 12000, Utility::GetFilePath("content\\textures\\foliage\\grass_type1.png"), 2.5f, TERRAIN_TILE_RESOLUTION/2, mFoliageZonesCenters[i], FoliageBillboardType::TWO_QUADS_CROSSING)},
						{ "content\\textures\\foliage\\grass_type4.png", new Foliage(*mGame, *mCamera, *mDirectionalLight, 15000, Utility::GetFilePath("content\\textures\\foliage\\grass_type4.png"), 2.0f, TERRAIN_TILE_RESOLUTION / 2, mFoliageZonesCenters[i], FoliageBillboardType::THREE_QUADS_CROSSING) },
						{ "content\\textures\\foliage\\grass_flower_type1.png", new Foliage(*mGame, *mCamera, *mDirectionalLight, 1200, Utility::GetFilePath("content\\textures\\foliage\\grass_flower_type1.png"), 3.5f, TERRAIN_TILE_RESOLUTION / 2, mFoliageZonesCenters[i], FoliageBillboardType::SINGLE) },
						{ "content\\textures\\foliage\\grass_flower_type3.png", new Foliage(*mGame, *mCamera, *mDirectionalLight, 1000, Utility::GetFilePath("content\\textures\\foliage\\grass_flower_type3.png"), 2.5f, TERRAIN_TILE_RESOLUTION / 2, mFoliageZonesCenters[i], FoliageBillboardType::SINGLE) },
						{ "content\\textures\\foliage\\grass_flower_type10.png", new Foliage(*mGame, *mCamera, *mDirectionalLight, 800, Utility::GetFilePath("content\\textures\\foliage\\grass_flower_type10.png"), 3.5f, TERRAIN_TILE_RESOLUTION / 2, mFoliageZonesCenters[i], FoliageBillboardType::SINGLE) }
					}
				)
			);
		}
	}

	void TerrainDemo::UpdateLevel(const GameTime& gameTime)
	{
		UpdateImGui();

		mDirectionalLight->UpdateProxyModel(gameTime, mCamera->ViewMatrix4X4(), mCamera->ProjectionMatrix4X4());
		mSkybox->Update(gameTime);
		mGrid->Update(gameTime);
		mPostProcessingStack->Update();
		for (auto foliageZone : mFoliageZonesCollections)
		{
			for (auto foliageType : foliageZone)
			{
				foliageType.second->SetDynamicLODMaxDistance(mFoliageDynamicLODToCameraDistance);
				foliageType.second->Update(gameTime);
			}
		}

		mCamera->Cull(mRenderingObjects);
		mShadowMapper->Update(gameTime);

		mTerrain->SetWireframeMode(isWireframe);
		mTerrain->SetTessellationTerrainMode(isTessellationTerrain);
		mTerrain->SetNormalTerrainMode(isNormalTerrain);
		mTerrain->SetTessellationFactor(tessellationFactor);
		mTerrain->SetTessellationFactorDynamic(tessellationFactorDynamic);
		mTerrain->SetTerrainHeightScale(terrainHeightScale);
		mTerrain->SetDynamicTessellation(isDynamicTessellation);
		mTerrain->SetDynamicTessellationDistanceFactor(distanceFactor);

		for (auto object : mRenderingObjects)
			object.second->Update(gameTime);
	}

	void TerrainDemo::UpdateImGui()
	{

		ImGui::Begin("Terrain Demo Scene");

		ImGui::Checkbox("Show Post Processing Stack", &mPostProcessingStack->isWindowOpened);
		if (mPostProcessingStack->isWindowOpened) mPostProcessingStack->ShowPostProcessingWindow();

		ImGui::Separator();

		if (Utility::IsEditorMode)
		{
			ImGui::Begin("Scene Objects");

			const char* listbox_items[] = { /*"Ground Plane", */"Sphere" };

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

		ImGui::Checkbox("Render Wireframe", &isWireframe);
		ImGui::Checkbox("Render tessellated terrain", &isTessellationTerrain);
		ImGui::Checkbox("Render non-tessellated terrain", &isNormalTerrain);
		ImGui::SliderInt("Tessellation factor static", &tessellationFactor, 1, 64);
		ImGui::SliderInt("Tessellation factor dynamic", &tessellationFactorDynamic, 1, 64);
		ImGui::Checkbox("Use dynamic tessellation", &isDynamicTessellation);
		ImGui::SliderFloat("Dynamic LOD distance factor", &distanceFactor, 0.0001f, 0.1f);
		ImGui::SliderFloat("Tessellated terrain height scale", &terrainHeightScale, 0.0f, 1000.0f);
		ImGui::Checkbox("Render foliage", &mRenderFoliage);
		ImGui::SliderFloat("Foliage dynamic LOD max distance", &mFoliageDynamicLODToCameraDistance, 100.0f, 5000.0f);

		ImGui::End();
	}

	void TerrainDemo::DrawLevel(const GameTime& gameTime)
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

		//shadows
		for (int i = 0; i < MAX_NUM_OF_CASCADES; i++)
		{
			mShadowMapper->BeginRenderingToShadowMap(i);
			const std::string name = MaterialHelper::shadowMapMaterialName + " " + std::to_string(i);

			XMMATRIX lvp = mShadowMapper->GetViewMatrix(i) * mShadowMapper->GetProjectionMatrix(i);
			int objectIndex = 0;
			for (auto it = mRenderingObjects.begin(); it != mRenderingObjects.end(); it++, objectIndex++)
			{
				if (static_cast<DepthMapMaterial*>(it->second->GetMaterials()[name]))
				{
					static_cast<DepthMapMaterial*>(it->second->GetMaterials()[name])->LightViewProjection() << lvp;
					//static_cast<DepthMapMaterial*>(it->second->GetMaterials()[name])->AlbedoAlphaMap() << it->second->GetTextureData(objectIndex).AlbedoMap;
					it->second->Draw(name, true);
				}
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
		mTerrain->Draw();

		//grid
		//if (Utility::IsEditorMode)
		//	mGrid->Draw(gameTime);

		//gizmo
		if (Utility::IsEditorMode)
			mDirectionalLight->DrawProxyModel(gameTime);

		//lighting
		for (auto it = mRenderingObjects.begin(); it != mRenderingObjects.end(); it++)
			it->second->Draw(MaterialHelper::lightingMaterialName);

		//foliage
		if (mRenderFoliage) {
			for (auto foliageZone : mFoliageZonesCollections)
			{
				for (auto foliageType : foliageZone)
					foliageType.second->Draw();
			}
		}

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

	void TerrainDemo::UpdateStandardLightingPBRMaterialVariables(const std::string& objectName, int meshIndex)
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
	void TerrainDemo::UpdateDeferredPrepassMaterialVariables(const std::string & objectName, int meshIndex)
	{
		XMMATRIX worldMatrix = XMLoadFloat4x4(&(mRenderingObjects[objectName]->GetTransformationMatrix4X4()));
		XMMATRIX vp = /*worldMatrix * */mCamera->ViewMatrix() * mCamera->ProjectionMatrix();
		static_cast<DeferredMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName])->ViewProjection() << vp;
		static_cast<DeferredMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName])->World() << worldMatrix;
		static_cast<DeferredMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName])->AlbedoMap() << mRenderingObjects[objectName]->GetTextureData(meshIndex).AlbedoMap;
		static_cast<DeferredMaterial*>(mRenderingObjects[objectName]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName])->ReflectionMaskFactor() << mRenderingObjects[objectName]->GetMeshReflectionFactor(meshIndex);
	}
	void TerrainDemo::UpdateShadow0MaterialVariables(const std::string & objectName, int meshIndex)
	{
		const std::string name = MaterialHelper::shadowMapMaterialName + " " + std::to_string(0);
		static_cast<DepthMapMaterial*>(mRenderingObjects[objectName]->GetMaterials()[name])->AlbedoAlphaMap() << mRenderingObjects[objectName]->GetTextureData(meshIndex).AlbedoMap;
	}
	void TerrainDemo::UpdateShadow1MaterialVariables(const std::string & objectName, int meshIndex)
	{
		const std::string name = MaterialHelper::shadowMapMaterialName + " " + std::to_string(1);
		static_cast<DepthMapMaterial*>(mRenderingObjects[objectName]->GetMaterials()[name])->AlbedoAlphaMap() << mRenderingObjects[objectName]->GetTextureData(meshIndex).AlbedoMap;
	}
	void TerrainDemo::UpdateShadow2MaterialVariables(const std::string & objectName, int meshIndex)
	{
		const std::string name = MaterialHelper::shadowMapMaterialName + " " + std::to_string(2);
		static_cast<DepthMapMaterial*>(mRenderingObjects[objectName]->GetMaterials()[name])->AlbedoAlphaMap() << mRenderingObjects[objectName]->GetTextureData(meshIndex).AlbedoMap;
	}

	void TerrainDemo::DistributeObjectAcrossTerrainGrid(RenderingObject* object, int count)
	{
		if (count & (count - 1) != 0)
			throw GameException("Can't distribute objects across terrain grid! Number of objects is not a power of two");

		if (!object->IsInstanced())
			throw GameException("Can't distribute objects across terrain grid! Object has disabled instancing!");
		else
		{
			object->ResetInstanceData(count);

			float tileWidth = TERRAIN_TILE_RESOLUTION / 2.0f;
			for (int i = 0; i < sqrt(count); i++)
			{
				for (int j = 0; j < sqrt(count); j++)
				{
					float x = (float)((int)tileWidth * (i - 1) - tileWidth / 2);
					float y = 0.0f;
					float z = (float)((int)-tileWidth * j + tileWidth * 1.5f);

					float scale = 30.0f;
					mFoliageZonesCenters.push_back(XMFLOAT3(x, y, z));
					object->AddInstanceData(XMMatrixScaling(scale, scale, scale) *  XMMatrixTranslation(x, y, z));
				}
			}
		}
	}
}