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



#include "DDSTextureLoader.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"


#include <WICTextureLoader.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <sstream>



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
		mSpatialElements(CollisionTestObjects::NUM_DYNAMIC_INSTANCES + CollisionTestObjects::NUM_STATIC_INSTANCES, nullptr),

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

		#pragma region DYNAMIC_OBJECT_INITIALIZATION

		mDynamicInstancedObject = new CollisionTestObjects::DynamicInstancedObject();

		// Load dynamic model
		std::unique_ptr<Model> model_main(new Model(*mGame, "C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\bunny.fbx", true));

		std::unique_ptr<Model> model_main_convexhull(new Model(*mGame, "C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\bunny_convexhull.fbx", true));


		// Load the instaning instancingEffect shader
		Effect* instancingEffect = new Effect(*mGame);
		instancingEffect->CompileFromFile(L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Effects\\Instancing.fx");

		mDynamicInstancedObject->Vertices = model_main->Meshes().at(0)->Vertices();
		mDynamicInstancedObject->VerticesConvexHull = model_main_convexhull->Meshes().at(0)->Vertices();

		for (size_t i = 0; i < CollisionTestObjects::NUM_DYNAMIC_INSTANCES; i++)
		{
			mDynamicMeshVertices.push_back(model_main->Meshes().at(0)->Vertices());

		}


		// Load Vertex & Index Buffers
		for (Mesh* mesh : model_main->Meshes())
		{
			mDynamicInstancedObject->MeshesIndexBuffers.push_back(nullptr);
			mDynamicInstancedObject->MeshesIndicesCounts.push_back(0);

			mDynamicInstancedObject->Materials.push_back(new InstancingMaterial());
			mDynamicInstancedObject->Materials.back()->Initialize(instancingEffect);

			//Vertex buffers
			ID3D11Buffer* vertexBuffer = nullptr;
			mDynamicInstancedObject->Materials.back()->CreateVertexBuffer(mGame->Direct3DDevice(), *mesh, &vertexBuffer);
			mDynamicInstancedObject->MeshesVertexBuffers.push_back(CollisionTestObjects::DynamicInstancedObject::VertexBufferData(vertexBuffer, mDynamicInstancedObject->Materials.back()->VertexSize(), 0));

			//Index buffers
			mesh->CreateIndexBuffer(&mDynamicInstancedObject->MeshesIndexBuffers[mDynamicInstancedObject->MeshesIndexBuffers.size() - 1]);
			mDynamicInstancedObject->MeshesIndicesCounts[mDynamicInstancedObject->MeshesIndicesCounts.size() - 1] = mesh->Indices().size();

		}

		// generate an AABB vector
		mDynamicInstancedObject->ModelAABB = model_main->GenerateAABB();


		for (size_t i = 0; i < CollisionTestObjects::NUM_DYNAMIC_INSTANCES; i++)
		{
			mDynamicMeshAABBs.push_back(mDynamicInstancedObject->ModelAABB);
			mDynamicMeshOBBs.push_back(mDynamicInstancedObject->ModelAABB);
		}

		float dynObjRad = CalculateObjectRadius(mDynamicInstancedObject->Vertices);

		for (size_t i = 0; i < CollisionTestObjects::NUM_DYNAMIC_INSTANCES; i++)
		{

			mSpatialElements[i] = new SpatialElement(i);
			mSpatialElements[i]->SetRadius(dynObjRad);

		}

		// generate dynamic objects' random positions within a volume
		CalculateDynamicObjectsRandomDistribution();

		// for every mesh material create instance buffer
		for (size_t i = 0; i < mDynamicInstancedObject->Materials.size(); i++)
		{
			//ID3D11Buffer* instanceBuffer = nullptr;
			mDynamicInstancedObject->Materials[i]->CreateInstanceBuffer(mGame->Direct3DDevice(), mDynamicInstancedObject->InstanceData, &mDynamicInstancedObject->InstanceBuffer);
			mDynamicInstancedObject->InstanceCount = mDynamicInstancedObject->InstanceData.size();
			mDynamicInstancedObject->MeshesVertexBuffers.push_back(CollisionTestObjects::DynamicInstancedObject::VertexBufferData(mDynamicInstancedObject->InstanceBuffer, mDynamicInstancedObject->Materials[i]->InstanceSize(), 0));

		}

		// Load diffuse texture
		std::wstring textureName = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\emptyDiffuseMap.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textureName.c_str(), nullptr, &mDynamicInstancedObject->AlbedoMap)))
		{
			throw GameException("Failed to load 'Albedo Map'.");
		}

		// Load normal texture
		std::wstring textureName2 = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\emptyNormalMap.jpg";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textureName2.c_str(), nullptr, &mDynamicInstancedObject->NormalMap)))
		{
			throw GameException("Failed to load 'Normal Map'.");
		}

		#pragma endregion

		#pragma region STATIC_OBJECT_INITIALIZATION

		mStaticInstancedObject = new CollisionTestObjects::StaticInstancedObject();

		// Load static model
		std::unique_ptr<Model> model_main_static(new Model(*mGame, "C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\lowpolySphere.fbx", true));

		// Load the instaning instancingEffect shader
		Effect* instancingEffect2 = new Effect(*mGame);
		instancingEffect2->CompileFromFile(L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Effects\\Instancing.fx");


		mStaticInstancedObject->Vertices = model_main_static->Meshes().at(0)->Vertices();


		for (size_t i = 0; i < CollisionTestObjects::NUM_STATIC_INSTANCES; i++)
		{
			mStaticMeshVertices.push_back(model_main_static->Meshes().at(0)->Vertices());

		}


		// Load Vertex & Index Buffers
		for (Mesh* mesh : model_main_static->Meshes())
		{
			mStaticInstancedObject->MeshesIndexBuffers.push_back(nullptr);
			mStaticInstancedObject->MeshesIndicesCounts.push_back(0);
			mStaticInstancedObject->Materials.push_back(new InstancingMaterial());
			mStaticInstancedObject->Materials.back()->Initialize(instancingEffect2);

			//Vertex buffers
			ID3D11Buffer* vertexBuffer = nullptr;
			mStaticInstancedObject->Materials.back()->CreateVertexBuffer(mGame->Direct3DDevice(), *mesh, &vertexBuffer);
			mStaticInstancedObject->MeshesVertexBuffers.push_back(CollisionTestObjects::StaticInstancedObject::VertexBufferData(vertexBuffer, mStaticInstancedObject->Materials.back()->VertexSize(), 0));

			//Index buffers
			mesh->CreateIndexBuffer(&mStaticInstancedObject->MeshesIndexBuffers[mStaticInstancedObject->MeshesIndexBuffers.size() - 1]);
			mStaticInstancedObject->MeshesIndicesCounts[mStaticInstancedObject->MeshesIndicesCounts.size() - 1] = mesh->Indices().size();

		}

		// generate an AABB vector
		mStaticInstancedObject->ModelAABB = model_main_static->GenerateAABB();


		for (size_t i = 0; i < CollisionTestObjects::NUM_STATIC_INSTANCES; i++)
		{
			mStaticMeshAABBs.push_back(mStaticInstancedObject->ModelAABB);
			mStaticMeshOBBs.push_back(mStaticInstancedObject->ModelAABB);
		}

		float staticObjRad = CalculateObjectRadius(mStaticInstancedObject->Vertices);

		for (size_t i = CollisionTestObjects::NUM_DYNAMIC_INSTANCES; i < CollisionTestObjects::NUM_DYNAMIC_INSTANCES + CollisionTestObjects::NUM_STATIC_INSTANCES; i++)
		{
			mSpatialElements[i] = new SpatialElement(i);
			mSpatialElements[i]->SetRadius(staticObjRad);

		}

		// generate static objects' random positions within a volume
		CalculateStaticObjectsRandomDistribution();

		// for every mesh material create instance buffer
		for (size_t i = 0; i < mStaticInstancedObject->Materials.size(); i++)
		{
			//ID3D11Buffer* instanceBuffer = nullptr;
			mStaticInstancedObject->Materials[i]->CreateInstanceBuffer(mGame->Direct3DDevice(), mStaticInstancedObject->InstanceData, &mStaticInstancedObject->InstanceBuffer);
			mStaticInstancedObject->InstanceCount = mStaticInstancedObject->InstanceData.size();
			mStaticInstancedObject->MeshesVertexBuffers.push_back(CollisionTestObjects::StaticInstancedObject::VertexBufferData(mStaticInstancedObject->InstanceBuffer, mStaticInstancedObject->Materials[i]->InstanceSize(), 0));

		}

		// Load diffuse texture
		std::wstring textureNameStatic = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\emptyDiffuseMap2.png";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textureNameStatic.c_str(), nullptr, &mStaticInstancedObject->AlbedoMap)))
		{
			throw GameException("Failed to load 'Albedo Map'.");
		}

		// Load normal texture
		std::wstring textureName2Static = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\emptyNormalMap.jpg";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textureName2Static.c_str(), nullptr, &mStaticInstancedObject->NormalMap)))
		{
			throw GameException("Failed to load 'Normal Map'.");
		}

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
		currentLightSource.ProxyModel = new ProxyModel(*mGame, *mCamera, "C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\DirectionalLightProxy.obj", 0.5f);
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
		mSkybox = new Skybox(*mGame, *mCamera, L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\Sky_Type_1.dds", 100.0f);
		mSkybox->Initialize();

	}


	// Update methods
	void CollisionTestDemo::Update(const GameTime& gameTime)
	{
		UpdateDirectionalLight(gameTime);

		mVolumeDebugAABB->Update(gameTime);

		if (mIsDynamic)
		{
			std::for_each(mSpatialElements.begin(), mSpatialElements.end(), [&](SpatialElement* a)
			{
				a->OBB->isColliding = false;
				a->AABB->isColliding = false;
			});

			auto startTimerTransformUpdate = std::chrono::high_resolution_clock::now();
			
			if (mIsTransformOptimized)
				UpdateObjectsTransforms_OptimizedDataAccess(*mDynamicInstancedObject, gameTime);
			else
				UpdateObjectsTransforms_UnoptimizedDataAccess(*mDynamicInstancedObject, gameTime);

			auto endTimerTransformUpdate = std::chrono::high_resolution_clock::now();
			mElapsedTimeTransformUpdate = endTimerTransformUpdate - startTimerTransformUpdate;

		}

		#pragma region UPDATE_SPATIAL_ELEMENT
		
		auto startTimerElementUpdate = std::chrono::high_resolution_clock::now();

		UpdateSpatialElement(mSpatialElements, 0, CollisionTestObjects::NUM_DYNAMIC_INSTANCES + CollisionTestObjects::NUM_STATIC_INSTANCES, gameTime);

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
	void CollisionTestDemo::UpdateObjectsTransforms_UnoptimizedDataAccess(CollisionTestObjects::DynamicInstancedObject& object, const GameTime& gameTime)
	{
		if (mDynamicMeshVertices.size() == 0) return;

		for (size_t i = 0; i < CollisionTestObjects::NUM_DYNAMIC_INSTANCES; i++)
		{

			#pragma region WALL_COLLISION

			if (object.InstancesPositions.at(i).x + object.ModelAABB.at(0).x <= -25.0f)
			{
				// Left wall
				XMVECTOR normal = { 1.0f, 0.0f, 0.0f };

				XMVECTOR currentDirection = XMLoadFloat3(&object.InstancesDirections.at(i));
				XMVECTOR newDirection = currentDirection - XMVECTOR{ 2.0f, 2.0f, 2.0f } *normal * XMVector3Dot(currentDirection, normal);

				XMStoreFloat3(&object.InstancesDirections.at(i), newDirection);
			}

			if (object.InstancesPositions.at(i).x + object.ModelAABB.at(1).x >= 25.0f)
			{
				// Right wall
				XMVECTOR normal = { -1.0f, 0.0f, 0.0f };

				XMVECTOR currentDirection = XMLoadFloat3(&object.InstancesDirections.at(i));
				XMVECTOR newDirection = currentDirection - XMVECTOR{ 2.0f, 2.0f, 2.0f } *normal * XMVector3Dot(currentDirection, normal);

				XMStoreFloat3(&object.InstancesDirections.at(i), newDirection);
			}

			if (object.InstancesPositions.at(i).z + object.ModelAABB.at(0).z <= -25.0f)
			{
				// Front Wall

				XMVECTOR normal = { 0.0f, 0.0f, 1.0f };

				XMVECTOR currentDirection = XMLoadFloat3(&object.InstancesDirections.at(i));
				XMVECTOR newDirection = currentDirection - XMVECTOR{ 2.0f, 2.0f, 2.0f } *normal * XMVector3Dot(currentDirection, normal);

				XMStoreFloat3(&object.InstancesDirections.at(i), newDirection);
			}

			if (object.InstancesPositions.at(i).z + object.ModelAABB.at(1).z >= 25.0f)
			{
				// Back wall
				XMVECTOR normal = { 0.0f, 0.0f, -1.0f };

				XMVECTOR currentDirection = XMLoadFloat3(&object.InstancesDirections.at(i));
				XMVECTOR newDirection = currentDirection - XMVECTOR{ 2.0f, 2.0f, 2.0f } *normal * XMVector3Dot(currentDirection, normal);

				XMStoreFloat3(&object.InstancesDirections.at(i), newDirection);
			}

			if (object.InstancesPositions.at(i).y + object.ModelAABB.at(0).y <= -25.0f)
			{
				// Bottom wall
				XMVECTOR normal = { 0.0f, 1.0f, 0.0f };

				XMVECTOR currentDirection = XMLoadFloat3(&object.InstancesDirections.at(i));
				XMVECTOR newDirection = currentDirection - XMVECTOR{ 2.0f, 2.0f, 2.0f } *normal * XMVector3Dot(currentDirection, normal);

				XMStoreFloat3(&object.InstancesDirections.at(i), newDirection);
			}

			if (object.InstancesPositions.at(i).y + object.ModelAABB.at(1).y >= 25.0f)
			{
				// Top wall
				XMVECTOR normal = { 0.0f, -1.0f, 0.0f };

				XMVECTOR currentDirection = XMLoadFloat3(&object.InstancesDirections.at(i));
				XMVECTOR newDirection = currentDirection - XMVECTOR{ 2.0f, 2.0f, 2.0f } *normal * XMVector3Dot(currentDirection, normal);

				XMStoreFloat3(&object.InstancesDirections.at(i), newDirection);
			}

			#pragma endregion

			// update positions
			object.InstancesPositions.at(i).x += object.InstancesDirections.at(i).x*mSpeed;
			object.InstancesPositions.at(i).y += object.InstancesDirections.at(i).y*mSpeed;
			object.InstancesPositions.at(i).z += object.InstancesDirections.at(i).z*mSpeed;

			// update rotations
			XMMATRIX rotation = XMMatrixRotationY(gameTime.TotalGameTime());
			mDynamicInstancedObject->InstanceData.at(i) = InstancingMaterial::InstancedData
			(
				rotation *

				XMMatrixTranslation
				(
					object.InstancesPositions.at(i).x,
					object.InstancesPositions.at(i).y,
					object.InstancesPositions.at(i).z
				)

			);

			// update positions of spatial objects
			mSpatialElements.at(i)->SetPosition(object.InstancesPositions.at(i));


			if (mCollisionMethodNumber == 0)
			{
				// update AABBs gizmos positions
				mSpatialElements.at(i)->AABB->SetPosition(
					XMFLOAT3
					(
						object.InstancesPositions.at(i).x,
						object.InstancesPositions.at(i).y,
						object.InstancesPositions.at(i).z
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
						object.InstancesPositions.at(i).x,
						object.InstancesPositions.at(i).y,
						object.InstancesPositions.at(i).z
					)
				);

				// set rotations for OBBs
				mSpatialElements.at(i)->OBB->SetRotationMatrix(rotation);
			}




		}

		for (size_t i = 0; i < mDynamicInstancedObject->Materials.size(); i++)
		{
			// dynamically update instance buffer
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
			mGame->Direct3DDeviceContext()->Map(mDynamicInstancedObject->MeshesVertexBuffers.at(1).VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			memcpy(mappedResource.pData, &mDynamicInstancedObject->InstanceData[0], sizeof(mDynamicInstancedObject->InstanceData[0]) *mDynamicInstancedObject->InstanceCount);
			mGame->Direct3DDeviceContext()->Unmap(mDynamicInstancedObject->MeshesVertexBuffers.at(1).VertexBuffer, 0);

		}
	}
	void CollisionTestDemo::UpdateObjectsTransforms_OptimizedDataAccess(CollisionTestObjects::DynamicInstancedObject& object, const GameTime& gameTime)
	{
		if (mDynamicMeshVertices.size() == 0) return;

		for (size_t i = 0; i < CollisionTestObjects::NUM_DYNAMIC_INSTANCES; i++)
		{

			#pragma region WALL_COLLISION

			if (object.InstancesPositions[i].x + object.ModelAABB.at(0).x <= -25.0f)
			{
				// Left wall
				XMVECTOR normal = { 1.0f, 0.0f, 0.0f };

				XMVECTOR currentDirection = XMLoadFloat3(&object.InstancesDirections[i]);
				XMVECTOR newDirection = currentDirection - XMVECTOR{ 2.0f, 2.0f, 2.0f } *normal * XMVector3Dot(currentDirection, normal);

				XMStoreFloat3(&object.InstancesDirections[i], newDirection);
			}

			if (object.InstancesPositions[i].x + object.ModelAABB.at(1).x >= 25.0f)
			{
				// Right wall
				XMVECTOR normal = { -1.0f, 0.0f, 0.0f };

				XMVECTOR currentDirection = XMLoadFloat3(&object.InstancesDirections[i]);
				XMVECTOR newDirection = currentDirection - XMVECTOR{ 2.0f, 2.0f, 2.0f } *normal * XMVector3Dot(currentDirection, normal);

				XMStoreFloat3(&object.InstancesDirections[i], newDirection);
			}

			if (object.InstancesPositions[i].z + object.ModelAABB.at(0).z <= -25.0f)
			{
				// Front Wall

				XMVECTOR normal = { 0.0f, 0.0f, 1.0f };

				XMVECTOR currentDirection = XMLoadFloat3(&object.InstancesDirections[i]);
				XMVECTOR newDirection = currentDirection - XMVECTOR{ 2.0f, 2.0f, 2.0f } *normal * XMVector3Dot(currentDirection, normal);

				XMStoreFloat3(&object.InstancesDirections[i], newDirection);
			}

			if (object.InstancesPositions[i].z + object.ModelAABB.at(1).z >= 25.0f)
			{
				// Back wall
				XMVECTOR normal = { 0.0f, 0.0f, -1.0f };

				XMVECTOR currentDirection = XMLoadFloat3(&object.InstancesDirections[i]);
				XMVECTOR newDirection = currentDirection - XMVECTOR{ 2.0f, 2.0f, 2.0f } *normal * XMVector3Dot(currentDirection, normal);

				XMStoreFloat3(&object.InstancesDirections[i], newDirection);
			}

			if (object.InstancesPositions[i].y + object.ModelAABB.at(0).y <= -25.0f)
			{
				// Bottom wall
				XMVECTOR normal = { 0.0f, 1.0f, 0.0f };

				XMVECTOR currentDirection = XMLoadFloat3(&object.InstancesDirections[i]);
				XMVECTOR newDirection = currentDirection - XMVECTOR{ 2.0f, 2.0f, 2.0f } *normal * XMVector3Dot(currentDirection, normal);

				XMStoreFloat3(&object.InstancesDirections[i], newDirection);
			}

			if (object.InstancesPositions[i].y + object.ModelAABB.at(1).y >= 25.0f)
			{
				// Top wall
				XMVECTOR normal = { 0.0f, -1.0f, 0.0f };

				XMVECTOR currentDirection = XMLoadFloat3(&object.InstancesDirections[i]);
				XMVECTOR newDirection = currentDirection - XMVECTOR{ 2.0f, 2.0f, 2.0f } *normal * XMVector3Dot(currentDirection, normal);

				XMStoreFloat3(&object.InstancesDirections[i], newDirection);
			}

			#pragma endregion

			// update positions
			object.InstancesPositions[i].x += object.InstancesDirections[i].x*mSpeed;
			object.InstancesPositions[i].y += object.InstancesDirections[i].y*mSpeed;
			object.InstancesPositions[i].z += object.InstancesDirections[i].z*mSpeed;

			// update rotations
			XMMATRIX rotation = XMMatrixRotationY(gameTime.TotalGameTime());
			mDynamicInstancedObject->InstanceData[i] = InstancingMaterial::InstancedData
			(
				rotation *

				XMMatrixTranslation
				(
					object.InstancesPositions[i].x,
					object.InstancesPositions[i].y,
					object.InstancesPositions[i].z
				)

			);

			// update positions of spatial objects
			mSpatialElements[i]->SetPosition(object.InstancesPositions[i]);


			if (mCollisionMethodNumber == 0)
			{
				// update AABBs gizmos positions
				mSpatialElements[i]->AABB->SetPosition(
					XMFLOAT3
					(
						object.InstancesPositions[i].x,
						object.InstancesPositions[i].y,
						object.InstancesPositions[i].z
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
						object.InstancesPositions[i].x,
						object.InstancesPositions[i].y,
						object.InstancesPositions[i].z
					)
				);

				// set rotations for OBBs
				mSpatialElements[i]->OBB->SetRotationMatrix(rotation);
			}




		}

		for (size_t i = 0; i < mDynamicInstancedObject->Materials.size(); i++)
		{
			// dynamically update instance buffer
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
			mGame->Direct3DDeviceContext()->Map(mDynamicInstancedObject->MeshesVertexBuffers.at(1).VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			memcpy(mappedResource.pData, &mDynamicInstancedObject->InstanceData[0], sizeof(mDynamicInstancedObject->InstanceData[0]) *mDynamicInstancedObject->InstanceCount);
			mGame->Direct3DDeviceContext()->Unmap(mDynamicInstancedObject->MeshesVertexBuffers.at(1).VertexBuffer, 0);

		}
	}
	void CollisionTestDemo::UpdateSpatialElement(std::vector<SpatialElement*>& elements, int startIndex, int endIndex, const GameTime& gameTime)
	{
		for	(int i = startIndex; i<endIndex;i++)
		{
			if (mCollisionMethodNumber == 0)
			{
				elements[i]->AABB->Update(gameTime);
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

		ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1), "Rendered dynamic instances: %i/%i", mDynamicInstancedObject->InstanceCount, CollisionTestObjects::NUM_DYNAMIC_INSTANCES);
		ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1), "Rendered static instances: %i/%i", mStaticInstancedObject->InstanceCount, CollisionTestObjects::NUM_STATIC_INSTANCES);

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


		#pragma region DRAW_INSTANCES_DYNAMIC

		// Draw Instances
		for (int i = 0; i < mDynamicInstancedObject->MeshesIndicesCounts.size(); i++)
		{
			Pass* pass = mDynamicInstancedObject->Materials[i]->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mDynamicInstancedObject->Materials[i]->InputLayouts().at(pass);
			direct3DDeviceContext->IASetInputLayout(inputLayout);

			ID3D11Buffer* vertexBuffers[2]
				= { mDynamicInstancedObject->MeshesVertexBuffers[i].VertexBuffer, mDynamicInstancedObject->MeshesVertexBuffers[i + mDynamicInstancedObject->MeshesVertexBuffers.size() / 2].VertexBuffer };

			UINT strides[2] = { mDynamicInstancedObject->MeshesVertexBuffers[i].Stride, mDynamicInstancedObject->MeshesVertexBuffers[i + mDynamicInstancedObject->MeshesVertexBuffers.size() / 2].Stride };
			UINT offsets[2] = { mDynamicInstancedObject->MeshesVertexBuffers[i].Offset, mDynamicInstancedObject->MeshesVertexBuffers[i + mDynamicInstancedObject->MeshesVertexBuffers.size() / 2].Offset };

			direct3DDeviceContext->IASetVertexBuffers(0, 2, vertexBuffers, strides, offsets);
			direct3DDeviceContext->IASetIndexBuffer(mDynamicInstancedObject->MeshesIndexBuffers[i], DXGI_FORMAT_R32_UINT, 0);

			mDynamicInstancedObject->Materials[i]->ViewProjection() << mCamera->ViewMatrix() * mCamera->ProjectionMatrix();

			XMVECTOR empty;

			mDynamicInstancedObject->Materials[i]->LightColor0() << empty;
			mDynamicInstancedObject->Materials[i]->LightPosition0() << empty;
			mDynamicInstancedObject->Materials[i]->LightColor1() << empty;
			mDynamicInstancedObject->Materials[i]->LightPosition1() << empty;
			mDynamicInstancedObject->Materials[i]->LightColor2() << empty;
			mDynamicInstancedObject->Materials[i]->LightPosition2() << empty;
			mDynamicInstancedObject->Materials[i]->LightColor3() << empty;
			mDynamicInstancedObject->Materials[i]->LightPosition3() << empty;

			mDynamicInstancedObject->Materials[i]->LightRadius0() << (float)0;
			mDynamicInstancedObject->Materials[i]->LightRadius1() << (float)0;
			mDynamicInstancedObject->Materials[i]->LightRadius2() << (float)0;
			mDynamicInstancedObject->Materials[i]->LightRadius3() << (float)0;

			mDynamicInstancedObject->Materials[i]->LightDirection() << currentLightSource.DirectionalLight->DirectionVector();
			mDynamicInstancedObject->Materials[i]->DirectionalLightColor() << currentLightSource.DirectionalLight->ColorVector();



			mDynamicInstancedObject->Materials[i]->ColorTexture() << mDynamicInstancedObject->AlbedoMap;
			mDynamicInstancedObject->Materials[i]->NormalTexture() << mDynamicInstancedObject->NormalMap;
			mDynamicInstancedObject->Materials[i]->CameraPosition() << mCamera->PositionVector();

			pass->Apply(0, direct3DDeviceContext);

			direct3DDeviceContext->DrawIndexedInstanced(mDynamicInstancedObject->MeshesIndicesCounts[i], mDynamicInstancedObject->InstanceCount, 0, 0, 0);
		}
#pragma endregion

		#pragma region DRAW_INSTANCES_STATIC
		// Draw Instances
		for (int i = 0; i < mStaticInstancedObject->MeshesIndicesCounts.size(); i++)
		{
			Pass* pass = mStaticInstancedObject->Materials[i]->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mStaticInstancedObject->Materials[i]->InputLayouts().at(pass);
			direct3DDeviceContext->IASetInputLayout(inputLayout);

			ID3D11Buffer* vertexBuffers[2]
				= { mStaticInstancedObject->MeshesVertexBuffers[i].VertexBuffer, mStaticInstancedObject->MeshesVertexBuffers[i + mStaticInstancedObject->MeshesVertexBuffers.size() / 2].VertexBuffer };

			UINT strides[2] = { mStaticInstancedObject->MeshesVertexBuffers[i].Stride, mStaticInstancedObject->MeshesVertexBuffers[i +mStaticInstancedObject->MeshesVertexBuffers.size() / 2].Stride };
			UINT offsets[2] = { mStaticInstancedObject->MeshesVertexBuffers[i].Offset, mStaticInstancedObject->MeshesVertexBuffers[i +mStaticInstancedObject->MeshesVertexBuffers.size() / 2].Offset };

			direct3DDeviceContext->IASetVertexBuffers(0, 2, vertexBuffers, strides, offsets);
			direct3DDeviceContext->IASetIndexBuffer(mStaticInstancedObject->MeshesIndexBuffers[i], DXGI_FORMAT_R32_UINT, 0);

			mStaticInstancedObject->Materials[i]->ViewProjection() << mCamera->ViewMatrix() * mCamera->ProjectionMatrix();

			XMVECTOR empty;

			mStaticInstancedObject->Materials[i]->LightColor0() << empty;
			mStaticInstancedObject->Materials[i]->LightPosition0() << empty;
			mStaticInstancedObject->Materials[i]->LightColor1() << empty;
			mStaticInstancedObject->Materials[i]->LightPosition1() << empty;
			mStaticInstancedObject->Materials[i]->LightColor2() << empty;
			mStaticInstancedObject->Materials[i]->LightPosition2() << empty;
			mStaticInstancedObject->Materials[i]->LightColor3() << empty;
			mStaticInstancedObject->Materials[i]->LightPosition3() << empty;

			mStaticInstancedObject->Materials[i]->LightRadius0() << (float)0;
			mStaticInstancedObject->Materials[i]->LightRadius1() << (float)0;
			mStaticInstancedObject->Materials[i]->LightRadius2() << (float)0;
			mStaticInstancedObject->Materials[i]->LightRadius3() << (float)0;

			mStaticInstancedObject->Materials[i]->LightDirection() << currentLightSource.DirectionalLight->DirectionVector();
			mStaticInstancedObject->Materials[i]->DirectionalLightColor() << currentLightSource.DirectionalLight->ColorVector();



			mStaticInstancedObject->Materials[i]->ColorTexture() << mStaticInstancedObject->AlbedoMap;
			mStaticInstancedObject->Materials[i]->NormalTexture() << mStaticInstancedObject->NormalMap;
			mStaticInstancedObject->Materials[i]->CameraPosition() << mCamera->PositionVector();

			pass->Apply(0, direct3DDeviceContext);

			direct3DDeviceContext->DrawIndexedInstanced(mStaticInstancedObject->MeshesIndicesCounts[i], mStaticInstancedObject->InstanceCount, 0, 0, 0);
		}
		#pragma endregion

		#pragma region DRAW_GIZMOS
		
		if (mIsAABBRendered)
			std::for_each(mSpatialElements.begin(), mSpatialElements.end(), [&](SpatialElement* a) {a->AABB->Draw(gameTime); });
		if (mIsOBBRendered)
			std::for_each(mSpatialElements.begin(), mSpatialElements.end(), [&](SpatialElement* a) {a->OBB->Draw(gameTime); });


		mVolumeDebugAABB->Draw(gameTime);

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
	float CollisionTestDemo::CalculateObjectRadius( std::vector<XMFLOAT3>& vertices)
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
		for (size_t i = 0; i < CollisionTestObjects::NUM_DYNAMIC_INSTANCES; i++)
		{

			// random position
			float x = (rand() / (static_cast<float>(RAND_MAX) + 1.0f)) * (25.0f - (-25.0f) - 2.0f * 5.0f) + (-25.0f) + 5.0f;
			float y = (rand() / (static_cast<float>(RAND_MAX) + 1.0f)) * (25.0f - (-25.0f) - 2.0f * 5.0f) + (-25.0f) + 5.0f;
			float z = (rand() / (static_cast<float>(RAND_MAX) + 1.0f)) * (25.0f - (-25.0f) - 2.0f * 5.0f) + (-25.0f) + 5.0f;

			mDynamicInstancedObject->InstanceData.push_back(InstancingMaterial::InstancedData(XMMatrixTranslation(x, y, z)));
			mDynamicInstancedObject->InstancesPositions.push_back(XMFLOAT3(x, y, z));


			float dirX = (rand() / (static_cast<float>(RAND_MAX) + 1.0f)) *	(25.0f - (-25.0f)) + (-25.0f);
			float dirY = (rand() / (static_cast<float>(RAND_MAX) + 1.0f)) *	(25.0f - (-25.0f)) + (-25.0f);
			float dirZ = (rand() / (static_cast<float>(RAND_MAX) + 1.0f)) * (25.0f - (-25.0f)) + (-25.0f);

			mDynamicInstancedObject->InstancesDirections.push_back(XMFLOAT3(dirX, dirY, dirZ));


			mSpatialElements.at(i)->SetBoundsAABB(mDynamicInstancedObject->ModelAABB.at(0), mDynamicInstancedObject->ModelAABB.at(1));
			mSpatialElements.at(i)->SetPosition(mDynamicInstancedObject->InstancesPositions.at(i));


			// create a debug AABB & OBB visualization for every instance
			mSpatialElements.at(i)->AABB = new RenderableAABB(*mGame, *mCamera);
			mSpatialElements.at(i)->AABB->Initialize();
			mSpatialElements.at(i)->AABB->InitializeGeometry(mDynamicInstancedObject->ModelAABB, XMMatrixScaling(1, 1, 1));
			mSpatialElements.at(i)->AABB->SetPosition(XMFLOAT3(x, y, z));

			mSpatialElements.at(i)->OBB = new RenderableOBB(*mGame, *mCamera);
			mSpatialElements.at(i)->OBB->Initialize();
			mSpatialElements.at(i)->OBB->InitializeGeometry(mDynamicInstancedObject->ModelAABB, XMMatrixScaling(1, 1, 1));
			mSpatialElements.at(i)->OBB->SetPosition(XMFLOAT3(x, y, z));
			mSpatialElements.at(i)->OBB->SetRotationMatrix(XMMatrixRotationAxis(XMVECTOR{ 1.0, 0.0, 0.0f }, 0.0f));


		}
	}
	void CollisionTestDemo::CalculateStaticObjectsRandomDistribution()
	{
		for (size_t i = CollisionTestObjects::NUM_DYNAMIC_INSTANCES; i < CollisionTestObjects::NUM_DYNAMIC_INSTANCES + CollisionTestObjects::NUM_STATIC_INSTANCES; i++)
		{

			// random position
			float x = (rand() / (static_cast<float>(RAND_MAX) + 1.0f)) * (25.0f - (-25.0f) - 2.0f * 5.0f) + (-25.0f) + 5.0f;
			float y = (rand() / (static_cast<float>(RAND_MAX) + 1.0f)) * (25.0f - (-25.0f) - 2.0f * 5.0f) + (-25.0f) + 5.0f;
			float z = (rand() / (static_cast<float>(RAND_MAX) + 1.0f)) * (25.0f - (-25.0f) - 2.0f * 5.0f) + (-25.0f) + 5.0f;

			mStaticInstancedObject->InstanceData.push_back(InstancingMaterial::InstancedData(XMMatrixTranslation(x, y, z)));
			mStaticInstancedObject->InstancesPositions.push_back(XMFLOAT3(x, y, z));


			mSpatialElements.at(i)->SetBoundsAABB(mStaticInstancedObject->ModelAABB.at(0), mStaticInstancedObject->ModelAABB.at(1));
			mSpatialElements.at(i)->SetPosition(mStaticInstancedObject->InstancesPositions.at(i - CollisionTestObjects::NUM_DYNAMIC_INSTANCES));


			// create a debug AABB & OBB visualization for every instance
			mSpatialElements.at(i)->AABB = new RenderableAABB(*mGame, *mCamera);
			mSpatialElements.at(i)->AABB->Initialize();
			mSpatialElements.at(i)->AABB->InitializeGeometry(mStaticInstancedObject->ModelAABB, XMMatrixScaling(1, 1, 1));
			mSpatialElements.at(i)->AABB->SetPosition(XMFLOAT3(x, y, z));

			mSpatialElements.at(i)->OBB = new RenderableOBB(*mGame, *mCamera);
			mSpatialElements.at(i)->OBB->Initialize();
			mSpatialElements.at(i)->OBB->InitializeGeometry(mStaticInstancedObject->ModelAABB, XMMatrixScaling(1, 1, 1));
			mSpatialElements.at(i)->OBB->SetPosition(XMFLOAT3(x, y, z));
			mSpatialElements.at(i)->OBB->SetRotationMatrix(XMMatrixRotationAxis(XMVECTOR{ 1.0, 0.0, 0.0f }, 0.0f));


		}
	}
	void CollisionTestDemo::RecalculateAABB(int index, CollisionTestObjects::DynamicInstancedObject & object, XMMATRIX& mat)
	{

		if (mIsAABBRecalculatedFromConvexHull)
		{

			int numVertices = object.VerticesConvexHull.size();
			// recalculate AABB 
			XMFLOAT3 minVertex = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
			XMFLOAT3 maxVertex = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

			for (UINT i = 0; i < numVertices; i++)
			{
				XMVECTOR tempVector = XMVector3Transform(XMLoadFloat3(&object.VerticesConvexHull.at(i)), mat);

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
			int numVertices = object.Vertices.size();

			// recalculate AABB 
			XMFLOAT3 minVertex = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
			XMFLOAT3 maxVertex = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

			for (UINT i = 0; i < numVertices; i++)
			{
				XMVECTOR tempVector = XMVector3Transform(XMLoadFloat3(&object.Vertices.at(i)), mat);

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

	}
	void CollisionTestDemo::VisualizeCollisionDetection()
	{


		mNumberOfCollisions = 0;

		for (size_t i = 0; i < CollisionTestObjects::NUM_DYNAMIC_INSTANCES + CollisionTestObjects::NUM_STATIC_INSTANCES; i++)
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