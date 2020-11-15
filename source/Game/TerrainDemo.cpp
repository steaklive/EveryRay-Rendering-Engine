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
#include "..\Library\ShaderCompiler.h"

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
	static std::string foliageZoneGizmoName = "Foliage Zone Gizmo Sphere";
	static std::string testSphereGizmoName = "Test Gizmo Sphere";

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

		mCamera->SetPosition(XMFLOAT3(0, 130.0f, 0.0f));
		mCamera->SetFarPlaneDistance(100000.0f);
		//mCamera->ApplyRotation(XMMatrixRotationAxis(mCamera->RightVector(), XMConvertToRadians(18.0f)) * XMMatrixRotationAxis(mCamera->UpVector(), -XMConvertToRadians(70.0f)));

		// test sphere for foliage zones
		mRenderingObjects.insert(std::pair<std::string, RenderingObject*>(foliageZoneGizmoName, new RenderingObject(foliageZoneGizmoName, *mGame, *mCamera, std::unique_ptr<Model>(new Model(*mGame, Utility::GetFilePath("content\\models\\sphere_lowpoly.fbx"), true)), true, true)));
		mRenderingObjects[foliageZoneGizmoName]->LoadMaterial(new StandardLightingMaterial(), lightingEffect, MaterialHelper::lightingMaterialName);
		mRenderingObjects[foliageZoneGizmoName]->LoadMaterial(new DeferredMaterial(), effectDeferredPrepass, MaterialHelper::deferredPrepassMaterialName);
		mRenderingObjects[foliageZoneGizmoName]->LoadRenderBuffers();
		mRenderingObjects[foliageZoneGizmoName]->GetMaterials()[MaterialHelper::lightingMaterialName]->SetCurrentTechnique(mRenderingObjects[foliageZoneGizmoName]->GetMaterials()[MaterialHelper::lightingMaterialName]->GetEffect()->TechniquesByName().at("standard_lighting_pbr_instancing"));
		mRenderingObjects[foliageZoneGizmoName]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName]->SetCurrentTechnique(mRenderingObjects[foliageZoneGizmoName]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName]->GetEffect()->TechniquesByName().at("deferred_instanced"));
		mRenderingObjects[foliageZoneGizmoName]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::lightingMaterialName, [&](int meshIndex) { UpdateStandardLightingPBRMaterialVariables(foliageZoneGizmoName, meshIndex); });
		mRenderingObjects[foliageZoneGizmoName]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::deferredPrepassMaterialName, [&](int meshIndex) { UpdateDeferredPrepassMaterialVariables(foliageZoneGizmoName, meshIndex); });
		DistributeFoliageZonesAcrossTerrainGrid(mRenderingObjects[foliageZoneGizmoName], mFoliageZonesCount);
		mRenderingObjects[foliageZoneGizmoName]->LoadInstanceBuffers();

		GenerateFoliageZones(mFoliageZonesCount);
		for (int i = 0; i < NUM_THREADS_PER_TERRAIN_SIDE * NUM_THREADS_PER_TERRAIN_SIDE; i++)
			PlaceFoliageOnTerrainTile(i);

		//mRenderingObjects.insert(std::pair<std::string, RenderingObject*>(testSphereGizmoName, new RenderingObject(testSphereGizmoName, *mGame, *mCamera, std::unique_ptr<Model>(new Model(*mGame, Utility::GetFilePath("content\\models\\sphere_lowpoly.fbx"), true)), true, true)));
		//mRenderingObjects[testSphereGizmoName]->LoadMaterial(new StandardLightingMaterial(), lightingEffect, MaterialHelper::lightingMaterialName);
		//mRenderingObjects[testSphereGizmoName]->LoadMaterial(new DeferredMaterial(), effectDeferredPrepass, MaterialHelper::deferredPrepassMaterialName);
		//mRenderingObjects[testSphereGizmoName]->LoadRenderBuffers();
		//mRenderingObjects[testSphereGizmoName]->GetMaterials()[MaterialHelper::lightingMaterialName]->SetCurrentTechnique(mRenderingObjects[testSphereGizmoName]->GetMaterials()[MaterialHelper::lightingMaterialName]->GetEffect()->TechniquesByName().at("standard_lighting_pbr_instancing"));
		//mRenderingObjects[testSphereGizmoName]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName]->SetCurrentTechnique(mRenderingObjects[testSphereGizmoName]->GetMaterials()[MaterialHelper::deferredPrepassMaterialName]->GetEffect()->TechniquesByName().at("deferred_instanced"));
		//mRenderingObjects[testSphereGizmoName]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::lightingMaterialName, [&](int meshIndex) { UpdateStandardLightingPBRMaterialVariables(testSphereGizmoName, meshIndex); });
		//mRenderingObjects[testSphereGizmoName]->MeshMaterialVariablesUpdateEvent->AddListener(MaterialHelper::deferredPrepassMaterialName, [&](int meshIndex) { UpdateDeferredPrepassMaterialVariables(testSphereGizmoName, meshIndex); });
		//DistributeAcrossTerrainGrid(mRenderingObjects[testSphereGizmoName], 100);
		//mRenderingObjects[testSphereGizmoName]->LoadInstanceBuffers();

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
						{ "content\\textures\\foliage\\grass_type1.png",			new Foliage(*mGame, *mCamera, *mDirectionalLight, 15000, Utility::GetFilePath("content\\textures\\foliage\\grass_type1.png"), 2.5f, TERRAIN_TILE_RESOLUTION / 2, mFoliageZonesCenters[i], FoliageBillboardType::TWO_QUADS_CROSSING)},
						{ "content\\textures\\foliage\\grass_type4.png",			new Foliage(*mGame, *mCamera, *mDirectionalLight, 15000, Utility::GetFilePath("content\\textures\\foliage\\grass_type4.png"), 2.0f, TERRAIN_TILE_RESOLUTION / 2, mFoliageZonesCenters[i], FoliageBillboardType::THREE_QUADS_CROSSING) },
						{ "content\\textures\\foliage\\grass_flower_type1.png",		new Foliage(*mGame, *mCamera, *mDirectionalLight, 800, Utility::GetFilePath("content\\textures\\foliage\\grass_flower_type1.png"), 3.5f, TERRAIN_TILE_RESOLUTION / 2, mFoliageZonesCenters[i], FoliageBillboardType::SINGLE) },
						{ "content\\textures\\foliage\\grass_flower_type3.png",		new Foliage(*mGame, *mCamera, *mDirectionalLight, 250, Utility::GetFilePath("content\\textures\\foliage\\grass_flower_type3.png"), 2.5f, TERRAIN_TILE_RESOLUTION / 2, mFoliageZonesCenters[i], FoliageBillboardType::SINGLE) },
						{ "content\\textures\\foliage\\grass_flower_type10.png",	new Foliage(*mGame, *mCamera, *mDirectionalLight, 250, Utility::GetFilePath("content\\textures\\foliage\\grass_flower_type10.png"), 3.5f, TERRAIN_TILE_RESOLUTION / 2, mFoliageZonesCenters[i], FoliageBillboardType::SINGLE) }
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
				foliageType.second->SetWindParams(mWindGustDistance, mWindStrength, mWindFrequency);
				foliageType.second->Update(gameTime);
			}
		}

		mCamera->Cull(mRenderingObjects);
		mShadowMapper->Update(gameTime);

		mTerrain->SetWireframeMode(mRenderTerrainWireframe);
		mTerrain->SetTessellationTerrainMode(mRenderTessellatedTerrain);
		mTerrain->SetNormalTerrainMode(mRenderNonTessellatedTerrain);
		mTerrain->SetTessellationFactor(mStaticTessellationFactor);
		mTerrain->SetTessellationFactorDynamic(mTessellationFactorDynamic);
		mTerrain->SetTerrainHeightScale(mTessellatedTerrainHeightScale);
		mTerrain->SetDynamicTessellation(mDynamicTessellation);
		mTerrain->SetDynamicTessellationDistanceFactor(mCameraDistanceFactor);

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

			const char* listbox_items[] = { /*"Ground Plane", */foliageZoneGizmoName.c_str() };

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

		ImGui::Checkbox("Render terrain wireframe", &mRenderTerrainWireframe);
		ImGui::Checkbox("Render non-tessellated terrain", &mRenderNonTessellatedTerrain);
		ImGui::Checkbox("Render tessellated terrain", &mRenderTessellatedTerrain);
		ImGui::SliderInt("Tessellation factor static", &mStaticTessellationFactor, 1, 64);
		ImGui::SliderInt("Tessellation factor dynamic", &mTessellationFactorDynamic, 1, 64);
		ImGui::Checkbox("Use dynamic tessellation", &mDynamicTessellation);
		ImGui::SliderFloat("Dynamic LOD distance factor", &mCameraDistanceFactor, 0.0001f, 0.1f);
		ImGui::SliderFloat("Tessellated terrain height scale", &mTessellatedTerrainHeightScale, 0.0f, 1000.0f);
		ImGui::Separator();
		ImGui::Checkbox("Render foliage", &mRenderFoliage);
		ImGui::Checkbox("Render foliage zones centers gizmos", &mRenderFoliageZonesCenters);
		ImGui::SliderFloat("Foliage dynamic LOD max distance", &mFoliageDynamicLODToCameraDistance, 100.0f, 5000.0f);
		ImGui::Separator();
		ImGui::SliderFloat("Wind strength", &mWindStrength, 0.0f, 100.0f);
		ImGui::SliderFloat("Wind gust distance", &mWindGustDistance, 0.0f, 100.0f);
		ImGui::SliderFloat("Wind frequency", &mWindFrequency, 0.0f, 100.0f);

		mRenderingObjects[foliageZoneGizmoName]->Visible(mRenderFoliageZonesCenters);


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
					foliageType.second->Draw(gameTime);
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

	void TerrainDemo::DistributeFoliageZonesAcrossTerrainGrid(RenderingObject* object, int count)
	{
		if (count & (count - 1) != 0)
			throw GameException("Can't distribute foliage zones across terrain grid! Number of objects is not a power of two");

		if (!object->IsInstanced())
			throw GameException("Can't distribute foliage zones across terrain grid! Object has disabled instancing!");
		else
		{
			object->ResetInstanceData(count);

			float tileWidth = NUM_THREADS_PER_TERRAIN_SIDE * TERRAIN_TILE_RESOLUTION / sqrt(count);
			for (int i = 0; i < sqrt(count); i++)
			{
				for (int j = 0; j < sqrt(count); j++)
				{
					float x = (float)((int)tileWidth * i - TERRAIN_TILE_RESOLUTION + tileWidth/2);
					float z = (float)((int)-tileWidth * j + TERRAIN_TILE_RESOLUTION - tileWidth/2);
					int heightMapIndex = (int)(i / (sqrt(count) / NUM_THREADS_PER_TERRAIN_SIDE)) * (NUM_THREADS_PER_TERRAIN_SIDE) + (int)(j / (sqrt(count) / NUM_THREADS_PER_TERRAIN_SIDE));
					float y = mTerrain->GetHeightmap(heightMapIndex)->FindHeightFromPosition(x, z);

					mFoliageZonesCenters.push_back(XMFLOAT3(x, y, z));
					object->AddInstanceData(XMMatrixScaling(mFoliageZoneGizmoSphereScale, mFoliageZoneGizmoSphereScale, mFoliageZoneGizmoSphereScale) *  XMMatrixTranslation(x, y, z));
				}
			}
		}
	}
	void TerrainDemo::DistributeAcrossTerrainGrid(RenderingObject* object, int count)
	{
		object->ResetInstanceData(count);
		float mDistributionRadius = 150;

		for (int i = 0; i < count; i++)
		{
			float x = mFoliageZonesCenters[0].x + ((float)rand() / (float)(RAND_MAX)) * mDistributionRadius - mDistributionRadius / 2;
			float z = mFoliageZonesCenters[0].z + ((float)rand() / (float)(RAND_MAX)) * mDistributionRadius - mDistributionRadius / 2;
			//float heightMapIndex = (int)(i / (sqrt(count) / NUM_THREADS)) * (NUM_THREADS)+(int)(j / (sqrt(count) / NUM_THREADS));
			float y = mTerrain->GetHeightmap(0)->FindHeightFromPosition(x, z);
			object->AddInstanceData(XMMatrixScaling(5.0f, 5.0f, 5.0f) *  XMMatrixTranslation(x, y, z));
		}
	}

	void TerrainDemo::PlaceFoliageOnTerrainTile(int tileIndex)
	{
		if (mFoliageZonesCenters.size() == 0)
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
		PlaceObjectsOnTerrain(&foliagePatchesPositions[0], maxSizeFoliagePatches, &terrainVertices[0], maxSizeTerrainVertices);

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
						foliageType.second->UpdateBufferGPU();
					}
				}
			}
		}
	}

	// generic method for displacing object positions by height of the terrain (GPU calculation)
	void TerrainDemo::PlaceObjectsOnTerrain(XMFLOAT4* objectsPositions, int objectsCount, XMFLOAT4* terrainVertices, int terrainVertexCount)
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

		// terrain vertex buffer
		ID3D11ShaderResourceView* terrainBufferSRV;
		ID3D11Buffer* terrainBuffer;
		D3D11_SUBRESOURCE_DATA data = { terrainVertices, 0, 0 };

		D3D11_BUFFER_DESC buf_descTerrain;
		buf_descTerrain.ByteWidth = sizeof(XMFLOAT4) * terrainVertexCount;
		buf_descTerrain.Usage = D3D11_USAGE_DEFAULT;
		buf_descTerrain.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		buf_descTerrain.CPUAccessFlags = 0;
		buf_descTerrain.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		buf_descTerrain.StructureByteStride = sizeof(XMFLOAT4);
		if (FAILED(mGame->Direct3DDevice()->CreateBuffer(&buf_descTerrain, terrainVertices != NULL ? &data : NULL, &terrainBuffer)))
			throw GameException("Failed to create terrain vertices buffer in PlaceObjectsOnTerrain call.");

		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
		srv_desc.Format = DXGI_FORMAT_UNKNOWN;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srv_desc.Buffer.FirstElement = 0;
		srv_desc.Buffer.NumElements = terrainVertexCount;
		if (FAILED(mGame->Direct3DDevice()->CreateShaderResourceView(terrainBuffer, &srv_desc, &terrainBufferSRV)))
			throw GameException("Failed to create terrain vertices SRV buffer in PlaceObjectsOnTerrain call.");

		// positions buffers
		ID3D11Buffer* posBuffer;
		ID3D11Buffer* outputPosBuffer;
		ID3D11UnorderedAccessView* posUAV;
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
		context->CSSetShaderResources(0, 1, &terrainBufferSRV);
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

		ReleaseObject(computeShader);
		ReleaseObject(posBuffer);
		ReleaseObject(outputPosBuffer);
		ReleaseObject(posUAV);
		ReleaseObject(terrainBuffer);
		ReleaseObject(terrainBufferSRV);
	}
}