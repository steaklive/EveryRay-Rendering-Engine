#define NOMINMAX
#include "stdafx.h"

#include "CollisionTestDemo.h"
#include "..\Library\InstancingMaterial.h"
#include "..\Library\AmbientLightingMaterial.h"
#include "..\Library\Game.h"
#include "..\Library\GameException.h"
#include "..\Library\MatrixHelper.h"
#include "..\Library\ColorHelper.h"
#include "..\Library\Camera.h"
#include "..\Library\Model.h"
#include "..\Library\Mesh.h"
#include "..\Library\Utility.h"
#include "..\Library\DirectionalLight.h"
#include "..\Library\Keyboard.h"
#include "..\Library\Light.h"
#include "..\Library\PointLight.h"
#include "..\Library\RenderStateHelper.h"
#include "..\Library\VectorHelper.h"
#include "..\Library\DemoLevel.h"
#include "..\Library\ProxyModel.h"
#include "..\Library\RenderableFrustum.h"
#include "..\Library\RenderableAABB.h"
#include "..\Library\RenderableOBB.h"
#include "..\Library\Skybox.h"
#include "..\Library\OctreeStructure.h"
#include "..\Library\BruteforceStructure.h"
#include "..\Library\SpatialElement.h"
#include "..\Library\RenderingObject.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include "DDSTextureLoader.h"
#include <WICTextureLoader.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <sstream>

const int NUM_DYNAMIC_INSTANCES = 300;
const int NUM_STATIC_INSTANCES = 25;

namespace Rendering
{
	RTTI_DEFINITIONS(CollisionTestDemo)


	CollisionTestDemoLightInfo::LightData currentLightSource;

	CollisionTestDemo::CollisionTestDemo(Game& game, Camera& camera)
		: DrawableGameComponent(game, camera),

		mKeyboard(nullptr),
		mSkybox(nullptr),
		mVolumeDebugAABB(nullptr),
		mOctreeStructure(nullptr), mBruteforceStructure(nullptr),
		mDynamicInstancedObject(nullptr), mStaticInstancedObject(nullptr),
		mSpatialElements(NUM_DYNAMIC_INSTANCES + NUM_STATIC_INSTANCES, nullptr),

		mWorldMatrix(MatrixHelper::Identity),
		mRenderStateHelper(game)
	{
	}

	CollisionTestDemo::~CollisionTestDemo()
	{
		DeleteObject(mVolumeDebugAABB);
		DeleteObject(mDynamicInstancedObject);
		DeleteObject(mStaticInstancedObject);
		DeleteObject(mOctreeStructure);
		DeleteObject(mBruteforceStructure);
		DeleteObject(mSkybox);
		DeletePointerCollection(mSpatialElements);

		currentLightSource.Destroy();
	}

	#pragma region COMPONENT_METHODS
	/////////////////////////////////////////////////////////////
	// 'DemoLevel' ugly methods...
	bool CollisionTestDemo::IsComponent()
	{
		return mGame->IsInGameComponents<CollisionTestDemo*>(mGame->components, this);
	}
	void CollisionTestDemo::Create()
	{
		Initialize();
		mGame->components.push_back(this);
	}
	void CollisionTestDemo::Destroy()
	{
		std::pair<bool, int> res = mGame->FindInGameComponents<CollisionTestDemo*>(mGame->components, this);

		if (res.first)
		{
			mGame->components.erase(mGame->components.begin() + res.second);

			//very provocative...
			delete this;
		}

	}
	/////////////////////////////////////////////////////////////  
	#pragma endregion


	void CollisionTestDemo::Initialize()
	{
		SetCurrentDirectory(Utility::ExecutableDirectory().c_str());

		// Load the instaning instancingEffect shader
		Effect* instancingEffect = new Effect(*mGame);
		instancingEffect->CompileFromFile(Utility::GetFilePath(L"content\\effects\\Instancing.fx"));
		
		mDynamicConvexHullObject = new RenderingObject("Bunny Convex Hull", *mGame, *mCamera, std::unique_ptr<Model>(new Model(*mGame, Utility::GetFilePath("content\\models\\bunny\\bunny_convexhull.fbx"), true)), false, true);

		#pragma region DYNAMIC_OBJECT_INITIALIZATION
	
		mDynamicInstancedObject = new RenderingObject("Bunny Dynamic", *mGame, *mCamera, std::unique_ptr<Model>(new Model(*mGame, Utility::GetFilePath("content\\models\\bunny\\bunny.fbx"), true)), false, true);
		mDynamicInstancedObject->LoadMaterial(new InstancingMaterial(), instancingEffect, "instancing");
		mDynamicInstancedObject->LoadRenderBuffers();
		for (size_t i = 0; i < mDynamicInstancedObject->GetMeshCount(); i++)
			mDynamicInstancedObject->LoadCustomMeshTextures(i,
				Utility::GetFilePath(L"content\\textures\\emptyDiffuseMap.png"),
				Utility::GetFilePath(L"content\\textures\\emptyNormalMap.jpg"),
				Utility::GetFilePath(L""),
				Utility::GetFilePath(L""),
				Utility::GetFilePath(L""),
				Utility::GetFilePath(L""),
				Utility::GetFilePath(L""),
				Utility::GetFilePath(L"")
			);

		float dynObjRad = CalculateObjectRadius(mDynamicInstancedObject->GetVertices());
		for (size_t i = 0; i < NUM_DYNAMIC_INSTANCES; i++)
		{
			mDynamicMeshAABBs.push_back(mDynamicInstancedObject->GetLocalAABB());
			mDynamicMeshOBBs.push_back(mDynamicInstancedObject->GetLocalAABB());
			mSpatialElements[i] = new SpatialElement(i);
			mSpatialElements[i]->SetRadius(dynObjRad);
		}

		// generate dynamic objects' random positions within a volume
		CalculateDynamicObjectsRandomDistribution();
		mDynamicInstancedObject->LoadInstanceBuffers(mDynamicObjectInstanceData, "instancing");
		mDynamicInstancedObject->MeshMaterialVariablesUpdateEvent->AddListener("Instancing Material Dynamic Object Update", [&](int meshIndex) { UpdateInstancingMaterialVariables(*mDynamicInstancedObject, meshIndex); });

		#pragma endregion

		#pragma region STATIC_OBJECT_INITIALIZATION

		mStaticInstancedObject = new RenderingObject("Bunny Dynamic", *mGame, *mCamera, std::unique_ptr<Model>(new Model(*mGame, Utility::GetFilePath("content\\models\\sphere_lowpoly.fbx"), true)), false, true);
		mStaticInstancedObject->LoadMaterial(new InstancingMaterial(), instancingEffect, "instancing");
		mStaticInstancedObject->LoadRenderBuffers();
		for (size_t i = 0; i < mStaticInstancedObject->GetMeshCount(); i++)
			mStaticInstancedObject->LoadCustomMeshTextures(i,
				Utility::GetFilePath(L"content\\textures\\emptyDiffuseMap2.png"),
				Utility::GetFilePath(L"content\\textures\\emptyNormalMap.jpg"),
				Utility::GetFilePath(L""),
				Utility::GetFilePath(L""),
				Utility::GetFilePath(L""),
				Utility::GetFilePath(L""),
				Utility::GetFilePath(L""),
				Utility::GetFilePath(L"")
			);

		float staticObjRad = CalculateObjectRadius(mStaticInstancedObject->GetVertices());
		for (size_t i = 0; i < NUM_STATIC_INSTANCES; i++)
		{
			mStaticMeshAABBs.push_back(mStaticInstancedObject->GetLocalAABB());
			mStaticMeshOBBs.push_back(mStaticInstancedObject->GetLocalAABB());
			mSpatialElements[i + NUM_DYNAMIC_INSTANCES] = new SpatialElement(i + NUM_DYNAMIC_INSTANCES);
			mSpatialElements[i + NUM_DYNAMIC_INSTANCES]->SetRadius(staticObjRad);
		}

		// generate static objects' random positions within a volume
		CalculateStaticObjectsRandomDistribution();
		mStaticInstancedObject->LoadInstanceBuffers(mStaticObjectInstanceData, "instancing");
		mStaticInstancedObject->MeshMaterialVariablesUpdateEvent->AddListener("Instancing Material Static Object Update", [&](int meshIndex) { UpdateInstancingMaterialVariables(*mStaticInstancedObject, meshIndex); });

#pragma endregion

		//initialize octree
		mOctreeStructure = new OctreeStructure(XMFLOAT3(0.0f, 0.0f, 0.0f), 25.0f);
		mOctreeStructure->AddObjects(mSpatialElements);
		mOctreeStructure->GetRootNode()->InitializeCellAABBs(*mGame, *mCamera);
		
		//initialize bruteforce
		mBruteforceStructure = new BruteforceStructure();
		mBruteforceStructure->AddObjects(mSpatialElements);

		// Directional Light initiazlization
		currentLightSource.DirectionalLight = new DirectionalLight(*mGame);
		currentLightSource.ProxyModel = new ProxyModel(*mGame, *mCamera, Utility::GetFilePath("content\\models\\DirectionalLightProxy.obj"), 0.5f);
		currentLightSource.ProxyModel->Initialize();
		currentLightSource.ProxyModel->SetPosition(0.0f, 30.0, 100.0f);

		// volume initialization
		std::vector<XMFLOAT3> volumeAABB;
		volumeAABB.push_back(XMFLOAT3(25.0f, 25.0f, 25.0f));
		volumeAABB.push_back(XMFLOAT3(-25.0f, -25.0f, -25.0f));

		mVolumeDebugAABB = new RenderableAABB(*mGame, *mCamera);
		mVolumeDebugAABB->Initialize();
		mVolumeDebugAABB->InitializeGeometry(volumeAABB, XMMatrixScaling(1, 1, 1));
		mVolumeDebugAABB->SetPosition(XMFLOAT3(0, 0.0f, 0));

		mKeyboard = (Keyboard*)mGame->Services().GetService(Keyboard::TypeIdClass());
		assert(mKeyboard != nullptr);

		//Skybox initialization
		mSkybox = new Skybox(*mGame, *mCamera, Utility::GetFilePath(L"content\\textures\\Sky_Type_1.dds"), 100.0f);
		mSkybox->Initialize();
	}

	// Update methods
	void CollisionTestDemo::Update(const GameTime& gameTime)
	{
		UpdateDirectionalLight(gameTime);

		mVolumeDebugAABB->Update();

		if (mIsDynamic)
		{
			std::for_each(mSpatialElements.begin(), mSpatialElements.end(), [&](SpatialElement* a)
			{
				a->OBB->isColliding = false;
				a->AABB->isColliding = false;
			});

			auto startTimerTransformUpdate = std::chrono::high_resolution_clock::now();
			
			if (mIsTransformOptimized)
				UpdateObjectsTransforms_OptimizedDataAccess(gameTime);
			else
				UpdateObjectsTransforms_UnoptimizedDataAccess(gameTime);

			auto endTimerTransformUpdate = std::chrono::high_resolution_clock::now();
			mElapsedTimeTransformUpdate = endTimerTransformUpdate - startTimerTransformUpdate;

		}

		#pragma region UPDATE_SPATIAL_ELEMENT
		
		auto startTimerElementUpdate = std::chrono::high_resolution_clock::now();

		UpdateSpatialElement(mSpatialElements, 0, NUM_DYNAMIC_INSTANCES + NUM_STATIC_INSTANCES, gameTime);

		auto endTimerElementUpdate = std::chrono::high_resolution_clock::now();
		mElapsedTimeElementUpdate = endTimerElementUpdate - startTimerElementUpdate;

		#pragma endregion
	
		#pragma region UPDATE_SPATIAL_STRUCTURE
		auto startTimerStructureUpdate = std::chrono::high_resolution_clock::now();
		// update spatial optimization structure
		switch (mSpatialHierarchyNumber)
		{
		case 0:
			mBruteforceStructure->Update();
			break;
		case 1:
			mOctreeStructure->Update();
			mOctreeStructure->GetRootNode()->UpdateCellsAABBs(gameTime);
			break;
		}
		auto endTimerStructureUpdate = std::chrono::high_resolution_clock::now();
		mElapsedTimeStructureUpdate = endTimerStructureUpdate - startTimerStructureUpdate;
		#pragma endregion

		VisualizeCollisionDetection();

		currentLightSource.ProxyModel->Update(gameTime);
		mSkybox->Update(gameTime);
		UpdateImGui();
	}

	void CollisionTestDemo::UpdateInstancingMaterialVariables(RenderingObject& object, int meshIndex)
	{
		XMVECTOR empty = { 0,0,0,0 };

		static_cast<InstancingMaterial*>(object.GetMaterials()["instancing"])->ViewProjection()				<< mCamera->ViewMatrix() * mCamera->ProjectionMatrix();
		static_cast<InstancingMaterial*>(object.GetMaterials()["instancing"])->LightColor0()				<< empty;
		static_cast<InstancingMaterial*>(object.GetMaterials()["instancing"])->LightPosition0()				<< empty;
		static_cast<InstancingMaterial*>(object.GetMaterials()["instancing"])->LightColor1()				<< empty;
		static_cast<InstancingMaterial*>(object.GetMaterials()["instancing"])->LightPosition1()				<< empty;
		static_cast<InstancingMaterial*>(object.GetMaterials()["instancing"])->LightColor2()				<< empty;
		static_cast<InstancingMaterial*>(object.GetMaterials()["instancing"])->LightPosition2()				<< empty;
		static_cast<InstancingMaterial*>(object.GetMaterials()["instancing"])->LightColor3()				<< empty;
		static_cast<InstancingMaterial*>(object.GetMaterials()["instancing"])->LightPosition3()				<< empty;
		static_cast<InstancingMaterial*>(object.GetMaterials()["instancing"])->LightRadius0()				<< (float)0;
		static_cast<InstancingMaterial*>(object.GetMaterials()["instancing"])->LightRadius1()				<< (float)0;
		static_cast<InstancingMaterial*>(object.GetMaterials()["instancing"])->LightRadius2()				<< (float)0;
		static_cast<InstancingMaterial*>(object.GetMaterials()["instancing"])->LightRadius3()				<< (float)0;
		static_cast<InstancingMaterial*>(object.GetMaterials()["instancing"])->LightDirection()				<< currentLightSource.DirectionalLight->DirectionVector();
		static_cast<InstancingMaterial*>(object.GetMaterials()["instancing"])->DirectionalLightColor()		<< currentLightSource.DirectionalLight->ColorVector();
		static_cast<InstancingMaterial*>(object.GetMaterials()["instancing"])->ColorTexture()				<< object.GetTextureData(meshIndex).AlbedoMap;
		static_cast<InstancingMaterial*>(object.GetMaterials()["instancing"])->NormalTexture()				<< object.GetTextureData(meshIndex).NormalMap;
		static_cast<InstancingMaterial*>(object.GetMaterials()["instancing"])->CameraPosition()				<< mCamera->PositionVector();

	}

	void CollisionTestDemo::UpdateDirectionalLight(const GameTime& gameTime)
	{
		float elapsedTime = (float)gameTime.ElapsedGameTime();

		const XMFLOAT2 LightRotationRate = XMFLOAT2(XM_PI / 2, XM_PI / 2);

		XMFLOAT2 rotationAmount = Vector2Helper::Zero;
		if (mKeyboard->IsKeyDown(DIK_LEFTARROW))
		{
			rotationAmount.x += LightRotationRate.x * elapsedTime;
		}
		if (mKeyboard->IsKeyDown(DIK_RIGHTARROW))
		{
			rotationAmount.x -= LightRotationRate.x * elapsedTime;
		}
		if (mKeyboard->IsKeyDown(DIK_UPARROW))
		{
			rotationAmount.y += 0.3f*LightRotationRate.y * elapsedTime;
		}
		if (mKeyboard->IsKeyDown(DIK_DOWNARROW))
		{
			rotationAmount.y -= 0.3f*LightRotationRate.y * elapsedTime;
		}

		//rotation matrix for light + light gizmo
		XMMATRIX lightRotationMatrix = XMMatrixIdentity();
		if (rotationAmount.x != 0)
		{
			lightRotationMatrix = XMMatrixRotationY(rotationAmount.x);
		}
		if (rotationAmount.y != 0)
		{
			XMMATRIX lightRotationAxisMatrix = XMMatrixRotationAxis(currentLightSource.DirectionalLight->DirectionVector(), rotationAmount.y);
			lightRotationMatrix *= lightRotationAxisMatrix;
		}
		if (rotationAmount.x != 0.0f || rotationAmount.y != 0.0f)
		{
			currentLightSource.DirectionalLight->ApplyRotation(lightRotationMatrix);
			currentLightSource.ProxyModel->ApplyRotation(lightRotationMatrix);
		}

	}
	void CollisionTestDemo::UpdateObjectsTransforms_UnoptimizedDataAccess(const GameTime& gameTime)
	{
		for (size_t i = 0; i < NUM_DYNAMIC_INSTANCES; i++)
		{

			#pragma region WALL_COLLISION

			if (mDynamicObjectInstancesPositions.at(i).x + mDynamicInstancedObject->GetLocalAABB().at(0).x <= -25.0f)
			{
				// Left wall
				XMVECTOR normal = { 1.0f, 0.0f, 0.0f };

				XMVECTOR currentDirection = XMLoadFloat3(&mDynamicObjectInstancesDirections.at(i));
				XMVECTOR newDirection = currentDirection - XMVECTOR{ 2.0f, 2.0f, 2.0f } *normal * XMVector3Dot(currentDirection, normal);

				XMStoreFloat3(&mDynamicObjectInstancesDirections.at(i), newDirection);
			}

			if (mDynamicObjectInstancesPositions.at(i).x + mDynamicInstancedObject->GetLocalAABB().at(1).x >= 25.0f)
			{
				// Right wall
				XMVECTOR normal = { -1.0f, 0.0f, 0.0f };

				XMVECTOR currentDirection = XMLoadFloat3(&mDynamicObjectInstancesDirections.at(i));
				XMVECTOR newDirection = currentDirection - XMVECTOR{ 2.0f, 2.0f, 2.0f } *normal * XMVector3Dot(currentDirection, normal);

				XMStoreFloat3(&mDynamicObjectInstancesDirections.at(i), newDirection);
			}

			if (mDynamicObjectInstancesPositions.at(i).z + mDynamicInstancedObject->GetLocalAABB().at(0).z <= -25.0f)
			{
				// Front Wall

				XMVECTOR normal = { 0.0f, 0.0f, 1.0f };

				XMVECTOR currentDirection = XMLoadFloat3(&mDynamicObjectInstancesDirections.at(i));
				XMVECTOR newDirection = currentDirection - XMVECTOR{ 2.0f, 2.0f, 2.0f } *normal * XMVector3Dot(currentDirection, normal);

				XMStoreFloat3(&mDynamicObjectInstancesDirections.at(i), newDirection);
			}

			if (mDynamicObjectInstancesPositions.at(i).z + mDynamicInstancedObject->GetLocalAABB().at(1).z >= 25.0f)
			{
				// Back wall
				XMVECTOR normal = { 0.0f, 0.0f, -1.0f };

				XMVECTOR currentDirection = XMLoadFloat3(&mDynamicObjectInstancesDirections.at(i));
				XMVECTOR newDirection = currentDirection - XMVECTOR{ 2.0f, 2.0f, 2.0f } *normal * XMVector3Dot(currentDirection, normal);

				XMStoreFloat3(&mDynamicObjectInstancesDirections.at(i), newDirection);
			}

			if (mDynamicObjectInstancesPositions.at(i).y + mDynamicInstancedObject->GetLocalAABB().at(0).y <= -25.0f)
			{
				// Bottom wall
				XMVECTOR normal = { 0.0f, 1.0f, 0.0f };

				XMVECTOR currentDirection = XMLoadFloat3(&mDynamicObjectInstancesDirections.at(i));
				XMVECTOR newDirection = currentDirection - XMVECTOR{ 2.0f, 2.0f, 2.0f } *normal * XMVector3Dot(currentDirection, normal);

				XMStoreFloat3(&mDynamicObjectInstancesDirections.at(i), newDirection);
			}

			if (mDynamicObjectInstancesPositions.at(i).y + mDynamicInstancedObject->GetLocalAABB().at(1).y >= 25.0f)
			{
				// Top wall
				XMVECTOR normal = { 0.0f, -1.0f, 0.0f };

				XMVECTOR currentDirection = XMLoadFloat3(&mDynamicObjectInstancesDirections.at(i));
				XMVECTOR newDirection = currentDirection - XMVECTOR{ 2.0f, 2.0f, 2.0f } *normal * XMVector3Dot(currentDirection, normal);

				XMStoreFloat3(&mDynamicObjectInstancesDirections.at(i), newDirection);
			}

			#pragma endregion

			// update positions
			mDynamicObjectInstancesPositions.at(i).x += mDynamicObjectInstancesDirections.at(i).x * mSpeed;
			mDynamicObjectInstancesPositions.at(i).y += mDynamicObjectInstancesDirections.at(i).y * mSpeed;
			mDynamicObjectInstancesPositions.at(i).z += mDynamicObjectInstancesDirections.at(i).z * mSpeed;

			// update rotations
			XMMATRIX rotation = XMMatrixRotationY(gameTime.TotalGameTime());
			mDynamicObjectInstanceData.at(i) = InstancingMaterial::InstancedData
			(
				rotation *

				XMMatrixTranslation
				(
					mDynamicObjectInstancesPositions.at(i).x,
					mDynamicObjectInstancesPositions.at(i).y,
					mDynamicObjectInstancesPositions.at(i).z
				)

			);

			// update positions of spatial objects
			mSpatialElements.at(i)->SetPosition(mDynamicObjectInstancesPositions.at(i));

			if (mCollisionMethodNumber == 0)
			{
				// update AABBs gizmos positions
				mSpatialElements.at(i)->AABB->SetPosition(
					XMFLOAT3
					(
						mDynamicObjectInstancesPositions.at(i).x,
						mDynamicObjectInstancesPositions.at(i).y,
						mDynamicObjectInstancesPositions.at(i).z
					)
				);

				// recalculate AABB for every instance
				if (mRecalculateAABBs)
				{
					auto startTimerRecalculateAABB = std::chrono::high_resolution_clock::now();

					RecalculateAABB(i, *mDynamicInstancedObject, rotation);
					mSpatialElements.at(i)->AABB->SetAABB(mDynamicMeshAABBs.at(i));

					auto endTimerRecalculateAABB = std::chrono::high_resolution_clock::now();
					mElapsedTimeRecalculateAABBUpdate = endTimerRecalculateAABB - startTimerRecalculateAABB;

				}
			}
			else if (mCollisionMethodNumber == 1)
			{
				// update OBBs gizmos positions
				mSpatialElements[i]->OBB->SetPosition(
					XMFLOAT3
					(
						mDynamicObjectInstancesPositions.at(i).x,
						mDynamicObjectInstancesPositions.at(i).y,
						mDynamicObjectInstancesPositions.at(i).z
					)
				);

				// set rotations for OBBs
				mSpatialElements.at(i)->OBB->SetRotationMatrix(rotation);
			}
		}

		mDynamicInstancedObject->UpdateInstanceData(mDynamicObjectInstanceData, "instancing");
	}
	void CollisionTestDemo::UpdateObjectsTransforms_OptimizedDataAccess(const GameTime& gameTime)
	{
		for (size_t i = 0; i < NUM_DYNAMIC_INSTANCES; i++)
		{

			#pragma region WALL_COLLISION

			if (mDynamicObjectInstancesPositions[i].x + mDynamicInstancedObject->GetLocalAABB().at(0).x <= -25.0f)
			{
				// Left wall
				XMVECTOR normal = { 1.0f, 0.0f, 0.0f };

				XMVECTOR currentDirection = XMLoadFloat3(&mDynamicObjectInstancesDirections[i]);
				XMVECTOR newDirection = currentDirection - XMVECTOR{ 2.0f, 2.0f, 2.0f } *normal * XMVector3Dot(currentDirection, normal);

				XMStoreFloat3(&mDynamicObjectInstancesDirections[i], newDirection);
			}

			if (mDynamicObjectInstancesPositions[i].x + mDynamicInstancedObject->GetLocalAABB().at(1).x >= 25.0f)
			{
				// Right wall
				XMVECTOR normal = { -1.0f, 0.0f, 0.0f };

				XMVECTOR currentDirection = XMLoadFloat3(&mDynamicObjectInstancesDirections[i]);
				XMVECTOR newDirection = currentDirection - XMVECTOR{ 2.0f, 2.0f, 2.0f } *normal * XMVector3Dot(currentDirection, normal);

				XMStoreFloat3(&mDynamicObjectInstancesDirections[i], newDirection);
			}

			if (mDynamicObjectInstancesPositions[i].z + mDynamicInstancedObject->GetLocalAABB().at(0).z <= -25.0f)
			{
				// Front Wall

				XMVECTOR normal = { 0.0f, 0.0f, 1.0f };

				XMVECTOR currentDirection = XMLoadFloat3(&mDynamicObjectInstancesDirections[i]);
				XMVECTOR newDirection = currentDirection - XMVECTOR{ 2.0f, 2.0f, 2.0f } *normal * XMVector3Dot(currentDirection, normal);

				XMStoreFloat3(&mDynamicObjectInstancesDirections[i], newDirection);
			}

			if (mDynamicObjectInstancesPositions[i].z + mDynamicInstancedObject->GetLocalAABB().at(1).z >= 25.0f)
			{
				// Back wall
				XMVECTOR normal = { 0.0f, 0.0f, -1.0f };

				XMVECTOR currentDirection = XMLoadFloat3(&mDynamicObjectInstancesDirections[i]);
				XMVECTOR newDirection = currentDirection - XMVECTOR{ 2.0f, 2.0f, 2.0f } *normal * XMVector3Dot(currentDirection, normal);

				XMStoreFloat3(&mDynamicObjectInstancesDirections[i], newDirection);
			}

			if (mDynamicObjectInstancesPositions[i].y + mDynamicInstancedObject->GetLocalAABB().at(0).y <= -25.0f)
			{
				// Bottom wall
				XMVECTOR normal = { 0.0f, 1.0f, 0.0f };

				XMVECTOR currentDirection = XMLoadFloat3(&mDynamicObjectInstancesDirections[i]);
				XMVECTOR newDirection = currentDirection - XMVECTOR{ 2.0f, 2.0f, 2.0f } *normal * XMVector3Dot(currentDirection, normal);

				XMStoreFloat3(&mDynamicObjectInstancesDirections[i], newDirection);
			}

			if (mDynamicObjectInstancesPositions[i].y + mDynamicInstancedObject->GetLocalAABB().at(1).y >= 25.0f)
			{
				// Top wall
				XMVECTOR normal = { 0.0f, -1.0f, 0.0f };

				XMVECTOR currentDirection = XMLoadFloat3(&mDynamicObjectInstancesDirections[i]);
				XMVECTOR newDirection = currentDirection - XMVECTOR{ 2.0f, 2.0f, 2.0f } *normal * XMVector3Dot(currentDirection, normal);

				XMStoreFloat3(&mDynamicObjectInstancesDirections[i], newDirection);
			}

			#pragma endregion

			// update positions
			mDynamicObjectInstancesPositions[i].x += mDynamicObjectInstancesDirections[i].x * mSpeed;
			mDynamicObjectInstancesPositions[i].y += mDynamicObjectInstancesDirections[i].y * mSpeed;
			mDynamicObjectInstancesPositions[i].z += mDynamicObjectInstancesDirections[i].z * mSpeed;

			// update rotations
			XMMATRIX rotation = XMMatrixRotationY(gameTime.TotalGameTime());
			mDynamicObjectInstanceData[i] = InstancingMaterial::InstancedData
			(
				rotation *

				XMMatrixTranslation
				(
					mDynamicObjectInstancesPositions[i].x,
					mDynamicObjectInstancesPositions[i].y,
					mDynamicObjectInstancesPositions[i].z
				)

			);

			// update positions of spatial objects
			mSpatialElements[i]->SetPosition(mDynamicObjectInstancesPositions[i]);

			if (mCollisionMethodNumber == 0)
			{
				// update AABBs gizmos positions
				mSpatialElements[i]->AABB->SetPosition(
					XMFLOAT3
					(
						mDynamicObjectInstancesPositions[i].x,
						mDynamicObjectInstancesPositions[i].y,
						mDynamicObjectInstancesPositions[i].z
					)
				);

				// recalculate AABB for every instance
				if (mRecalculateAABBs)
				{
					auto startTimerRecalculateAABB = std::chrono::high_resolution_clock::now();

					RecalculateAABB(i, *mDynamicInstancedObject, rotation);
					mSpatialElements[i]->AABB->SetAABB(mDynamicMeshAABBs[i]);

					auto endTimerRecalculateAABB = std::chrono::high_resolution_clock::now();
					mElapsedTimeRecalculateAABBUpdate = endTimerRecalculateAABB - startTimerRecalculateAABB;

				}
			}
			else if (mCollisionMethodNumber == 1)
			{
				// update OBBs gizmos positions
				mSpatialElements[i]->OBB->SetPosition(
					XMFLOAT3
					(
						mDynamicObjectInstancesPositions[i].x,
						mDynamicObjectInstancesPositions[i].y,
						mDynamicObjectInstancesPositions[i].z
					)
				);

				// set rotations for OBBs
				mSpatialElements[i]->OBB->SetRotationMatrix(rotation);
			}
		}

		mDynamicInstancedObject->UpdateInstanceData(mDynamicObjectInstanceData, "instancing");
	}
	void CollisionTestDemo::UpdateSpatialElement(std::vector<SpatialElement*>& elements, int startIndex, int endIndex, const GameTime& gameTime)
	{
		for	(int i = startIndex; i<endIndex;i++)
		{
			if (mCollisionMethodNumber == 0)
			{
				elements[i]->AABB->Update();
				elements[i]->CollisionType = CollisionDetectionTypes::AABBvsAABB;
				elements[i]->SetBoundsAABB(const_cast<XMFLOAT3&>(elements[i]->AABB->GetAABB().at(0)), const_cast<XMFLOAT3&>(elements[i]->AABB->GetAABB().at(1))); // oof... const_cast... bad design

			}
			else if (mCollisionMethodNumber == 1)
			{
				elements[i]->OBB->Update(gameTime);
				elements[i]->CollisionType = CollisionDetectionTypes::OBBvsOBB;

			}

			elements[i]->IsColliding(false);
			elements[i]->DoSphereCheckFirst(mIsCollisionPrechecked);
		}
	}
	void CollisionTestDemo::UpdateImGui()
	{
		#pragma region LEVEL_SPECIFIC_IMGUI

		ImGui::Begin("Profiling stats");

		ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1), "Number of collisions %i", mNumberOfCollisions);
		ImGui::Separator();

		ImGui::TextColored(ImVec4(0.8f, 0.5f, 0.2f, 1), "Objects Transform Update %f ms", mElapsedTimeTransformUpdate.count() * 1000);
		if (mCollisionMethodNumber == 0 && mRecalculateAABBs && mIsDynamic)
		{
			ImGui::TextColored(ImVec4(0.8f, 0.5f, 0.2f, 1), " -> Recalculate AABB %f ms", mElapsedTimeRecalculateAABBUpdate.count() * 1000);
		}
		ImGui::TextColored(ImVec4(0.8f, 0.5f, 0.2f, 1), "Spatial Elements Update %f ms", mElapsedTimeElementUpdate.count() * 1000);
		ImGui::TextColored(ImVec4(0.8f, 0.5f, 0.2f, 1), "Spatial Hierarchy Update %f ms", mElapsedTimeStructureUpdate.count() * 1000);

		if (mSpatialHierarchyNumber == 0)	ImGui::TextColored(ImVec4(0.8f, 0.5f, 0.2f, 1), " -> Collision Update %f ms", mElapsedTimeStructureUpdate.count() * 1000);
		if (mSpatialHierarchyNumber == 1)
		{
			ImGui::TextColored(ImVec4(0.8f, 0.5f, 0.2f, 1), " -> Collision Update %f ms", mOctreeStructure->GetCollisionUpdateTime() * 1000);
			ImGui::TextColored(ImVec4(0.8f, 0.5f, 0.2f, 1), " -> Octree Rebuild %f ms", mOctreeStructure->GetRebuildUpdateTime() * 1000);

		}


		ImGui::End();


		ImGui::Begin("Collision Detection - Demo");

		ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1), "Rendered dynamic instances: %i/%i", mDynamicInstancedObject->GetInstanceCount(), NUM_DYNAMIC_INSTANCES);
		ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1), "Rendered static instances: %i/%i", mStaticInstancedObject->GetInstanceCount(), NUM_STATIC_INSTANCES);

		ImGui::Separator();
		ImGui::Checkbox("Objects Movement", &mIsDynamic);
		ImGui::SliderFloat("Speed", &mSpeed, 0.0f, 0.01f);
		ImGui::Separator();


		if (ImGui::CollapsingHeader("Spatial Hierarchy"))
		{
			ImGui::RadioButton("None (Bruteforce)", &mSpatialHierarchyNumber, 0);
			ImGui::RadioButton("Octree", &mSpatialHierarchyNumber, 1);

			if (mSpatialHierarchyNumber == 1)
				ImGui::Checkbox("Draw Octree", &mIsOctreeRendered);

		}

		if (ImGui::CollapsingHeader("Collision Method"))
		{
			ImGui::RadioButton("AABB vs AABB", &mCollisionMethodNumber, 0);
			ImGui::RadioButton("OBB vs OBB", &mCollisionMethodNumber, 1);

			switch (mCollisionMethodNumber)
			{
			case 0:
				ImGui::Checkbox("Draw objects' AABBs", &mIsAABBRendered);
				ImGui::Checkbox("Recalculate AABBs", &mRecalculateAABBs);
				mIsOBBRendered = false;
				break;
			case 1:
				ImGui::Checkbox("Draw objects' OBBs", &mIsOBBRendered);
				mIsAABBRendered = false;
				mRecalculateAABBs = false;
				break;
			}

		}


		if (ImGui::CollapsingHeader("Optimizations"))
		{
			ImGui::Checkbox("Transform Update - Data Access", &mIsTransformOptimized);
			ImGui::Checkbox("Bruteforce Update - Smart check", &mBruteforceStructure->CheckPairs());
			ImGui::Checkbox("Element Update - Sphere pre-check", &mIsCollisionPrechecked);
			ImGui::Checkbox("Recalculate AABB - Convex Hull", &mIsAABBRecalculatedFromConvexHull);

		}


		ImGui::End();
		#pragma endregion
	}

	// Rendering
	void CollisionTestDemo::Draw(const GameTime& gameTime)
	{

		mRenderStateHelper.SaveRasterizerState();

		#pragma region DRAW_SKYBOX
		//mSkybox->Draw(gameTime);
#pragma endregion

		ID3D11DeviceContext* direct3DDeviceContext = mGame->Direct3DDeviceContext();
		direct3DDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		#pragma region DRAW_OBJECTS
		mDynamicInstancedObject->Draw("instancing");
		mStaticInstancedObject->Draw("instancing");
		#pragma endregion

		#pragma region DRAW_GIZMOS
		
		if (mIsAABBRendered)
			std::for_each(mSpatialElements.begin(), mSpatialElements.end(), [&](SpatialElement* a) {a->AABB->Draw(); });
		if (mIsOBBRendered)
			std::for_each(mSpatialElements.begin(), mSpatialElements.end(), [&](SpatialElement* a) {a->OBB->Draw(gameTime); });


		mVolumeDebugAABB->Draw();

		if (mIsOctreeRendered)
			mOctreeStructure->GetRootNode()->DrawCells(gameTime);
		
		//currentLightSource.ProxyModel->Draw(gameTime);
		#pragma endregion
		
		mRenderStateHelper.SaveAll();
		//mRenderStateHelper.RestoreAll();

		#pragma region DRAW_IMGUI
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

#pragma endregion
	}
	
	// Utility methods
	float CollisionTestDemo::CalculateObjectRadius(std::vector<XMFLOAT3> vertices)
	{
		float Radius = -1;
		for (size_t i = 0; i < vertices.size(); i++)
		{
			float tempRadius = sqrt(vertices.at(i).x*vertices.at(i).x + vertices.at(i).y*vertices.at(i).y+ vertices.at(i).z*vertices.at(i).z);
			if (tempRadius > Radius) Radius = tempRadius;
		}

		return Radius;
	}
	void CollisionTestDemo::CalculateDynamicObjectsRandomDistribution()
	{
		for (size_t i = 0; i < NUM_DYNAMIC_INSTANCES; i++)
		{
			// random position
			float x = (rand() / (static_cast<float>(RAND_MAX) + 1.0f)) * (25.0f - (-25.0f) - 2.0f * 5.0f) + (-25.0f) + 5.0f;
			float y = (rand() / (static_cast<float>(RAND_MAX) + 1.0f)) * (25.0f - (-25.0f) - 2.0f * 5.0f) + (-25.0f) + 5.0f;
			float z = (rand() / (static_cast<float>(RAND_MAX) + 1.0f)) * (25.0f - (-25.0f) - 2.0f * 5.0f) + (-25.0f) + 5.0f;
			mDynamicObjectInstanceData.push_back(InstancingMaterial::InstancedData(XMMatrixTranslation(x, y, z)));
			mDynamicObjectInstancesPositions.push_back(XMFLOAT3(x, y, z));

			float dirX = (rand() / (static_cast<float>(RAND_MAX) + 1.0f)) *	(25.0f - (-25.0f)) + (-25.0f);
			float dirY = (rand() / (static_cast<float>(RAND_MAX) + 1.0f)) *	(25.0f - (-25.0f)) + (-25.0f);
			float dirZ = (rand() / (static_cast<float>(RAND_MAX) + 1.0f)) * (25.0f - (-25.0f)) + (-25.0f);
			mDynamicObjectInstancesDirections.push_back(XMFLOAT3(dirX, dirY, dirZ));

			mSpatialElements.at(i)->SetBoundsAABB(mDynamicInstancedObject->GetLocalAABB().at(0), mDynamicInstancedObject->GetLocalAABB().at(1));
			mSpatialElements.at(i)->SetPosition(mDynamicObjectInstancesPositions[i]);

			// create a debug AABB & OBB visualization for every instance
			mSpatialElements.at(i)->AABB = new RenderableAABB(*mGame, *mCamera);
			mSpatialElements.at(i)->AABB->Initialize();
			mSpatialElements.at(i)->AABB->InitializeGeometry(mDynamicInstancedObject->GetLocalAABB(), XMMatrixScaling(1, 1, 1));
			mSpatialElements.at(i)->AABB->SetPosition(XMFLOAT3(x, y, z));
			mSpatialElements.at(i)->AABB->SetAABB(mDynamicMeshAABBs[i]);

			mSpatialElements.at(i)->OBB = new RenderableOBB(*mGame, *mCamera);
			mSpatialElements.at(i)->OBB->Initialize();
			mSpatialElements.at(i)->OBB->InitializeGeometry(mDynamicInstancedObject->GetLocalAABB(), XMMatrixScaling(1, 1, 1));
			mSpatialElements.at(i)->OBB->SetPosition(XMFLOAT3(x, y, z));
			mSpatialElements.at(i)->OBB->SetRotationMatrix(XMMatrixRotationAxis(XMVECTOR{ 1.0, 0.0, 0.0f }, 0.0f));
		}
	}
	void CollisionTestDemo::CalculateStaticObjectsRandomDistribution()
	{
		for (size_t i = NUM_DYNAMIC_INSTANCES; i < NUM_DYNAMIC_INSTANCES + NUM_STATIC_INSTANCES; i++)
		{
			// random position
			float x = (rand() / (static_cast<float>(RAND_MAX) + 1.0f)) * (25.0f - (-25.0f) - 2.0f * 5.0f) + (-25.0f) + 5.0f;
			float y = (rand() / (static_cast<float>(RAND_MAX) + 1.0f)) * (25.0f - (-25.0f) - 2.0f * 5.0f) + (-25.0f) + 5.0f;
			float z = (rand() / (static_cast<float>(RAND_MAX) + 1.0f)) * (25.0f - (-25.0f) - 2.0f * 5.0f) + (-25.0f) + 5.0f;
			mStaticObjectInstanceData.push_back(InstancingMaterial::InstancedData(XMMatrixTranslation(x, y, z)));
			mStaticObjectInstancesPositions.push_back(XMFLOAT3(x, y, z));
			mSpatialElements.at(i)->SetBoundsAABB(mStaticInstancedObject->GetLocalAABB().at(0), mStaticInstancedObject->GetLocalAABB().at(1));
			mSpatialElements.at(i)->SetPosition(mStaticObjectInstancesPositions[i - NUM_DYNAMIC_INSTANCES]);

			// create a debug AABB & OBB visualization for every instance
			mSpatialElements.at(i)->AABB = new RenderableAABB(*mGame, *mCamera);
			mSpatialElements.at(i)->AABB->Initialize();
			mSpatialElements.at(i)->AABB->InitializeGeometry(mStaticInstancedObject->GetLocalAABB(), XMMatrixScaling(1, 1, 1));
			mSpatialElements.at(i)->AABB->SetPosition(XMFLOAT3(x, y, z));
			mSpatialElements.at(i)->AABB->SetAABB(mStaticMeshAABBs[i - NUM_DYNAMIC_INSTANCES]);

			mSpatialElements.at(i)->OBB = new RenderableOBB(*mGame, *mCamera);
			mSpatialElements.at(i)->OBB->Initialize();
			mSpatialElements.at(i)->OBB->InitializeGeometry(mStaticInstancedObject->GetLocalAABB(), XMMatrixScaling(1, 1, 1));
			mSpatialElements.at(i)->OBB->SetPosition(XMFLOAT3(x, y, z));
			mSpatialElements.at(i)->OBB->SetRotationMatrix(XMMatrixRotationAxis(XMVECTOR{ 1.0, 0.0, 0.0f }, 0.0f));
		}
	}
	void CollisionTestDemo::RecalculateAABB(int index, RenderingObject& object, XMMATRIX& mat)
	{
		if (mIsAABBRecalculatedFromConvexHull)
		{
			int numVertices = mDynamicConvexHullObject->GetVertices().size();
			// recalculate AABB 
			XMFLOAT3 minVertex = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
			XMFLOAT3 maxVertex = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

			for (UINT i = 0; i < numVertices; i++)
			{
				XMVECTOR tempVector = XMVector3Transform(XMLoadFloat3(&(mDynamicConvexHullObject->GetVertices()).at(i)), mat);

				//Get the smallest vertex 
				minVertex.x = std::min(minVertex.x, tempVector.m128_f32[0]);    // Find smallest x value in model
				minVertex.y = std::min(minVertex.y, tempVector.m128_f32[1]);    // Find smallest y value in model
				minVertex.z = std::min(minVertex.z, tempVector.m128_f32[2]);    // Find smallest z value in model

				maxVertex.x = std::max(maxVertex.x, tempVector.m128_f32[0]);    // Find largest x value in model
				maxVertex.y = std::max(maxVertex.y, tempVector.m128_f32[1]);    // Find largest y value in model
				maxVertex.z = std::max(maxVertex.z, tempVector.m128_f32[2]);    // Find largest z value in model
			}


			// store unique AABB of an instance
			mDynamicMeshAABBs.at(index).at(0) = (minVertex);
			mDynamicMeshAABBs.at(index).at(1) = (maxVertex);
		}
		else
		{
			int numVertices = object.GetVertices().size();

			// recalculate AABB 
			XMFLOAT3 minVertex = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
			XMFLOAT3 maxVertex = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

			for (UINT i = 0; i < numVertices; i++)
			{
				XMVECTOR tempVector = XMVector3Transform(XMLoadFloat3(&(object.GetVertices()).at(i)), mat);

				//Get the smallest vertex 
				minVertex.x = std::min(minVertex.x, tempVector.m128_f32[0]);    // Find smallest x value in model
				minVertex.y = std::min(minVertex.y, tempVector.m128_f32[1]);    // Find smallest y value in model
				minVertex.z = std::min(minVertex.z, tempVector.m128_f32[2]);    // Find smallest z value in model

				maxVertex.x = std::max(maxVertex.x, tempVector.m128_f32[0]);    // Find largest x value in model
				maxVertex.y = std::max(maxVertex.y, tempVector.m128_f32[1]);    // Find largest y value in model
				maxVertex.z = std::max(maxVertex.z, tempVector.m128_f32[2]);    // Find largest z value in model
			}

			// store unique AABB of an instance
			mDynamicMeshAABBs[index][0] = (minVertex);
			mDynamicMeshAABBs[index][1] = (maxVertex);
		}

	}
	void CollisionTestDemo::VisualizeCollisionDetection()
	{
		mNumberOfCollisions = 0;

		for (size_t i = 0; i < NUM_DYNAMIC_INSTANCES + NUM_STATIC_INSTANCES; i++)
		{
			if (mSpatialElements[i]->IsColliding())
			{
				mNumberOfCollisions++;
				if (mIsOBBRendered) mSpatialElements[i]->OBB->UpdateColor(XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f));
				if (mIsAABBRendered) mSpatialElements[i]->AABB->UpdateColor(XMFLOAT4(1.0f, 0.0f, 0.0f,1.0f));
			}
			else
			{
				if (mIsOBBRendered) mSpatialElements[i]->OBB->UpdateColor(XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f));
				if (mIsAABBRendered) mSpatialElements[i]->AABB->UpdateColor(XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f));

			}

		}

	}
}