#include "stdafx.h"

#include "TerrainDemo.h"
#include "..\Library\Game.h"
#include "..\Library\GameException.h"
#include "..\Library\VectorHelper.h"
#include "..\Library\MatrixHelper.h"
#include "..\Library\ColorHelper.h"
#include "..\Library\MaterialHelper.h"
#include "..\Library\Camera.h"
#include "..\Library\Editor.h"
#include "..\Library\Model.h"
#include "..\Library\Mesh.h"
#include "..\Library\Utility.h"
#include "..\Library\DirectionalLight.h"
#include "..\Library\Keyboard.h"
#include "..\Library\Light.h"
#include "..\Library\ER_Skybox.h"
#include "..\Library\Grid.h"
#include "..\Library\DemoLevel.h"
#include "..\Library\RenderStateHelper.h"
#include "..\Library\RenderingObject.h"
#include "..\Library\DepthMap.h"
#include "..\Library\Frustum.h"
#include "..\Library\StandardLightingMaterial.h"
#include "..\Library\ScreenSpaceReflectionsMaterial.h"
#include "..\Library\DepthMapMaterial.h"
#include "..\Library\ER_PostProcessingStack.h"
#include "..\Library\FullScreenRenderTarget.h"
#include "..\Library\IBLRadianceMap.h"
#include "..\Library\DeferredMaterial.h"
#include "..\Library\GBuffer.h"
#include "..\Library\FullScreenQuad.h"
#include "..\Library\ShadowMapper.h"
#include "..\Library\Terrain.h"
#include "..\Library\ER_Foliage.h"
#include "..\Library\Scene.h"
#include "..\Library\VolumetricClouds.h"
#include "..\Library\ShaderCompiler.h"

namespace Rendering
{
	RTTI_DEFINITIONS(TerrainDemo)

	static int selectedObjectIndex = -1;
	static std::string foliageZoneGizmoName = "Foliage Zone Gizmo Sphere";
	static std::string testSphereGizmoName = "Test Gizmo Sphere";

	TerrainDemo::TerrainDemo(Game& game, Camera& camera, Editor& editor)
		: DrawableGameComponent(game, camera, editor),
		mWorldMatrix(MatrixHelper::Identity),
		mRenderStateHelper(nullptr),
		mDirectionalLight(nullptr),
		mSkybox(nullptr),
		mIrradianceDiffuseTextureSRV(nullptr),
		mIrradianceSpecularTextureSRV(nullptr),
		mIntegrationMapTextureSRV(nullptr),
		mGrid(nullptr),
		mGBuffer(nullptr),
		mShadowMapper(nullptr),
		mTerrain(nullptr),
		mPostProcessingStack(nullptr),
		mScene(nullptr),
		mVolumetricClouds(nullptr)
	{
	}

	TerrainDemo::~TerrainDemo()
	{
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
		DeleteObject(mScene);
		DeleteObject(mVolumetricClouds);

		ReleaseObject(mIrradianceDiffuseTextureSRV);
		ReleaseObject(mIrradianceSpecularTextureSRV);
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

		mScene = new Scene(*mGame, *mCamera, Utility::GetFilePath("content\\levels\\terrainScene.json"));
		for (auto& object : mScene->objects) {
			object.second->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::forwardLightingMaterialName, [&](int meshIndex) { UpdateStandardLightingPBRMaterialVariables(object.first, meshIndex); });
			object.second->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::deferredPrepassMaterialName, [&](int meshIndex) { UpdateDeferredPrepassMaterialVariables(object.first, meshIndex); });
			object.second->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::shadowMapMaterialName + " " + std::to_string(0), [&](int meshIndex) { UpdateShadow0MaterialVariables(object.first, meshIndex); });
			object.second->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::shadowMapMaterialName + " " + std::to_string(1), [&](int meshIndex) { UpdateShadow1MaterialVariables(object.first, meshIndex); });
			object.second->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::shadowMapMaterialName + " " + std::to_string(2), [&](int meshIndex) { UpdateShadow2MaterialVariables(object.first, meshIndex); });
		}
		mEditor->LoadScene(mScene);

		mSkybox = new Skybox(*mGame, *mCamera, Utility::GetFilePath(Utility::ToWideString(mScene->skyboxPath)), 10000);
		mSkybox->Initialize();

		mGrid = new Grid(*mGame, *mCamera, 200, 56, XMFLOAT4(0.961f, 0.871f, 0.702f, 1.0f));
		mGrid->Initialize();
		mGrid->SetColor((XMFLOAT4)ColorHelper::LightGray);

		//directional light
		mDirectionalLight = new DirectionalLight(*mGame, *mCamera);
		mDirectionalLight->ApplyRotation(XMMatrixRotationAxis(mDirectionalLight->RightVector(), -XMConvertToRadians(70.0f)) * XMMatrixRotationAxis(mDirectionalLight->UpVector(), -XMConvertToRadians(25.0f)));
		mDirectionalLight->SetAmbientColor(mScene->ambientColor);
		mDirectionalLight->SetSunColor(mScene->sunColor);

		mShadowMapper = new ShadowMapper(*mGame, *mCamera, *mDirectionalLight, 4096, 4096);
		mDirectionalLight->RotationUpdateEvent->AddListener("shadow mapper", [&]() {mShadowMapper->ApplyTransform(); });

		mRenderStateHelper = new RenderStateHelper(*mGame);

		//PP
		mPostProcessingStack = new PostProcessingStack(*mGame, *mCamera);
		mPostProcessingStack->Initialize(false, false, true, true, true, false, true);
		mPostProcessingStack->SetDirectionalLight(mDirectionalLight);
		mPostProcessingStack->SetSunOcclusionSRV(mSkybox->GetSunOcclusionOutputTexture());

		mTerrain = new Terrain(Utility::GetFilePath("content\\terrain\\terrain"), *mGame, *mCamera, *mDirectionalLight, *mPostProcessingStack, false);

		mCamera->SetPosition(mScene->cameraPosition);
		mCamera->SetFarPlaneDistance(100000.0f);

		// place placable instanced objects on terrain
		DistributeVegetationZonesPositionsAcrossTerrainGrid(nullptr, mVegetationZonesCount);
		
		// place foliage on terrain
		GenerateFoliageInVegetationZones(mVegetationZonesCount);
		for (int i = 0; i < NUM_THREADS_PER_TERRAIN_SIDE * NUM_THREADS_PER_TERRAIN_SIDE; i++)
			PlaceFoliageOnTerrainTile(i);
		
		// place placeable instanced objects on terrain
		for (auto& object : mScene->objects)
			if (object.second->IsInstanced() && object.second->IsPlacedOnTerrain())
				GenerateObjectsInVegetationZones(mVegetationZonesCount, object.first);
		
		for (int i = 0; i < NUM_THREADS_PER_TERRAIN_SIDE * NUM_THREADS_PER_TERRAIN_SIDE; i++)
			for (auto& object : mScene->objects)
				if (object.second->IsPlacedOnTerrain() && object.second->IsInstanced())
					PlaceInstanceObjectOnTerrain(object.second, i, object.second->GetNumInstancesPerVegetationZone());

		//IBL
		if (FAILED(DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), Utility::GetFilePath(L"content\\textures\\skyboxes\\Sky_5\\textureDiffuseHDR.dds").c_str(), nullptr, &mIrradianceDiffuseTextureSRV)))
			throw GameException("Failed to create Diffuse Irradiance Map.");
		//if (FAILED(DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), Utility::GetFilePath(L"content\\textures\\skyboxes\\Sky_5\\textureSpecularHDR.dds").c_str(), nullptr, &mIrradianceSpecularTextureSRV)))
		//	throw GameException("Failed to create Specular Irradiance Map.");
		mIBLRadianceMap.reset(new IBLRadianceMap(*mGame, Utility::GetFilePath(L"content\\textures\\skyboxes\\Sky_5\\textureEnvHDR.dds")));
		mIBLRadianceMap->Initialize();
		mIBLRadianceMap->Create(*mGame);

		mIrradianceSpecularTextureSRV = *mIBLRadianceMap->GetShaderResourceViewAddress();
		if (mIrradianceSpecularTextureSRV == nullptr)
			throw GameException("Failed to create Specular Irradiance Map.");
		mIBLRadianceMap.release();
		mIBLRadianceMap.reset(nullptr);

		// Load a pre-computed Integration Map
		if (FAILED(DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), Utility::GetFilePath(L"content\\textures\\skyboxes\\Sky_4\\textureBrdf.dds").c_str(), nullptr, &mIntegrationMapTextureSRV)))
			throw GameException("Failed to create Integration Texture.");

		// volumetric clouds
		mVolumetricClouds = new VolumetricClouds(*mGame, *mCamera, *mDirectionalLight, *mPostProcessingStack, *mSkybox);
	}

	void TerrainDemo::GenerateFoliageInVegetationZones(int count)
	{
		if (mVegetationZonesCenters.size() != count)
			throw GameException("Failed to foliage zones! Centers collections size is not equal to zones count");

		for (int i = 0; i < count; i++)
		{
			mFoliageZonesCollections.push_back(
				std::map<std::string, Foliage*>
				(
					{
						{ "content\\textures\\foliage\\grass_type1.png",			new Foliage(*mGame, *mCamera, *mDirectionalLight, 15000, Utility::GetFilePath("content\\textures\\foliage\\grass_type1.png"), 2.0f, TERRAIN_TILE_RESOLUTION / 2, mVegetationZonesCenters[i], FoliageBillboardType::TWO_QUADS_CROSSING)},
						{ "content\\textures\\foliage\\grass_type4.png",			new Foliage(*mGame, *mCamera, *mDirectionalLight, 14000, Utility::GetFilePath("content\\textures\\foliage\\grass_type4.png"), 1.5f, TERRAIN_TILE_RESOLUTION / 2, mVegetationZonesCenters[i], FoliageBillboardType::TWO_QUADS_CROSSING) },
						{ "content\\textures\\foliage\\grass_type6.png",			new Foliage(*mGame, *mCamera, *mDirectionalLight, 20000, Utility::GetFilePath("content\\textures\\foliage\\grass_type6.png"), 1.5f, TERRAIN_TILE_RESOLUTION / 2, mVegetationZonesCenters[i], FoliageBillboardType::TWO_QUADS_CROSSING) },
						{ "content\\textures\\foliage\\grass_flower_type1.png",		new Foliage(*mGame, *mCamera, *mDirectionalLight, 500, Utility::GetFilePath("content\\textures\\foliage\\grass_flower_type1.png"), 3.5f, TERRAIN_TILE_RESOLUTION / 2, mVegetationZonesCenters[i], FoliageBillboardType::SINGLE) },
						{ "content\\textures\\foliage\\grass_flower_type3.png",		new Foliage(*mGame, *mCamera, *mDirectionalLight, 250, Utility::GetFilePath("content\\textures\\foliage\\grass_flower_type3.png"), 2.5f, TERRAIN_TILE_RESOLUTION / 2, mVegetationZonesCenters[i], FoliageBillboardType::SINGLE) },
						{ "content\\textures\\foliage\\grass_flower_type10.png",	new Foliage(*mGame, *mCamera, *mDirectionalLight, 250, Utility::GetFilePath("content\\textures\\foliage\\grass_flower_type10.png"), 3.5f, TERRAIN_TILE_RESOLUTION / 2, mVegetationZonesCenters[i], FoliageBillboardType::SINGLE) }
					}
				)
			);
		}
	}

	void TerrainDemo::GenerateObjectsInVegetationZones(int zonesCount, std::string nameInScene)
	{
		int instancesCount = mScene->objects.find(nameInScene)->second->GetNumInstancesPerVegetationZone();

		if (mVegetationZonesCenters.size() != zonesCount)
			throw GameException("Failed to foliage zones! Centers collections size is not equal to zones count");

		if (!mScene->objects.find(nameInScene)->second)
			return;

		//mScene->objects.find(nameInScene)->second->SetNumInstancesPerVegetationZone(instancesCount);
		mScene->objects.find(nameInScene)->second->ResetInstanceData(0, true);

		std::vector<InstancedData> data;
		for (int i = 0; i < (zonesCount); i++)
			DistributeAcrossVegetationZones(data, instancesCount, TERRAIN_TILE_RESOLUTION / 2, mVegetationZonesCenters[i],
				mScene->objects.find(nameInScene)->second->GetMinScale(), mScene->objects.find(nameInScene)->second->GetMaxScale());
		
		for (int i = 0; i < data.size(); i++)
			mScene->objects.find(nameInScene)->second->AddInstanceData(XMLoadFloat4x4(&data[i].World));

		mScene->objects.find(nameInScene)->second->UpdateInstanceBuffer(data);
	}

	void TerrainDemo::UpdateLevel(const GameTime& gameTime)
	{
		UpdateImGui();

		mDirectionalLight->UpdateProxyModel(gameTime, mCamera->ViewMatrix4X4(), mCamera->ProjectionMatrix4X4());
		mSkybox->SetUseCustomSkyColor(mEditor->IsSkyboxUsingCustomColor());
		mSkybox->SetSkyColors(mEditor->GetBottomSkyColor(), mEditor->GetTopSkyColor());
		mSkybox->SetSunData(mDirectionalLight->IsSunRendered(),
			mDirectionalLight->DirectionVector(),
			XMVECTOR{ mDirectionalLight->GetDirectionalLightColor().x, mDirectionalLight->GetDirectionalLightColor().y, mDirectionalLight->GetDirectionalLightColor().z, 1.0 },
			mDirectionalLight->GetSunBrightness(), mDirectionalLight->GetSunExponent());
		mSkybox->Update(gameTime);
		mGrid->Update(gameTime);
		mTerrain->Update(gameTime);
		mPostProcessingStack->Update();
		mVolumetricClouds->Update(gameTime);

		for (auto foliageZone : mFoliageZonesCollections)
		{
			for (auto foliageType : foliageZone)
			{
				foliageType.second->SetDynamicLODMaxDistance(mFoliageDynamicLODToCameraDistance);
				foliageType.second->SetWindParams(mWindGustDistance, mWindStrength, mWindFrequency);
				foliageType.second->Update(gameTime);
			}
		}

		mCamera->Cull(mScene->objects);
		mShadowMapper->Update(gameTime);

		for (auto object : mScene->objects)
			object.second->Update(gameTime);

		mEditor->Update(gameTime);
	}

	void TerrainDemo::UpdateImGui()
	{
		ImGui::Begin("Terrain Demo Scene");

		ImGui::Checkbox("Show Post Processing Stack", &mPostProcessingStack->isWindowOpened);
		//if (mPostProcessingStack->isWindowOpened) mPostProcessingStack->ShowPostProcessingWindow();

		ImGui::Separator();

		if (ImGui::Button("Terrain")) {
			mTerrain->Config();
		}
		if (ImGui::Button("Volumetric Clouds")) {
			mVolumetricClouds->Config();
		}

		ImGui::Checkbox("Render foliage", &mRenderFoliage);
		//ImGui::Checkbox("Render vegetation zones centers gizmos", &mRenderVegetationZonesCenters);
		ImGui::SliderFloat("Foliage dynamic LOD max distance", &mFoliageDynamicLODToCameraDistance, 100.0f, 5000.0f);
		ImGui::Separator();
		ImGui::SliderFloat("Wind strength", &mWindStrength, 0.0f, 100.0f);
		ImGui::SliderFloat("Wind gust distance", &mWindGustDistance, 0.0f, 100.0f);
		ImGui::SliderFloat("Wind frequency", &mWindFrequency, 0.0f, 100.0f);

		ImGui::End();
	}

	void TerrainDemo::DrawLevel(const GameTime& gameTime)
	{
		float clear_color[4] = { 0.0f, 1.0f, 1.0f, 0.0f };

		mRenderStateHelper->SaveRasterizerState();

		ID3D11DeviceContext* direct3DDeviceContext = mGame->Direct3DDeviceContext();
		direct3DDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		#pragma region GBUFFER_PREPASS

		mGBuffer->Start();
		for (auto it = mScene->objects.begin(); it != mScene->objects.end(); it++)
			it->second->Draw(MaterialHelper::deferredPrepassMaterialName, true);
		mGBuffer->End();

#pragma endregion

		#pragma region DRAW_SHADOWS
		mShadowMapper->Draw(mScene);
		mRenderStateHelper->RestoreRasterizerState();
#pragma endregion

		mPostProcessingStack->Begin(true);

		#pragma region DRAW_LIGHTING

		//skybox
		mSkybox->Draw();

		//terrain
		mTerrain->Draw(mShadowMapper);

		//grid
		//if (Utility::IsEditorMode)
		//	mGrid->Draw(gameTime);

		//gizmo
		if (Utility::IsEditorMode)
			mDirectionalLight->DrawProxyModel(gameTime);

		//lighting
		for (auto it = mScene->objects.begin(); it != mScene->objects.end(); it++)
			it->second->Draw(MaterialHelper::forwardLightingMaterialName);

		//foliage
		if (mRenderFoliage) {
			for (auto foliageZone : mFoliageZonesCollections)
			{
				for (auto foliageType : foliageZone)
					foliageType.second->Draw(gameTime, mShadowMapper);
			}
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
		mPostProcessingStack->UpdateSSRMaterial(mGBuffer->GetNormals()->GetSRV(), mGBuffer->GetDepth()->getSRV(), mGBuffer->GetExtraBuffer()->GetSRV(), (float)gameTime.TotalGameTime());
		mPostProcessingStack->DrawEffects(gameTime);
		mPostProcessingStack->ResetMainRTtoOriginal();
#pragma endregion
		
		mRenderStateHelper->SaveAll();

#pragma region DRAW_IMGUI
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

#pragma endregion
	}

	void TerrainDemo::UpdateStandardLightingPBRMaterialVariables(const std::string& objectName, int meshIndex)
	{
		XMMATRIX worldMatrix = XMLoadFloat4x4(&(mScene->objects[objectName]->GetTransformationMatrix4X4()));
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

		//static_cast<StandardLightingMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->ViewProjection() << vp;
		//static_cast<StandardLightingMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->World() << worldMatrix;
		//static_cast<StandardLightingMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->ShadowMatrices().SetMatrixArray(shadowMatrices, 0, NUM_SHADOW_CASCADES);
		//static_cast<StandardLightingMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->CameraPosition() << mCamera->PositionVector();
		//static_cast<StandardLightingMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->SunDirection() << XMVectorNegate(mDirectionalLight->DirectionVector());
		//static_cast<StandardLightingMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->SunColor() << XMVECTOR{ mDirectionalLight->GetDirectionalLightColor().x,  mDirectionalLight->GetDirectionalLightColor().y, mDirectionalLight->GetDirectionalLightColor().z , 1.0f };
		//static_cast<StandardLightingMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->AmbientColor() << XMVECTOR{ mDirectionalLight->GetAmbientLightColor().x,  mDirectionalLight->GetAmbientLightColor().y, mDirectionalLight->GetAmbientLightColor().z , 1.0f };
		//static_cast<StandardLightingMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->ShadowTexelSize() << XMVECTOR{ 1.0f / mShadowMapper->GetResolution(), 1.0f, 1.0f , 1.0f };
		//static_cast<StandardLightingMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->ShadowCascadeDistances() << XMVECTOR{ mCamera->GetCameraFarCascadeDistance(0), mCamera->GetCameraFarCascadeDistance(1), mCamera->GetCameraFarCascadeDistance(2), 1.0f };
		//static_cast<StandardLightingMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->AlbedoTexture() << mScene->objects[objectName]->GetTextureData(meshIndex).AlbedoMap;
		//static_cast<StandardLightingMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->NormalTexture() << mScene->objects[objectName]->GetTextureData(meshIndex).NormalMap;
		//static_cast<StandardLightingMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->SpecularTexture() << mScene->objects[objectName]->GetTextureData(meshIndex).SpecularMap;
		//static_cast<StandardLightingMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->RoughnessTexture() << mScene->objects[objectName]->GetTextureData(meshIndex).RoughnessMap;
		//static_cast<StandardLightingMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->MetallicTexture() << mScene->objects[objectName]->GetTextureData(meshIndex).MetallicMap;
		//static_cast<StandardLightingMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->CascadedShadowTextures().SetResourceArray(shadowMaps, 0, NUM_SHADOW_CASCADES);
		//static_cast<StandardLightingMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->IrradianceDiffuseTexture() << mIrradianceDiffuseTextureSRV;
		//static_cast<StandardLightingMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->IrradianceSpecularTexture() << mIrradianceSpecularTextureSRV;
		//static_cast<StandardLightingMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::forwardLightingMaterialName])->IntegrationTexture() << mIntegrationMapTextureSRV;
	}
	void TerrainDemo::UpdateDeferredPrepassMaterialVariables(const std::string & objectName, int meshIndex)
	{
		if (mScene->objects[objectName]->GetMaterials().count(MaterialHelper::deferredPrepassMaterialName) == 0 ||
			!static_cast<DeferredMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName]))
			return;

		XMMATRIX worldMatrix = XMLoadFloat4x4(&(mScene->objects[objectName]->GetTransformationMatrix4X4()));
		XMMATRIX vp = /*worldMatrix * */mCamera->ViewMatrix() * mCamera->ProjectionMatrix();
		static_cast<DeferredMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName])->ViewProjection() << vp;
		static_cast<DeferredMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName])->World() << worldMatrix;
		static_cast<DeferredMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName])->AlbedoMap() << mScene->objects[objectName]->GetTextureData(meshIndex).AlbedoMap;
		static_cast<DeferredMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName])->RoughnessMap() << mScene->objects[objectName]->GetTextureData(meshIndex).RoughnessMap;
		static_cast<DeferredMaterial*>(mScene->objects[objectName]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName])->ReflectionMaskFactor() << mScene->objects[objectName]->GetMeshReflectionFactor(meshIndex);
	}
	void TerrainDemo::UpdateShadow0MaterialVariables(const std::string & objectName, int meshIndex)
	{
		const std::string name = MaterialHelper::shadowMapMaterialName + " " + std::to_string(0);
		static_cast<DepthMapMaterial*>(mScene->objects[objectName]->GetMaterials()[name])->AlbedoAlphaMap() << mScene->objects[objectName]->GetTextureData(meshIndex).AlbedoMap;
	}
	void TerrainDemo::UpdateShadow1MaterialVariables(const std::string & objectName, int meshIndex)
	{
		const std::string name = MaterialHelper::shadowMapMaterialName + " " + std::to_string(1);
		static_cast<DepthMapMaterial*>(mScene->objects[objectName]->GetMaterials()[name])->AlbedoAlphaMap() << mScene->objects[objectName]->GetTextureData(meshIndex).AlbedoMap;
	}
	void TerrainDemo::UpdateShadow2MaterialVariables(const std::string & objectName, int meshIndex)
	{
		const std::string name = MaterialHelper::shadowMapMaterialName + " " + std::to_string(2);
		static_cast<DepthMapMaterial*>(mScene->objects[objectName]->GetMaterials()[name])->AlbedoAlphaMap() << mScene->objects[objectName]->GetTextureData(meshIndex).AlbedoMap;
	}

	void TerrainDemo::DistributeVegetationZonesPositionsAcrossTerrainGrid(RenderingObject* object, int count)
	{
		if (count & (count - 1) != 0)
			throw GameException("Can't distribute foliage zones across terrain grid! Number of objects is not a power of two");

		//if (!object->IsInstanced())
		//	throw GameException("Can't distribute foliage zones across terrain grid! Object has disabled instancing!");
		//else
		{
			//object->ResetInstanceData(count);

			float tileWidth = NUM_THREADS_PER_TERRAIN_SIDE * TERRAIN_TILE_RESOLUTION / sqrt(count);
			for (int i = 0; i < sqrt(count); i++)
			{
				for (int j = 0; j < sqrt(count); j++)
				{
					float x = (float)((int)tileWidth * i - TERRAIN_TILE_RESOLUTION + tileWidth/2);
					float z = (float)((int)-tileWidth * j + TERRAIN_TILE_RESOLUTION - tileWidth/2);
					int heightMapIndex = (int)(i / (sqrt(count) / NUM_THREADS_PER_TERRAIN_SIDE)) * (NUM_THREADS_PER_TERRAIN_SIDE) + (int)(j / (sqrt(count) / NUM_THREADS_PER_TERRAIN_SIDE));
					float y = mTerrain->GetHeightmap(heightMapIndex)->FindHeightFromPosition(x, z);

					mVegetationZonesCenters.push_back(XMFLOAT3(x, y, z));
					//object->AddInstanceData(XMMatrixScaling(mFoliageZoneGizmoSphereScale, mFoliageZoneGizmoSphereScale, mFoliageZoneGizmoSphereScale) *  XMMatrixTranslation(x, y, z));
				}
			}
		}
	}
	void TerrainDemo::DistributeAcrossVegetationZones(std::vector<InstancedData>& data, int count, float radius, XMFLOAT3 center, float minScale, float maxScale)
	{
		for (int i = 0; i < count; i++)
		{
			float x = center.x + ((float)rand() / (float)(RAND_MAX)) * radius - radius / 2;
			float y = 0.0f;
			float z = center.z + ((float)rand() / (float)(RAND_MAX)) * radius - radius / 2;

			float scale = Utility::RandomFloat(minScale, maxScale);
			float angle = Utility::RandomFloat(0.0f, 360.0f);

			data.push_back(XMMatrixRotationY(XMConvertToRadians(angle)) * XMMatrixScaling(scale, scale, scale) * XMMatrixTranslation(x, y, z));
		}
	}

	void TerrainDemo::PlaceFoliageOnTerrainTile(int tileIndex)
	{
		if (mVegetationZonesCenters.size() == 0)
			throw GameException("Failed to load foliage zones when placing foliage on terrain! No zones!");

		// prepare before GPU dispatch on compute shader
		std::vector<XMFLOAT4> foliagePatchesPositions;
		int maxSizeFoliagePatches = 0;
		int count = mFoliageZonesCollections.size();

		for (int i = 0; i < sqrt(count); i++)
		{
			for (int j = 0; j < sqrt(count); j++)
			{
				int heightMapIndex = (int)(i / (sqrt(count) / NUM_THREADS_PER_TERRAIN_SIDE)) * (NUM_THREADS_PER_TERRAIN_SIDE)+(int)(j / (sqrt(count) / NUM_THREADS_PER_TERRAIN_SIDE));
				if (heightMapIndex == tileIndex) {
					for (auto foliageType : mFoliageZonesCollections[i * sqrt(count) + j]) {
						maxSizeFoliagePatches += foliageType.second->GetPatchesCount();
						for (int patchIndex = 0; patchIndex < foliageType.second->GetPatchesCount(); patchIndex++)
						{
							foliagePatchesPositions.push_back(XMFLOAT4(foliageType.second->GetPatchPositionX(patchIndex), 0.0f, foliageType.second->GetPatchPositionZ(patchIndex), 1.0f));
						}
					}
				}
			}
		}
		
		std::vector<XMFLOAT4> terrainVertices;
		int maxSizeTerrainVertices = mTerrain->GetHeightmap(tileIndex)->mVertexCount;
		for (int j = 0; j < maxSizeTerrainVertices; j++)
			terrainVertices.push_back(XMFLOAT4(mTerrain->GetHeightmap(tileIndex)->mVertexList[j].x, mTerrain->GetHeightmap(tileIndex)->mVertexList[j].y, mTerrain->GetHeightmap(tileIndex)->mVertexList[j].z, 1.0f));

		// compute shader pass
		PlaceObjectsOnTerrain(tileIndex, &foliagePatchesPositions[0], maxSizeFoliagePatches, &terrainVertices[0], maxSizeTerrainVertices, TerrainSplatChannels::GRASS);

		// read back to foliage
		int offset = 0;
		for (int i = 0; i < sqrt(count); i++)
		{
			for (int j = 0; j < sqrt(count); j++)
			{
				int heightMapIndex = (int)(i / (sqrt(count) / NUM_THREADS_PER_TERRAIN_SIDE)) * (NUM_THREADS_PER_TERRAIN_SIDE)+(int)(j / (sqrt(count) / NUM_THREADS_PER_TERRAIN_SIDE));
				if (heightMapIndex == tileIndex) {
					for (auto foliageType : mFoliageZonesCollections[i * sqrt(count) + j]) 
					{
						for (int patchIndex = 0; patchIndex < foliageType.second->GetPatchesCount(); patchIndex++)
						{
							foliageType.second->SetPatchPosition(patchIndex,
								foliageType.second->GetPatchPositionX(patchIndex),
								foliagePatchesPositions[offset + patchIndex].y,
								foliageType.second->GetPatchPositionZ(patchIndex));
						}
						offset += mFoliageZonesCollections[i * sqrt(count) + j].at(foliageType.first)->GetPatchesCount();
						foliageType.second->CreateBufferGPU();
					}
				}
			}
		}
	}

	void TerrainDemo::PlaceInstanceObjectOnTerrain(RenderingObject* object, int tileIndex, int numInstancesPerZone, int splatChannel)
	{
		if (/*object->GetIsSavedOnTerrain() ||*/ numInstancesPerZone == 0)
			return;

		if (mVegetationZonesCenters.size() * numInstancesPerZone != object->GetInstanceCount())
			throw GameException("Failed to place object instances on terrain! Number of objects' instances != num zones * num instances per zone");

		// prepare before GPU dispatch on compute shader
		std::vector<XMFLOAT4> positions;
		int count = numInstancesPerZone;
		int zonesCount = mVegetationZonesCenters.size();
		int bufferOffset = 0;
		for (int i = 0; i < sqrt(zonesCount); i++)
		{
			for (int j = 0; j < sqrt(zonesCount); j++)
			{
				int heightMapIndex = (int)(i / (sqrt(zonesCount) / NUM_THREADS_PER_TERRAIN_SIDE)) * (NUM_THREADS_PER_TERRAIN_SIDE)+(int)(j / (sqrt(zonesCount) / NUM_THREADS_PER_TERRAIN_SIDE));
				
				if (heightMapIndex == tileIndex) {
					for (int k = 0; k < count; k++) {
						XMFLOAT3 translation;
						MatrixHelper::GetTranslation(XMLoadFloat4x4(&(object->GetInstancesData()[k  + (i * sqrt(zonesCount) + j) * numInstancesPerZone].World)), translation);
						positions.push_back(XMFLOAT4(translation.x, 0.0f, translation.z, 1.0f));
					}
					bufferOffset += numInstancesPerZone;
				}
			}
		}

		if (positions.empty())
			return;

		std::vector<XMFLOAT4> terrainVertices;
		int maxSizeTerrainVertices = mTerrain->GetHeightmap(tileIndex)->mVertexCount;
		for (int j = 0; j < maxSizeTerrainVertices; j++)
			terrainVertices.push_back(XMFLOAT4(mTerrain->GetHeightmap(tileIndex)->mVertexList[j].x, mTerrain->GetHeightmap(tileIndex)->mVertexList[j].y, mTerrain->GetHeightmap(tileIndex)->mVertexList[j].z, 1.0f));
		
		// compute shader pass
		int totalCount = count * zonesCount / (NUM_THREADS_PER_TERRAIN_SIDE * NUM_THREADS_PER_TERRAIN_SIDE);
		PlaceObjectsOnTerrain(tileIndex, &positions[0], totalCount, &terrainVertices[0], maxSizeTerrainVertices, splatChannel);
		//object->SetSavedOnTerrain(true);

		// read back to instance data
		bufferOffset = 0;
		for (int i = 0; i < sqrt(zonesCount); i++)
		{
			for (int j = 0; j < sqrt(zonesCount); j++)
			{
				int heightMapIndex = (int)(i / (sqrt(zonesCount) / NUM_THREADS_PER_TERRAIN_SIDE)) * (NUM_THREADS_PER_TERRAIN_SIDE)+(int)(j / (sqrt(zonesCount) / NUM_THREADS_PER_TERRAIN_SIDE));
				if (heightMapIndex == tileIndex) {
					for (int posIndex = 0; posIndex < numInstancesPerZone; posIndex++) {
						if (positions[posIndex].y != -999.0f) {
							object->GetInstancesData()[(i * sqrt(zonesCount) + j) * numInstancesPerZone + posIndex].World._41 = positions[posIndex + bufferOffset].x;
							object->GetInstancesData()[(i * sqrt(zonesCount) + j) * numInstancesPerZone + posIndex].World._42 = positions[posIndex + bufferOffset].y;
							object->GetInstancesData()[(i * sqrt(zonesCount) + j) * numInstancesPerZone + posIndex].World._43 = positions[posIndex + bufferOffset].z;
						}
					}
					bufferOffset += numInstancesPerZone;
				}
			}
		}

		object->UpdateInstanceBuffer(object->GetInstancesData());
	}

	// generic method for displacing object positions by height of the terrain (GPU calculation)
	void TerrainDemo::PlaceObjectsOnTerrain(int tileIndex, XMFLOAT4* objectsPositions, int objectsCount, XMFLOAT4* terrainVertices, int terrainVertexCount, int splatChannel)
	{
		ID3D11DeviceContext* context = mGame->Direct3DDeviceContext();
		UINT initCounts = 0;

		ID3DBlob* placeObjectsOnTerrainCS = NULL;
		ID3D11ComputeShader* computeShader = NULL;

		if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Terrain\\PlaceObjectsOnTerrain.hlsl").c_str(), "displaceOnTerrainCS", "cs_5_0", &placeObjectsOnTerrainCS)))
			throw GameException("Failed to load a shader: Compute Shader from PlaceObjectsOnTerrain.hlsl!");
		if (FAILED(mGame->Direct3DDevice()->CreateComputeShader(placeObjectsOnTerrainCS->GetBufferPointer(), placeObjectsOnTerrainCS->GetBufferSize(), NULL, &computeShader)))
			throw GameException("Failed to create shader from PlaceObjectsOnTerrain.hlsl!");
		
		ReleaseObject(placeObjectsOnTerrainCS);

		// cbuffer
		ID3D11Buffer* cBuffer = NULL;
		float consts[] = 
		{ 
			mTerrain->GetHeightmap(tileIndex)->mUVOffsetToTextureSpace.x,
			mTerrain->GetHeightmap(tileIndex)->mUVOffsetToTextureSpace.y,
			mTerrain->GetHeightScale(true),
			static_cast<float>(splatChannel)
		};
		D3D11_SUBRESOURCE_DATA init_cb0 = { &consts[0], 0, 0 };
		D3D11_BUFFER_DESC cb_desc;
		cb_desc.Usage = D3D11_USAGE_IMMUTABLE;
		cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cb_desc.CPUAccessFlags = 0;
		cb_desc.MiscFlags = 0;
		cb_desc.ByteWidth = PAD16(sizeof(consts));

		if (FAILED(mGame->Direct3DDevice()->CreateBuffer(&cb_desc, &init_cb0, &cBuffer)))
			throw GameException("Failed to create cbuffer in PlaceObjectsOnTerrain call.");

		// terrain vertex buffer
		ID3D11ShaderResourceView* terrainBufferSRV = NULL;
		ID3D11Buffer* terrainBuffer = NULL;
		D3D11_SUBRESOURCE_DATA data = { terrainVertices, 0, 0 };

		D3D11_BUFFER_DESC buf_descTerrain;
		buf_descTerrain.ByteWidth = sizeof(XMFLOAT4) * terrainVertexCount;
		buf_descTerrain.Usage = D3D11_USAGE_DEFAULT;
		buf_descTerrain.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		buf_descTerrain.CPUAccessFlags = 0;
		buf_descTerrain.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		buf_descTerrain.StructureByteStride = sizeof(XMFLOAT4);
		if (FAILED(mGame->Direct3DDevice()->CreateBuffer(&buf_descTerrain, terrainVertices != NULL ? &data : NULL, &terrainBuffer)))
			throw GameException("Failed to create terrain vertices buffer in PlaceObjectsOnTerrain call. Maybe increase TDR of your graphics driver...");

		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
		srv_desc.Format = DXGI_FORMAT_UNKNOWN;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srv_desc.Buffer.FirstElement = 0;
		srv_desc.Buffer.NumElements = terrainVertexCount;
		if (FAILED(mGame->Direct3DDevice()->CreateShaderResourceView(terrainBuffer, &srv_desc, &terrainBufferSRV)))
			throw GameException("Failed to create terrain vertices SRV buffer in PlaceObjectsOnTerrain call.");

		// positions buffers
		ID3D11Buffer* posBuffer = NULL;
		ID3D11Buffer* outputPosBuffer = NULL;
		ID3D11UnorderedAccessView* posUAV = NULL;
		D3D11_SUBRESOURCE_DATA init_data = { objectsPositions, 0, 0 };

		D3D11_BUFFER_DESC buf_desc;
		buf_desc.ByteWidth = sizeof(XMFLOAT4) * objectsCount;
		buf_desc.Usage = D3D11_USAGE_DEFAULT;
		buf_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		buf_desc.CPUAccessFlags = 0;
		buf_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		buf_desc.StructureByteStride = sizeof(XMFLOAT4);
		if (FAILED(mGame->Direct3DDevice()->CreateBuffer(&buf_desc, objectsPositions != NULL ? &init_data : NULL, &posBuffer)))
			throw GameException("Failed to create objects positions buffer in PlaceObjectsOnTerrain call.");

		// uav for positions
		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
		uav_desc.Format = DXGI_FORMAT_UNKNOWN;
		uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uav_desc.Buffer.FirstElement = 0;
		uav_desc.Buffer.NumElements = objectsCount;// sizeof(objectsPositions) / sizeof(XMFLOAT4);
		uav_desc.Buffer.Flags = 0;
		if (FAILED(mGame->Direct3DDevice()->CreateUnorderedAccessView(posBuffer, &uav_desc, &posUAV)))
			throw GameException("Failed to create UAV of objects positions buffer in PlaceObjectsOnTerrain call.");

		// create the ouput buffer for storing data from GPU for positions
		buf_desc.Usage = D3D11_USAGE_STAGING;
		buf_desc.BindFlags = 0;
		buf_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		if (FAILED(mGame->Direct3DDevice()->CreateBuffer(&buf_desc, 0, &outputPosBuffer)))
			throw GameException("Failed to create objects positions output buffer in PlaceObjectsOnTerrain call.");

		// run
		context->CSSetShader(computeShader, NULL, 0);
		context->CSSetConstantBuffers(0, 1, &cBuffer);
		context->CSSetShaderResources(0, 1, &terrainBufferSRV);
		context->CSSetShaderResources(1, 1, &(mTerrain->GetHeightmap(tileIndex)->mHeightTexture));
		context->CSSetShaderResources(2, 1, &(mTerrain->GetHeightmap(tileIndex)->mSplatTexture));
		context->CSSetUnorderedAccessViews(0, 1, &posUAV, &initCounts);
		context->Dispatch(512, 1, 1);

		// read results
		context->CopyResource(outputPosBuffer, posBuffer);
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		HRESULT hr = context->Map(outputPosBuffer, 0, D3D11_MAP_READ, 0, &mappedResource);

		if (SUCCEEDED(hr))
		{
			XMFLOAT4* positions = reinterpret_cast<XMFLOAT4*>(mappedResource.pData);
			for (size_t i = 0; i < objectsCount; i++)
				objectsPositions[i] = positions[i];
		}
		else
			throw GameException("Failed to read objects positions from GPU in output buffer in PlaceObjectsOnTerrain call.");
		
		context->Unmap(outputPosBuffer, 0);

		// Unbind resources for CS
		ID3D11UnorderedAccessView* UAViewNULL[1] = { NULL };
		context->CSSetUnorderedAccessViews(0, 1, UAViewNULL, &initCounts);
		ID3D11ShaderResourceView* SRVNULL[1] = { NULL };
		context->CSSetShaderResources(0, 1, SRVNULL);
		context->CSSetShaderResources(1, 1, SRVNULL);
		context->CSSetShaderResources(2, 1, SRVNULL);
		ID3D11Buffer* CBNULL[1] = { NULL };
		context->CSSetConstantBuffers(0, 1, CBNULL);

		ReleaseObject(computeShader);
		ReleaseObject(cBuffer);
		ReleaseObject(posBuffer);
		ReleaseObject(outputPosBuffer);
		ReleaseObject(posUAV);
		ReleaseObject(terrainBuffer);
		ReleaseObject(terrainBufferSRV);
	}
}