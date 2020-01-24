#include "stdafx.h"

#include "FrustumCullingDemo.h"
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
#include "..\Library\Skybox.h"

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
	RTTI_DEFINITIONS(FrustumCullingDemo)


	FrustumCullingDemoLightInfo::LightData currentLightSource;

	FrustumCullingDemo::FrustumCullingDemo(Game& game, Camera& camera)
		: DrawableGameComponent(game, camera),

		mKeyboard(nullptr),

		mFrustum(XMMatrixIdentity()),
		mDebugFrustum(nullptr),

		mSkybox(nullptr),
		mInstancedObject(nullptr),
		mDefaultPlaneObject(nullptr),

		mWorldMatrix(MatrixHelper::Identity),
		mRenderStateHelper(game)
	{
	}

	FrustumCullingDemo::~FrustumCullingDemo()
	{
		DeleteObject(mInstancedObject);
		DeleteObject(mDefaultPlaneObject);
		DeleteObject(mDebugFrustum);
		DeleteObject(mSkybox);

		currentLightSource.Destroy();
	}

	/////////////////////////////////////////////////////////////
	// 'DemoLevel' ugly methods...
	bool FrustumCullingDemo::IsComponent()
	{
		return mGame->IsInGameComponents<FrustumCullingDemo*>(mGame->components, this);
	}
	void FrustumCullingDemo::Create()
	{
		Initialize();
		mGame->components.push_back(this);
	}
	void FrustumCullingDemo::Destroy()
	{
		std::pair<bool, int> res = mGame->FindInGameComponents<FrustumCullingDemo*>(mGame->components, this);

		if (res.first)
		{
			mGame->components.erase(mGame->components.begin() + res.second);

			//very provocative...
			delete this;
		}
	}
	/////////////////////////////////////////////////////////////

	void FrustumCullingDemo::Initialize()
	{
		SetCurrentDirectory(Utility::ExecutableDirectory().c_str());

		#pragma region INSTANCED_OBJECT_INITIALIZATION
		mInstancedObject = new InstancedObject();

		// Load the model_main
		std::unique_ptr<Model> model_main(new Model(*mGame, "C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\pine_tree\\pine.fbx", true));

		// Load the instaning instancingEffect shader
		Effect* instancingEffect = new Effect(*mGame);
		instancingEffect->CompileFromFile(L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Effects\\Instancing.fx");

		// Load Vertex & Index Buffers
		for (Mesh* mesh : model_main->Meshes())
		{
			mInstancedObject->MeshesIndexBuffers.push_back(nullptr);
			mInstancedObject->MeshesIndicesCounts.push_back(0);

			mInstancedObject->Materials.push_back(new InstancingMaterial());
			mInstancedObject->Materials.back()->Initialize(instancingEffect);

			//Vertex buffers
			ID3D11Buffer* vertexBuffer = nullptr;
			mInstancedObject->Materials.back()->CreateVertexBuffer(mGame->Direct3DDevice(), *mesh, &vertexBuffer);
			mInstancedObject->MeshesVertexBuffers.push_back(InstancedObject::VertexBufferData(vertexBuffer, mInstancedObject->Materials.back()->VertexSize(), 0));

			//Index buffers
			mesh->CreateIndexBuffer(&mInstancedObject->MeshesIndexBuffers[mInstancedObject->MeshesIndexBuffers.size() - 1]);
			mInstancedObject->MeshesIndicesCounts[mInstancedObject->MeshesIndicesCounts.size() - 1] = mesh->Indices().size();

		}


		// generate an AABB vector
		mInstancedObject->ModelAABB = model_main->GenerateAABB();

		// generate objects' random positions within a disk
		DiskRandomDistribution();

		// for every mesh material create instance buffer
		for (size_t i = 0; i < mInstancedObject->Materials.size(); i++)
		{
			mInstancedObject->InstanceCount = mInstancedObject->InstanceData.size();
			mInstancedObject->Materials[i]->CreateInstanceBuffer(mGame->Direct3DDevice(), mInstancedObject->InstanceData, &mInstancedObject->InstanceBuffer);
			mInstancedObject->MeshesVertexBuffers.push_back(InstancedObject::VertexBufferData(mInstancedObject->InstanceBuffer, mInstancedObject->Materials[i]->InstanceSize(), 0));
		}

		// Load diffuse texture
		std::wstring textureName = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\pine_tree\\Pinetree_D.dds";
		if (FAILED(DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textureName.c_str(), nullptr, &mInstancedObject->AlbedoMap)))
		{
			throw GameException("Failed to load 'Albedo Map'.");
		}

		// Load normal texture
		std::wstring textureName2 = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\pine_tree\\Pinetree_N.dds";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textureName2.c_str(), nullptr, &mInstancedObject->NormalMap)))
		{
			throw GameException("Failed to load 'Normal Map'.");
		}

#pragma endregion

		#pragma region DEFAULT_PLANE_OBJECT_INITIALIZATION
		mDefaultPlaneObject = new DefaultPlaneObject();

		// Default Plane Object
		std::unique_ptr<Model> model_plane(new Model(*mGame, "C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\default_plane\\default_plane.obj", true));

		// Load the instancingEffect
		Effect* ambientEffect = new Effect(*mGame);
		ambientEffect->CompileFromFile(L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Effects\\AmbientLighting.fx");

		mDefaultPlaneObject->Material = new AmbientLightingMaterial();
		mDefaultPlaneObject->Material->Initialize(ambientEffect);

		Mesh* mesh_default_plane = model_plane->Meshes().at(0);
		mDefaultPlaneObject->Material->CreateVertexBuffer(mGame->Direct3DDevice(), *mesh_default_plane, &mDefaultPlaneObject->VertexBuffer);
		mesh_default_plane->CreateIndexBuffer(&mDefaultPlaneObject->IndexBuffer);
		mDefaultPlaneObject->IndexCount = mesh_default_plane->Indices().size();

		// load diffuse texture
		std::wstring textureName3 = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\default_plane\\UV_Grid_Lrg.jpg";
		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textureName3.c_str(), nullptr, &mDefaultPlaneObject->DiffuseTexture)))
		{
			throw GameException("Failed to load plane's 'Diffuse Texture'.");
		}

		#pragma endregion

		// Directional Light initiazlization
		currentLightSource.DirectionalLight = new DirectionalLight(*mGame);
		currentLightSource.ProxyModel =  new ProxyModel(*mGame, *mCamera, "C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\DirectionalLightProxy.obj", 0.5f);
		currentLightSource.ProxyModel->Initialize();
		currentLightSource.ProxyModel->SetPosition(0.0f, 30.0, 100.0f);


		mKeyboard = (Keyboard*)mGame->Services().GetService(Keyboard::TypeIdClass());
		assert(mKeyboard != nullptr);

		//Frustum initialization
		mFrustum.SetMatrix(SetMatrixForCustomFrustum(*mGame, *mCamera));
		
		//Debug frustum visualization
		mDebugFrustum = new RenderableFrustum(*mGame, *mCamera);
		mDebugFrustum->Initialize();
		mDebugFrustum->SetColor((XMFLOAT4)ColorHelper::Red);
		mDebugFrustum->InitializeGeometry(mFrustum);
		//mDebugFrustum->SetPosition(XMFLOAT3(0, 10, 0));

		//Skybox initialization
		mSkybox = new Skybox(*mGame, *mCamera, L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\Sky_Type_4.dds", 100.0f);
		mSkybox->Initialize();

	}

	void FrustumCullingDemo::DiskRandomDistribution()
	{
		for (size_t i = 0; i < NUM_INSTANCES; i++)
		{
			float a = (static_cast <float> (rand()) / static_cast <float> (RAND_MAX)) * 2 * 3.14f;
			double r = mInstancedObject->RandomDistributionRadius * sqrt((static_cast <float> (rand()) / static_cast <float> (RAND_MAX)));

			// If you need it in Cartesian coordinates
			float x = r * cos(a);
			float z = r * sin(a);

			float scale = 0.5f + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / (1.0f - 0.5f)));

			mInstancedObject->InstanceData.push_back(InstancingMaterial::InstancedData(XMMatrixScaling(scale, scale, scale) * XMMatrixTranslation(x, 0, z)));
			mInstancedObject->InstancesPositions.push_back(XMFLOAT3(x, 0, z));

			// create a debug AABB visualization for every instance
			mInstancedObject->DebugInstancesAABBs[i] = new RenderableAABB(*mGame, *mCamera);
			mInstancedObject->DebugInstancesAABBs[i]->Initialize();
			mInstancedObject->DebugInstancesAABBs[i]->InitializeGeometry(mInstancedObject->ModelAABB, XMMatrixScaling(scale, scale, scale));
			mInstancedObject->DebugInstancesAABBs[i]->SetPosition(XMFLOAT3(x, 0, z));

		}
	}

	void FrustumCullingDemo::Update(const GameTime& gameTime)
	{
		UpdateDirectionalLight(gameTime);

		mFrustum.SetMatrix(SetMatrixForCustomFrustum(*mGame, *mCamera));
		
		if (!mIsCustomFrustumActive)
		{
			mDebugFrustum->InitializeGeometry(mFrustum);
			mIsCustomFrustumInitialized = false;
		}
		else if (!mIsCustomFrustumInitialized)
		{
			mDebugFrustum->InitializeGeometry(mFrustum);
			mIsCustomFrustumInitialized = true;
		}
		
		mDebugFrustum->Update(gameTime);

		
		if (mIsCustomFrustumRotating && mIsCustomFrustumActive) RotateCustomFrustum(gameTime);
		else
		{
			mDebugFrustum->Reset();
		}
		
		std::for_each(mInstancedObject->DebugInstancesAABBs.begin(), mInstancedObject->DebugInstancesAABBs.end(), [&](RenderableAABB* a) {a->Update(gameTime); });
		
		currentLightSource.ProxyModel->Update(gameTime);
		
		// Do the culling
		CullAABB(mIsCullingEnabled, mFrustum, mInstancedObject->ModelAABB, mInstancedObject->InstancesPositions);

		mSkybox->Update(gameTime);
		UpdateImGui();
	}

	void FrustumCullingDemo::UpdateImGui()
	{
		#pragma region LEVEL_SPECIFIC_IMGUI
		ImGui::Begin("Frustum Culling - Demo");

		ImGui::TextColored(ImVec4(0.6f, 0.01f, 0.0f, 1), "Number of rendered instances %i/%i", mInstancedObject->InstanceCount, NUM_INSTANCES);

		ImGui::Separator();


		ImGui::Checkbox("Enable culling", &mIsCullingEnabled);
		ImGui::Checkbox("Switch to custom frustum", &mIsCustomFrustumActive);
		ImGui::Checkbox("Rotate custom frustum", &mIsCustomFrustumRotating);
		ImGui::Checkbox("Draw object's AABBs", &mIsAABBRendered);



		ImGui::End();
		#pragma endregion
	}

	void FrustumCullingDemo::CullAABB(bool isEnabled, const Frustum& frustum, const std::vector<XMFLOAT3>& aabb, const std::vector<XMFLOAT3>& positions)
	{
		mInstancedObject->InstanceCount = 0;

		// temp instance data
		std::vector<InstancingMaterial::InstancedData> newInstanceData;

		bool cull = false;

		for (int i = 0; i < NUM_INSTANCES; ++i)
		{
			cull = false;

			if (isEnabled) {
				
				// start a loop through all frustum planes
				for (int planeID = 0; planeID < 6; ++planeID)
				{
					XMVECTOR planeNormal = XMVectorSet(frustum.Planes()[planeID].x, frustum.Planes()[planeID].y, frustum.Planes()[planeID].z, 0.0f);
					float planeConstant = frustum.Planes()[planeID].w;

					XMFLOAT3 axisVert;

					// x-axis
					if (frustum.Planes()[planeID].x > 0.0f) 
						axisVert.x = aabb[0].x + positions[i].x; 
					else
						axisVert.x = aabb[1].x + positions[i].x; 

					// y-axis
					if (frustum.Planes()[planeID].y > 0.0f) 
						axisVert.y = aabb[0].y + positions[i].y;
					else
						axisVert.y = aabb[1].y + positions[i].y;

					// z-axis
					if (frustum.Planes()[planeID].z > 0.0f)  
						axisVert.z = aabb[0].z + positions[i].z;
					else
						axisVert.z = aabb[1].z + positions[i].z;


					if (XMVectorGetX(XMVector3Dot(planeNormal, XMLoadFloat3(&axisVert))) + planeConstant > 0.0f)
					{
						cull = true;
						// Skip remaining planes to check and move on 
						break;
					}
				}
			}

			if (!cull)    
			{
				newInstanceData.push_back(mInstancedObject->InstanceData[i]);
				mInstancedObject->InstanceCount++;
			}
		}

		if (mInstancedObject->InstanceCount > 0)
		{
			// for every mesh material create instance buffer
			for (size_t i = 0; i < mInstancedObject->Materials.size(); i++)
			{
				mInstancedObject->Materials[i]->UpdateInstanceBuffer(mGame->Direct3DDeviceContext(), newInstanceData, mInstancedObject->InstanceCount, mInstancedObject->MeshesVertexBuffers.at(1).VertexBuffer);
				//mInstancedObject->MeshesVertexBuffers[mInstancedObject->Materials.size() + i] = (InstancedObject::VertexBufferData(mInstancedObject->MeshesVertexBuffers.at(0).VertexBuffer, mInstancedObject->Materials[i]->InstanceSize(), 0));
			}
		}
		
	}

	void FrustumCullingDemo::Draw(const GameTime& gameTime)
	{

		mRenderStateHelper.SaveRasterizerState();

		#pragma region DRAW_SKYBOX
		mSkybox->Draw(gameTime);
#pragma endregion

		ID3D11DeviceContext* direct3DDeviceContext = mGame->Direct3DDeviceContext();
		direct3DDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Draw Defaul Plane
		{
			Pass* pass = mDefaultPlaneObject->Material->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mDefaultPlaneObject->Material->InputLayouts().at(pass);
			direct3DDeviceContext->IASetInputLayout(inputLayout);

			UINT stride =  mDefaultPlaneObject->Material->VertexSize();
			UINT offset = 0;
			direct3DDeviceContext->IASetVertexBuffers(0, 1, &mDefaultPlaneObject->VertexBuffer, &stride, &offset);
			direct3DDeviceContext->IASetIndexBuffer(mDefaultPlaneObject->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

			XMMATRIX worldMatrix = XMLoadFloat4x4(&mWorldMatrix);
			XMMATRIX wvp = worldMatrix * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();

			XMFLOAT4 color = {0.9, 0.9, 0.9, 1.0};
			XMVECTOR ambientColor = XMLoadFloat4(&color);

			mDefaultPlaneObject->Material->WorldViewProjection() << wvp;
			mDefaultPlaneObject->Material->ColorTexture() << mDefaultPlaneObject->DiffuseTexture;
			mDefaultPlaneObject->Material->AmbientColor() << ambientColor;

			pass->Apply(0, direct3DDeviceContext);

			direct3DDeviceContext->DrawIndexed(mDefaultPlaneObject->IndexCount, 0, 0);
		
		}

		// Draw Instances
		for (int i = 0; i < mInstancedObject->MeshesIndicesCounts.size(); i++)
		{
			Pass* pass = mInstancedObject->Materials[i]->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mInstancedObject->Materials[i]->InputLayouts().at(pass);
			direct3DDeviceContext->IASetInputLayout(inputLayout);

			ID3D11Buffer* vertexBuffers[2]
						    = { mInstancedObject->MeshesVertexBuffers[i].VertexBuffer, mInstancedObject->MeshesVertexBuffers[i + mInstancedObject->MeshesVertexBuffers.size() / 2].VertexBuffer };
			
			UINT strides[2] = { mInstancedObject->MeshesVertexBuffers[i].Stride, mInstancedObject->MeshesVertexBuffers[i + mInstancedObject->MeshesVertexBuffers.size() / 2].Stride };
			UINT offsets[2] = { mInstancedObject->MeshesVertexBuffers[i].Offset, mInstancedObject->MeshesVertexBuffers[i + mInstancedObject->MeshesVertexBuffers.size() / 2].Offset };

			direct3DDeviceContext->IASetVertexBuffers(0, 2, vertexBuffers, strides, offsets);
			direct3DDeviceContext->IASetIndexBuffer(mInstancedObject->MeshesIndexBuffers[i], DXGI_FORMAT_R32_UINT, 0);

			mInstancedObject->Materials[i]->ViewProjection() << mCamera->ViewMatrix() * mCamera->ProjectionMatrix();

			XMVECTOR empty;

			mInstancedObject->Materials[i]->LightColor0()	 << empty;
			mInstancedObject->Materials[i]->LightPosition0() << empty;
			mInstancedObject->Materials[i]->LightColor1()	 << empty;
			mInstancedObject->Materials[i]->LightPosition1() << empty;
			mInstancedObject->Materials[i]->LightColor2()	 << empty;
			mInstancedObject->Materials[i]->LightPosition2() << empty;
			mInstancedObject->Materials[i]->LightColor3()	 << empty;
			mInstancedObject->Materials[i]->LightPosition3() << empty;

			mInstancedObject->Materials[i]->LightRadius0() << (float)0;
			mInstancedObject->Materials[i]->LightRadius1() << (float)0;
			mInstancedObject->Materials[i]->LightRadius2() << (float)0;
			mInstancedObject->Materials[i]->LightRadius3() << (float)0;

			mInstancedObject->Materials[i]->LightDirection() << currentLightSource.DirectionalLight->DirectionVector();
			mInstancedObject->Materials[i]->DirectionalLightColor() << currentLightSource.DirectionalLight->ColorVector();



			mInstancedObject->Materials[i]->ColorTexture() << mInstancedObject->AlbedoMap;
			mInstancedObject->Materials[i]->NormalTexture() << mInstancedObject->NormalMap;
			mInstancedObject->Materials[i]->CameraPosition() << mCamera->PositionVector();

			pass->Apply(0, direct3DDeviceContext);

			direct3DDeviceContext->DrawIndexedInstanced(mInstancedObject->MeshesIndicesCounts[i], mInstancedObject->InstanceCount, 0, 0, 0);
		}
		

		currentLightSource.ProxyModel->Draw(gameTime);
		
		mDebugFrustum->Draw(gameTime);

		if (mIsAABBRendered) std::for_each(mInstancedObject->DebugInstancesAABBs.begin(), mInstancedObject->DebugInstancesAABBs.end(), [&](RenderableAABB* a) {a->Draw(gameTime); });


		mRenderStateHelper.SaveAll();
		//mRenderStateHelper.RestoreAll();

		#pragma region DRAW_IMGUI
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

#pragma endregion
	}

	void FrustumCullingDemo::RotateCustomFrustum(const GameTime& gameTime)
	{
		float elapsedTime = (float)gameTime.ElapsedGameTime();
		const XMFLOAT2 LightRotationRate = XMFLOAT2(XM_PI / 2, XM_PI / 2);

		XMFLOAT2 rotationAmount = Vector2Helper::Zero;
		rotationAmount.x -= 0.5f*LightRotationRate.x * elapsedTime;

		XMMATRIX projectorRotationMatrix = XMMatrixIdentity();
		projectorRotationMatrix = XMMatrixRotationY(rotationAmount.x);

			
		mDebugFrustum->ApplyRotation(projectorRotationMatrix);

	}

	void FrustumCullingDemo::UpdateDirectionalLight(const GameTime& gameTime)
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

	XMMATRIX FrustumCullingDemo::SetMatrixForCustomFrustum(Game& game, Camera& camera)
	{
		//XMVECTOR direction = XMLoadFloat3(&Vector3Helper::Right);
		if (mIsCustomFrustumActive)
		{

			XMMATRIX projectionMatrix = XMMatrixPerspectiveFovRH(XM_PIDIV4, game.AspectRatio(), 1.0f, 100.0f);
			XMMATRIX viewMatrix;

			if(mDebugFrustum == nullptr)
				viewMatrix = XMMatrixLookToRH(XMVECTOR{ 0, 0, 0 }, XMVECTOR{ 1,0,0 }, XMVECTOR{ 0, 1 ,0 });
			else
				viewMatrix = XMMatrixLookToRH(XMVECTOR{ 0, 0, 0 }, mDebugFrustum->RightVector(), XMVECTOR{ 0, 1 ,0 });
			return XMMatrixMultiply(viewMatrix, projectionMatrix);

		}
		else
		{

			XMMATRIX projectionMatrix = XMMatrixPerspectiveFovRH(camera.FieldOfView(), game.AspectRatio(), 1.0f, camera.FarPlaneDistance());
			XMMATRIX viewMatrix = XMMatrixLookToRH(XMVECTOR{ camera.Position().x,  camera.Position().y,  camera.Position().z }, camera.DirectionVector(), XMVECTOR{ 0, 1 ,0 });
			return XMMatrixMultiply(viewMatrix, projectionMatrix);
		}

	}

}