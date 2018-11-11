#include "stdafx.h"

#include "ShadowMappingDemo.h"
#include "..\Library\Game.h"
#include "..\Library\GameException.h"
#include "..\Library\VectorHelper.h"
#include "..\Library\MatrixHelper.h"
#include "..\Library\ColorHelper.h"
#include "..\Library\Camera.h"
#include "..\Library\Model.h"
#include "..\Library\Mesh.h"
#include "..\Library\Utility.h"
#include "..\Library\PointLight.h"
#include "..\Library\DirectionalLight.h"
#include "..\Library\Keyboard.h"
#include "..\Library\ProxyModel.h"
#include "..\Library\Projector.h"
#include "..\Library\RenderableFrustum.h"
#include "..\Library\ShadowMappingMaterial.h"
#include "..\Library\ShadowMappingDirectionalMaterial.h"
#include "..\Library\EnvironmentMappingMaterial.h"
#include "..\Library\DepthMapMaterial.h"
#include "..\Library\DepthMap.h"
#include "..\Library\FullScreenRenderTarget.h"
#include "..\Library\DemoLevel.h"
#include "..\Library\Skybox.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <WICTextureLoader.h>
#include <DDSTextureLoader.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <sstream>
#include <iomanip>
#include <limits>

namespace Rendering
{
	RTTI_DEFINITIONS(ShadowMappingDemo)

	//number of frustums and clipping distances
	const int NUM_FRUSTUMS = 3;
	const float clippingPlanes[NUM_FRUSTUMS] = {75.0f, 150.0f, 600.0f};

	//light properties
	const float ShadowMappingDemo::LightModulationRate = UCHAR_MAX;
	const float ShadowMappingDemo::LightMovementRate = 10.0f;
	const XMFLOAT2 ShadowMappingDemo::LightRotationRate = XMFLOAT2(XM_PI/2, XM_PI/2);
	
	//depth map properties
	const UINT ShadowMappingDemo::DepthMapWidth = 2048;
	const UINT ShadowMappingDemo::DepthMapHeight = 2048;
	const float ShadowMappingDemo::DepthBiasModulationRate = 10000;

	//sizes of depth maps for debugging
	const RECT ShadowMappingDemo::DepthMapDestinationRectangle0 = { 0, 640, 128, 768 };
	const RECT ShadowMappingDemo::DepthMapDestinationRectangle1 = { 128, 640, 256, 768 };
	const RECT ShadowMappingDemo::DepthMapDestinationRectangle2 = { 256, 640, 384, 768 };

	static float modelColorFloat3[3] = { 0.56f, 0.67f, 0.3f };
	XMVECTOR modelColorVector = XMLoadColor(&XMCOLOR(modelColorFloat3[0], modelColorFloat3[1], modelColorFloat3[2], 1));

	ShadowMappingDemo::ShadowMappingDemo(Game& game, Camera& camera)
		: DrawableGameComponent(game, camera),
		
		mKeyboard(nullptr),
		mProxyModel(nullptr),
		
		mProjectedTextureScalingMatrix(MatrixHelper::Zero),
		mRenderStateHelper(game),

		mPlaneTexture(nullptr), mPlanePositionVertexBuffer(nullptr), mPlanePositionUVNormalVertexBuffer(nullptr),
		mPlaneIndexBuffer(nullptr), mPlaneVertexCount(0),mPlaneWorldMatrix(MatrixHelper::Identity),

		mModelPositionVertexBuffer(nullptr), mModelPositionUVNormalVertexBuffer(nullptr), mModelIndexBuffer(nullptr),
		mModelIndexCount(0),mModelWorldMatrix(MatrixHelper::Identity), mModelWorldMatrices(16, MatrixHelper::Identity),
		
		mPointLight(nullptr), mDirectionalLight(nullptr),
		mAmbientColor(1.0f, 1.0f, 1.0, 0.0f),mSpecularColor(1.0f, 0.0f, 0.0f, 1.0f), mSpecularPower(25.0f),

		mShadowMappingEffect(nullptr), mShadowMappingDirectionalMaterial(nullptr),
		mEnvironmentMappingEffect(nullptr), mEnvironmentMappingMaterial(nullptr),mCubeMapShaderResourceView(nullptr), mTextureShaderResourceView(nullptr),
		mDepthMapEffect(nullptr), mDepthMapMaterial(nullptr), 
		
		mDepthMap0(nullptr), mDepthMap1(nullptr), mDepthMap2(nullptr),
		mIsDrawDepthMap(false),mDepthBiasState(nullptr), mDepthBias(0), mSlopeScaledDepthBias(2.0f),
		
		mCameraViewFrustums(NUM_FRUSTUMS, XMMatrixIdentity()), mCameraFrustumsDebug(NUM_FRUSTUMS, nullptr), mIsDrawFrustum(false),

		mLightProjectors(NUM_FRUSTUMS, nullptr), mLightProjectorFrustums(NUM_FRUSTUMS, XMMatrixIdentity()),
		mLightProjectorCenteredPositions(NUM_FRUSTUMS, XMFLOAT3{}), mLightProjectorFrustumsDebug(NUM_FRUSTUMS, nullptr),
		mIsVisualizeCascades(false),
		mActiveTechnique(ShadowMappingTechniqueSimplePCF),

		mSkybox(nullptr)

			
	{
	}
	ShadowMappingDemo::~ShadowMappingDemo()
	{
		//delete light data
		DeleteObject(mPointLight);
		DeleteObject(mDirectionalLight)
		DeleteObject(mProxyModel);


		//delete depth data
		DeleteObject(mDepthMap0);
		DeleteObject(mDepthMap1);
		DeleteObject(mDepthMap2);
		ReleaseObject(mDepthBiasState);

		//delete shadow receiver model data
		ReleaseObject(mModelIndexBuffer);
		ReleaseObject(mModelPositionUVNormalVertexBuffer);
		ReleaseObject(mModelPositionVertexBuffer);

		//delete plane model data
		ReleaseObject(mPlaneTexture);
		ReleaseObject(mPlanePositionUVNormalVertexBuffer);
		ReleaseObject(mPlanePositionVertexBuffer);
		ReleaseObject(mPlaneIndexBuffer);

		//delete materials data
		DeleteObject(mShadowMappingDirectionalMaterial);
		DeleteObject(mShadowMappingEffect);

		DeleteObject(mEnvironmentMappingEffect);
		DeleteObject(mEnvironmentMappingMaterial);
		ReleaseObject(mTextureShaderResourceView);
		ReleaseObject(mCubeMapShaderResourceView);
		
		DeleteObject(mDepthMapMaterial);
		DeleteObject(mDepthMapEffect);


		//delete projectors and frustums data
		DeletePointerCollection(mCameraFrustumsDebug);
		DeletePointerCollection(mLightProjectorFrustumsDebug);
		DeletePointerCollection(mLightProjectors);

		DeleteObject(mSkybox);


	}

	/////////////////////////////////////////////////////////////
	// 'DemoLevel' ugly methods...
	bool ShadowMappingDemo::IsComponent()
	{
		return mGame->IsInGameComponents<ShadowMappingDemo*>(mGame->components, this);
	}
	void ShadowMappingDemo::Create()
	{
		Initialize();
		mGame->components.push_back(this);
	}
	void ShadowMappingDemo::Destroy()
	{
		std::pair<bool, int> res = mGame->FindInGameComponents<ShadowMappingDemo*>(mGame->components, this);
	
		if (res.first)
		{
			mGame->components.erase(mGame->components.begin() + res.second);
			
			//very provocative...
			delete this;
		}
	
	
	}
	/////////////////////////////////////////////////////////////


	void ShadowMappingDemo::Initialize()
	{
		SetCurrentDirectory(Utility::ExecutableDirectory().c_str());

		#pragma region INITIALIZE_MATERIALS

		//Shadow Mapping

		//shader
		mShadowMappingEffect = new Effect(*mGame);
		mShadowMappingEffect->CompileFromFile(L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Effects\\ShadowMappingDirectional.fx");

		//material
		mShadowMappingDirectionalMaterial = new ShadowMappingDirectionalMaterial();
		mShadowMappingDirectionalMaterial->Initialize(mShadowMappingEffect);


		//Depth mapping

		//shader
		mDepthMapEffect = new Effect(*mGame);
		mDepthMapEffect->CompileFromFile(L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Effects\\DepthMap.fx");

		//material
		mDepthMapMaterial = new DepthMapMaterial();
		mDepthMapMaterial->Initialize(mDepthMapEffect);

		//Environment Mapping

		//shader
		mEnvironmentMappingEffect = new Effect(*mGame);
		mEnvironmentMappingEffect->CompileFromFile(L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Effects\\EnvironmentMapping.fx");

		//material
		mEnvironmentMappingMaterial = new EnvironmentMappingMaterial();
		mEnvironmentMappingMaterial->Initialize(mEnvironmentMappingEffect);
		
		//load environment texture
		HRESULT hr = DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\Maskonaive2_1024.dds", nullptr, &mCubeMapShaderResourceView);
		if (FAILED(hr))
		{
			throw GameException("CreateDDSTextureFromFile() failed.", hr);
		}
#pragma endregion

		#pragma region INITIALIZE_LIGHT



		//directional light
		mDirectionalLight = new DirectionalLight(*mGame);
		mAmbientColor.a = 55;

		//directional gizmo model
		mProxyModel = new ProxyModel(*mGame, *mCamera, "C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\DirectionalLightProxy.obj", 0.5f);
		mProxyModel->Initialize();
		mProxyModel->ApplyRotation(XMMatrixRotationY(XM_PIDIV2));
		mProxyModel->SetPosition(51.0f, 30.0, 116.0f);
#pragma endregion
		
		#pragma region INITIALIZE_FRUSTUMS



		for (size_t i = 0; i < NUM_FRUSTUMS; i++)
		{
			//initialize camera frustum gizmo for debug
			mCameraFrustumsDebug[i] = new RenderableFrustum(*mGame, *mCamera);
			mCameraFrustumsDebug[i]->Initialize();
			mCameraFrustumsDebug[i]->SetColor((XMFLOAT4)ColorHelper::Red);

			//set matrices to camera view frustums by partitioning with near/far planes
			mCameraViewFrustums[i].SetMatrix(SetMatrixForCustomFrustum(*mGame, *mCamera, i, mCamera->Position(), mCamera->DirectionVector()));
			mCameraFrustumsDebug[i]->InitializeGeometry(mCameraViewFrustums[i]);
			
			//initialize projectors for depth map
			mLightProjectors[i] = new Projector(*mGame);
			mLightProjectors[i]->Initialize();
			mLightProjectors[i]->SetProjectionMatrix(GetProjectionAABB(i, mCameraViewFrustums[i], *mDirectionalLight, *mProxyModel));

			//initialize projector frustums for depth map
			mLightProjectorFrustums[i].SetMatrix(GetProjectionAABB(i, mCameraViewFrustums[i], *mDirectionalLight, *mProxyModel));

			//initialize gizmo of projectors AABB for debug
			mLightProjectorFrustumsDebug[i] = new RenderableFrustum(*mGame, *mCamera);
			mLightProjectorFrustumsDebug[i]->Initialize();
			mLightProjectorFrustumsDebug[i]->InitializeGeometry(mLightProjectorFrustums[i]);
		}
		
		InitializeProjectedTextureScalingMatrix();
#pragma endregion

		#pragma region INITIALIZE_MODELS


		//Load plane model
		std::unique_ptr<Model> plane_model(new Model(*mGame, "C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\default_plane\\default_plane.obj", true));
		
		Mesh* mesh_plane = plane_model->Meshes().at(0);
		mDepthMapMaterial->CreateVertexBuffer(mGame->Direct3DDevice(), *mesh_plane, &mPlanePositionVertexBuffer);
		mShadowMappingDirectionalMaterial->CreateVertexBuffer(mGame->Direct3DDevice(), *mesh_plane, &mPlanePositionUVNormalVertexBuffer);

		mesh_plane->CreateIndexBuffer(&mPlaneIndexBuffer);
		mPlaneVertexCount = mesh_plane->Indices().size();
		//XMStoreFloat4x4(&mPlaneWorldMatrix, XMMatrixScaling(7.0f, 7.0f, 7.0f));


		std::wstring textureName = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\default_plane\\UV_Grid_Lrg.jpg";
		HRESULT hr2 = DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textureName.c_str(), nullptr, &mPlaneTexture);
		if (FAILED(hr2))
		{
			throw GameException("CreateWICTextureFromFile() failed.", hr2);
		}

		//Load dragon models
		std::unique_ptr<Model> model(new Model(*mGame, "C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\dragon50k_array.obj", true));

		Mesh* mesh = model->Meshes().at(0);
		mDepthMapMaterial->CreateVertexBuffer(mGame->Direct3DDevice(), *mesh, &mModelPositionVertexBuffer);
		mEnvironmentMappingMaterial->CreateVertexBuffer(mGame->Direct3DDevice(), *mesh, &mModelPositionUVNormalVertexBuffer);
		mesh->CreateIndexBuffer(&mModelIndexBuffer);
		mModelIndexCount = mesh->Indices().size();
#pragma endregion

		#pragma region INITIALIZE_KEYBOARD

		mKeyboard = (Keyboard*)mGame->Services().GetService(Keyboard::TypeIdClass());
		assert(mKeyboard != nullptr);
#pragma endregion

		#pragma region INITIALIZE_DEPTHMAPS

		//Depth maps for debug
		mDepthMap0 = new DepthMap(*mGame, DepthMapWidth, DepthMapHeight);
		mDepthMap1 = new DepthMap(*mGame, DepthMapWidth, DepthMapHeight);
		mDepthMap2 = new DepthMap(*mGame, DepthMapWidth, DepthMapHeight);
		UpdateDepthBiasState();
#pragma endregion

		#pragma region INITIALIZE_SKYBOX


		//Skybox initialization
		mSkybox = new Skybox(*mGame, *mCamera, L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Textures\\Sky_Type_1.dds", 100.0f);
		mSkybox->Initialize();
#pragma endregion



	}


	void ShadowMappingDemo::Update(const GameTime& gameTime)
	{
		mProxyModel->Update(gameTime);

		UpdateImGui();
		UpdateDepthBias(gameTime);
		UpdateDirectionalLightAndProjector(gameTime);
		
		#pragma region UPDATE_FRUSTUMS_INFO

		std::for_each(mCameraFrustumsDebug.begin(), mCameraFrustumsDebug.end(), [&](RenderableFrustum* a) {a->Update(gameTime); });
		std::for_each(mLightProjectors.begin(), mLightProjectors.end(), [&](Projector* a) {a->Update(gameTime); });
		std::for_each(mLightProjectorFrustumsDebug.begin(), mLightProjectorFrustumsDebug.end(), [&](RenderableFrustum* a) {a->Update(gameTime); });

		for (size_t i = 0; i < NUM_FRUSTUMS; i++)
		{
			//update matrices for camera frustums
			mCameraViewFrustums[i].SetMatrix(SetMatrixForCustomFrustum(*mGame , *mCamera, i, mCamera->Position(), mCamera->DirectionVector()));
			//update camera frustums' gizmos for debug
			mCameraFrustumsDebug[i]->InitializeGeometry(mCameraViewFrustums[i]);

			//update matrices for projectors, AABBs and etc.
			mLightProjectors[i]->SetProjectionMatrix(GetProjectionAABB(i, mCameraViewFrustums[i], *mDirectionalLight, *mProxyModel));
			mLightProjectorFrustums[i].SetMatrix(GetProjectionAABB(i, mCameraViewFrustums[i], *mDirectionalLight, *mProxyModel));
			mLightProjectorFrustumsDebug[i]->InitializeGeometry(mLightProjectorFrustums[i]);

		}

#pragma endregion

	}
	
	void ShadowMappingDemo::Draw(const GameTime& gameTime)
	{

		#pragma region DRAW_DEPTH_MAPS
		mRenderStateHelper.SaveRasterizerState();

		//FIRST DEPTH MAP
		mDepthMap0->Begin();
		
		ID3D11DeviceContext* direct3DDeviceContext0 = mGame->Direct3DDeviceContext();
		direct3DDeviceContext0->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		direct3DDeviceContext0->ClearDepthStencilView(mDepthMap0->DepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		Pass* pass0 = mDepthMapMaterial->CurrentTechnique()->Passes().at(0);
		ID3D11InputLayout* inputLayout0 = mDepthMapMaterial->InputLayouts().at(pass0);
		direct3DDeviceContext0->IASetInputLayout(inputLayout0);

		direct3DDeviceContext0->RSSetState(mDepthBiasState);

		UINT stride0 = mDepthMapMaterial->VertexSize();
		UINT offset0 = 0;
		direct3DDeviceContext0->IASetVertexBuffers(0, 1, &mModelPositionVertexBuffer, &stride0, &offset0);
		direct3DDeviceContext0->IASetIndexBuffer(mModelIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

		XMMATRIX modelWorldMatrix0 = XMLoadFloat4x4(&mModelWorldMatrix);
		mDepthMapMaterial->WorldLightViewProjection() << modelWorldMatrix0 * mLightProjectors[0]->ViewMatrix() * mLightProjectors[0]->ProjectionMatrix();

		pass0->Apply(0, direct3DDeviceContext0);

		direct3DDeviceContext0->DrawIndexed(mModelIndexCount, 0, 0);

		mDepthMap0->End();
		
		//SECOND DEPTH MAP
		mDepthMap1->Begin();

		ID3D11DeviceContext* direct3DDeviceContext1 = mGame->Direct3DDeviceContext();
		direct3DDeviceContext1->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		direct3DDeviceContext1->ClearDepthStencilView(mDepthMap1->DepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		Pass* pass1 = mDepthMapMaterial->CurrentTechnique()->Passes().at(0);
		ID3D11InputLayout* inputLayout1 = mDepthMapMaterial->InputLayouts().at(pass1);
		direct3DDeviceContext1->IASetInputLayout(inputLayout1);

		direct3DDeviceContext1->RSSetState(mDepthBiasState);

		UINT stride1 = mDepthMapMaterial->VertexSize();
		UINT offset1 = 0;
		direct3DDeviceContext1->IASetVertexBuffers(0, 1, &mModelPositionVertexBuffer, &stride1, &offset1);
		direct3DDeviceContext1->IASetIndexBuffer(mModelIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

		XMMATRIX modelWorldMatrix1 = XMLoadFloat4x4(&mModelWorldMatrix);
		mDepthMapMaterial->WorldLightViewProjection() << modelWorldMatrix1 * mLightProjectors[1]->ViewMatrix() * mLightProjectors[1]->ProjectionMatrix();

		pass1->Apply(0, direct3DDeviceContext1);

		direct3DDeviceContext1->DrawIndexed(mModelIndexCount, 0, 0);

		mDepthMap1->End();

		//THIRD DEPTH MAP
		mDepthMap2->Begin();

		ID3D11DeviceContext* direct3DDeviceContext2 = mGame->Direct3DDeviceContext();
		direct3DDeviceContext2->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		direct3DDeviceContext2->ClearDepthStencilView(mDepthMap2->DepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		Pass* pass2 = mDepthMapMaterial->CurrentTechnique()->Passes().at(0);
		ID3D11InputLayout* inputLayout2 = mDepthMapMaterial->InputLayouts().at(pass2);
		direct3DDeviceContext2->IASetInputLayout(inputLayout2);

		direct3DDeviceContext2->RSSetState(mDepthBiasState);

		UINT stride2 = mDepthMapMaterial->VertexSize();
		UINT offset2 = 0;
		direct3DDeviceContext2->IASetVertexBuffers(0, 1, &mModelPositionVertexBuffer, &stride2, &offset2);
		direct3DDeviceContext2->IASetIndexBuffer(mModelIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

		XMMATRIX modelWorldMatrix2 = XMLoadFloat4x4(&mModelWorldMatrix);
		mDepthMapMaterial->WorldLightViewProjection() << modelWorldMatrix2 * mLightProjectors[2]->ViewMatrix() * mLightProjectors[2]->ProjectionMatrix();

		pass2->Apply(0, direct3DDeviceContext2);

		direct3DDeviceContext2->DrawIndexed(mModelIndexCount, 0, 0);

		mDepthMap2->End();

		mRenderStateHelper.RestoreRasterizerState();
		
		// Projective texture mapping directional pass
		pass0 = mShadowMappingDirectionalMaterial->CurrentTechnique()->Passes().at(0);
		inputLayout0 = mShadowMappingDirectionalMaterial->InputLayouts().at(pass0);
		direct3DDeviceContext0->IASetInputLayout(inputLayout0);

		pass1 = mShadowMappingDirectionalMaterial->CurrentTechnique()->Passes().at(0);
		inputLayout1 = mShadowMappingDirectionalMaterial->InputLayouts().at(pass1);
		direct3DDeviceContext1->IASetInputLayout(inputLayout1);

		pass2 = mShadowMappingDirectionalMaterial->CurrentTechnique()->Passes().at(0);
		inputLayout2 = mShadowMappingDirectionalMaterial->InputLayouts().at(pass2);
		direct3DDeviceContext2->IASetInputLayout(inputLayout2);

#pragma endregion

		#pragma region DRAW_MODELS


		//DRAW PLANE MODEL
		stride0 = mShadowMappingDirectionalMaterial->VertexSize();
		direct3DDeviceContext0->IASetVertexBuffers(0, 1, &mPlanePositionUVNormalVertexBuffer, &stride0, &offset0);
		direct3DDeviceContext0->IASetIndexBuffer(mPlaneIndexBuffer, DXGI_FORMAT_R32_UINT, 0);


		XMMATRIX planeWorldMatrix = XMLoadFloat4x4(&mPlaneWorldMatrix);
		XMMATRIX planeWVP = planeWorldMatrix * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();
		XMMATRIX projectiveTextureMatrix[3] = { planeWorldMatrix * mLightProjectors[0]->ViewMatrix() * mLightProjectors[0]->ProjectionMatrix() * XMLoadFloat4x4(&mProjectedTextureScalingMatrix) ,
											    planeWorldMatrix * mLightProjectors[1]->ViewMatrix() * mLightProjectors[1]->ProjectionMatrix() * XMLoadFloat4x4(&mProjectedTextureScalingMatrix) ,
												planeWorldMatrix * mLightProjectors[2]->ViewMatrix() * mLightProjectors[2]->ProjectionMatrix() * XMLoadFloat4x4(&mProjectedTextureScalingMatrix) };
		
		XMVECTOR ambientColor = XMLoadColor(&mAmbientColor);
		XMVECTOR specularColor = XMLoadColor(&mSpecularColor);
		XMVECTOR shadowMapSize = XMVectorSet(static_cast<float>(DepthMapWidth), static_cast<float>(DepthMapHeight), 0.0f, 0.0f);

		XMVECTOR distancesToFarPlanes = {clippingPlanes[0], clippingPlanes[1], clippingPlanes[2] };


		mShadowMappingDirectionalMaterial->WorldViewProjection() << planeWVP;
		mShadowMappingDirectionalMaterial->World() << planeWorldMatrix;
		mShadowMappingDirectionalMaterial->AmbientColor() << ambientColor;

		mShadowMappingDirectionalMaterial->LightColor() << mDirectionalLight->ColorVector();
		mShadowMappingDirectionalMaterial->LightDirection() << mDirectionalLight->DirectionVector();

		mShadowMappingDirectionalMaterial->ColorTexture() << mPlaneTexture;
		mShadowMappingDirectionalMaterial->Distances() << distancesToFarPlanes;
		mShadowMappingDirectionalMaterial->CameraPosition() << mCamera->PositionVector();
		
		mShadowMappingDirectionalMaterial->ProjectiveTextureMatrix0() << projectiveTextureMatrix[0];
		mShadowMappingDirectionalMaterial->ProjectiveTextureMatrix1() << projectiveTextureMatrix[1];
		mShadowMappingDirectionalMaterial->ProjectiveTextureMatrix2() << projectiveTextureMatrix[2];

		mShadowMappingDirectionalMaterial->ShadowMap0() << mDepthMap0->OutputTexture();
		mShadowMappingDirectionalMaterial->ShadowMap1() << mDepthMap1->OutputTexture();
		mShadowMappingDirectionalMaterial->ShadowMap2() << mDepthMap2->OutputTexture();

		mShadowMappingDirectionalMaterial->visualizeCascades() << mIsVisualizeCascades;



		mShadowMappingDirectionalMaterial->ShadowMapSize() << shadowMapSize;

		pass0->Apply(0, direct3DDeviceContext0);

		direct3DDeviceContext0->DrawIndexed(mPlaneVertexCount, 0, 0);
		mGame->UnbindPixelShaderResources(0, 3);


		//DRAW DRAGON MODELS
		//for now just one pass and one material
		const int NUM_PASSES_FOR_DRAGON = 1;
		std::vector<Pass*> passes(NUM_PASSES_FOR_DRAGON);
		for (size_t i = 0; i < NUM_PASSES_FOR_DRAGON; i++)
		{

			passes[i] = mEnvironmentMappingMaterial->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout2 = mEnvironmentMappingMaterial->InputLayouts().at(passes[i]);
			direct3DDeviceContext0->IASetInputLayout(inputLayout2);

			UINT stride2 = mEnvironmentMappingMaterial->VertexSize();
			UINT offset2 = 0;

			direct3DDeviceContext0->IASetVertexBuffers(0, 1, &mModelPositionUVNormalVertexBuffer, &stride2, &offset2);
			direct3DDeviceContext0->IASetIndexBuffer(mModelIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

			XMMATRIX worldMatrix = XMLoadFloat4x4(&mModelWorldMatrices[i]);
			XMMATRIX modelWVP = XMLoadFloat4x4(&mModelWorldMatrices[i]) * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();
			//XMVECTOR ambientColor = XMLoadColor(&mAmbientColor);
			XMVECTOR envColor = modelColorVector;

			mEnvironmentMappingMaterial->WorldViewProjection() << modelWVP;
			mEnvironmentMappingMaterial->World() << modelWorldMatrix0;
			mEnvironmentMappingMaterial->ReflectionAmount() << 1.0f;
			mEnvironmentMappingMaterial->AmbientColor() << ambientColor;
			mEnvironmentMappingMaterial->EnvColor() << envColor;
			mEnvironmentMappingMaterial->CameraPosition() << mCamera->PositionVector();
			mEnvironmentMappingMaterial->ColorTexture() << mTextureShaderResourceView;
			mEnvironmentMappingMaterial->EnvironmentMap() << mCubeMapShaderResourceView;


			passes[i]->Apply(0, direct3DDeviceContext0);

			direct3DDeviceContext0->DrawIndexed(mModelIndexCount, 0, 0);
			mGame->UnbindPixelShaderResources(0, 3);
		}

		//DRAW LIGHT GIZMO MODEL
		mProxyModel->Draw(gameTime);

		//DRAW FRUSUM & AABB GIZMOS
		if (mIsDrawFrustum) 
		{
			std::for_each(mLightProjectorFrustumsDebug.begin(), mLightProjectorFrustumsDebug.end(), [&](RenderableFrustum* a) {a->Draw(gameTime); });
			std::for_each(mCameraFrustumsDebug.begin(), mCameraFrustumsDebug.end(), [&](RenderableFrustum* a) {a->Draw(gameTime); });
		}

		mRenderStateHelper.SaveAll();
#pragma endregion

		#pragma region DRAW_IMGUI
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

#pragma endregion

	}

	/*****************************************UPDATES************************************************/
	void ShadowMappingDemo::UpdateImGui()
	{
		#pragma region LEVEL_SPECIFIC_IMGUI
		ImGui::Begin("Cascaded Shadow Mapping - Demo");

		if (ImGui::CollapsingHeader("Shadow Properties"))
		{
			
			const char* items[] = { "Simple Shadow Mapping", "Cascaded Shadow Mapping" };
			static int item_current = 0;
			ImGui::Combo("Technique", &item_current, items, IM_ARRAYSIZE(items));
			
			mActiveTechnique = ShadowMappingTechnique(item_current);
			mShadowMappingDirectionalMaterial->SetCurrentTechnique(mShadowMappingDirectionalMaterial->GetEffect()->TechniquesByName().at(ShadowMappingTechniqueNames[mActiveTechnique]));
			mDepthMapMaterial->SetCurrentTechnique(mDepthMapMaterial->GetEffect()->TechniquesByName().at(DepthMappingTechniqueNames[mActiveTechnique]));
			
			ImGui::Checkbox("Visualize cascades", &mIsVisualizeCascades);
		}

		if (ImGui::CollapsingHeader("Light Properties"))
		{
			ImGui::SliderInt("Ambient", reinterpret_cast<int*>(&mAmbientColor.a), 0, 100);
			ImGui::TextWrapped("Use arrow keys to rotate directional light.\n\n");

		}

		if (ImGui::CollapsingHeader("Models Properties"))
		{
			ImGui::ColorEdit3("Color", modelColorFloat3);
			modelColorVector = XMLoadColor(&XMCOLOR(modelColorFloat3[0], modelColorFloat3[1], modelColorFloat3[2], 1));
		}

		ImGui::End();
		#pragma endregion
	}
	void ShadowMappingDemo::UpdateDepthBias(const GameTime& gameTime)
	{
		if (mKeyboard != nullptr)
		{
			if (mKeyboard->IsKeyDown(DIK_O))
			{
				mSlopeScaledDepthBias += (float)gameTime.ElapsedGameTime();
				UpdateDepthBiasState();
			}

			if (mKeyboard->IsKeyDown(DIK_P) && mSlopeScaledDepthBias > 0)
			{
				mSlopeScaledDepthBias -= (float)gameTime.ElapsedGameTime();
				mSlopeScaledDepthBias = XMMax(mSlopeScaledDepthBias, 0.0f);
				UpdateDepthBiasState();
			}

			if (mKeyboard->IsKeyDown(DIK_J))
			{
				mDepthBias += DepthBiasModulationRate * (float)gameTime.ElapsedGameTime();
				UpdateDepthBiasState();
			}

			if (mKeyboard->IsKeyDown(DIK_K) && mDepthBias > 0)
			{
				mDepthBias -= DepthBiasModulationRate * (float)gameTime.ElapsedGameTime();
				mDepthBias = XMMax(mDepthBias, 0.0f);
				UpdateDepthBiasState();
			}
		}
	}
	void ShadowMappingDemo::UpdateDepthBiasState()
	{
		ReleaseObject(mDepthBiasState);

		D3D11_RASTERIZER_DESC rasterizerStateDesc;
		ZeroMemory(&rasterizerStateDesc, sizeof(rasterizerStateDesc));
		rasterizerStateDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerStateDesc.CullMode = D3D11_CULL_BACK;
		rasterizerStateDesc.DepthClipEnable = true;
		rasterizerStateDesc.DepthBias = (int)mDepthBias;
		rasterizerStateDesc.SlopeScaledDepthBias = mSlopeScaledDepthBias;

		HRESULT hr = mGame->Direct3DDevice()->CreateRasterizerState(&rasterizerStateDesc, &mDepthBiasState);
		if (FAILED(hr))
		{
			throw GameException("ID3D11Device::CreateRasterizerState() failed.", hr);
		}
	}
	void ShadowMappingDemo::UpdateDirectionalLightAndProjector(const GameTime& gameTime)
	{
		float elapsedTime = (float)gameTime.ElapsedGameTime();

		#pragma region UPDATE_POSITIONS


		for (size_t i = 0; i < NUM_FRUSTUMS; i++)
		{
			mLightProjectors[i]->SetPosition(XMVECTOR{ mLightProjectorCenteredPositions[i].x, mLightProjectorCenteredPositions[i].y , mLightProjectorCenteredPositions[i].z } /*+movement*/);
			mLightProjectorFrustumsDebug[i]->SetPosition(XMVECTOR{ mLightProjectorCenteredPositions[i].x, mLightProjectorCenteredPositions[i].y , mLightProjectorCenteredPositions[i].z } /*+ movement*/);
		}
		#pragma endregion

		#pragma region UPDATE_ROTATIONS

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
			XMMATRIX lightRotationAxisMatrix = XMMatrixRotationAxis(mDirectionalLight->RightVector(), rotationAmount.y);
			lightRotationMatrix *= lightRotationAxisMatrix;
		}
		if (rotationAmount.x != 0.0f || rotationAmount.y != 0.0f)
		{
			mDirectionalLight->ApplyRotation(lightRotationMatrix);
			mProxyModel->ApplyRotation(lightRotationMatrix);
		}
		
		//rotation matrices for frustums and AABBs
		for (size_t i = 0; i < NUM_FRUSTUMS; i++)
		{
			XMMATRIX projectorRotationMatrix = XMMatrixIdentity();
			if (rotationAmount.x != 0)
			{
				projectorRotationMatrix = XMMatrixRotationY(rotationAmount.x);
			}
			if (rotationAmount.y != 0)
			{
				XMMATRIX projectorRotationAxisMatrix = XMMatrixRotationAxis(mLightProjectors[i]->RightVector(), rotationAmount.y);
				projectorRotationMatrix *= projectorRotationAxisMatrix;
			}
			if (rotationAmount.x != Vector2Helper::Zero.x || rotationAmount.y != Vector2Helper::Zero.y)
			{

				mLightProjectors[i]->ApplyRotation(projectorRotationMatrix);
				mLightProjectorFrustumsDebug[i]->ApplyRotation(projectorRotationMatrix);

			}
		}
#pragma endregion

	}
	/************************************************************************************************/
	
	void ShadowMappingDemo::InitializeProjectedTextureScalingMatrix()
	{
		mProjectedTextureScalingMatrix._11 = 0.5f;
		mProjectedTextureScalingMatrix._22 = -0.5f;
		mProjectedTextureScalingMatrix._33 = 1.0f;
		mProjectedTextureScalingMatrix._41 = 0.5f;
		mProjectedTextureScalingMatrix._42 = 0.5f;
		mProjectedTextureScalingMatrix._44 = 1.0f;
	}
	
	XMMATRIX ShadowMappingDemo::SetMatrixForCustomFrustum(Game& game, Camera& camera, int number, XMFLOAT3 pos, XMVECTOR dir)
	{
		XMMATRIX projectionMatrix;

		switch (number)
		{
			case 0:
				projectionMatrix = XMMatrixPerspectiveFovRH(XM_PIDIV4, game.AspectRatio(), -25.0f, clippingPlanes[0]);
				break;
			case 1:
				projectionMatrix = XMMatrixPerspectiveFovRH(XM_PIDIV4, game.AspectRatio(), -25.0f, clippingPlanes[1]);
				break;																	   										   
			case 2:																		   										   
				projectionMatrix = XMMatrixPerspectiveFovRH(XM_PIDIV4, game.AspectRatio(), -25.0f, clippingPlanes[2]);
				break;																	   				    
			default:																	   				    
				projectionMatrix = XMMatrixPerspectiveFovRH(XM_PIDIV4, game.AspectRatio(), -25.0f, clippingPlanes[2]);
				break;


		}
		XMMATRIX viewMatrix = XMMatrixLookToRH(XMVECTOR{ pos.x, pos.y, pos.z }, dir, XMVECTOR{ 0, 1 ,0 });

		return XMMatrixMultiply(viewMatrix, projectionMatrix);
	}
	XMMATRIX ShadowMappingDemo::GetProjectionAABB(int index, Frustum& cameraFrustum, DirectionalLight& light, ProxyModel& positionObject)
	{
		//create corners
		XMFLOAT3 frustumCorners[9] = {};

		frustumCorners[0] = (cameraFrustum.Corners()[0]);
		frustumCorners[1] = (cameraFrustum.Corners()[1]);
		frustumCorners[2] = (cameraFrustum.Corners()[2]);
		frustumCorners[3] = (cameraFrustum.Corners()[3]);
		frustumCorners[4] = (cameraFrustum.Corners()[4]);
		frustumCorners[5] = (cameraFrustum.Corners()[5]);
		frustumCorners[6] = (cameraFrustum.Corners()[6]);
		frustumCorners[7] = (cameraFrustum.Corners()[7]);
		frustumCorners[8] = (cameraFrustum.Corners()[8]);

		XMFLOAT3 frustumCenter = { 0, 0, 0 };

		for (size_t i = 0; i < 8; i++)
		{
			frustumCenter = XMFLOAT3(frustumCenter.x + frustumCorners[i].x,
									 frustumCenter.y + frustumCorners[i].y,
								     frustumCenter.z + frustumCorners[i].z);
		}

		//calculate frustum's center position
		frustumCenter = XMFLOAT3(frustumCenter.x * (1.0f / 8.0f), 
								 frustumCenter.y * (1.0f / 8.0f), 
								 frustumCenter.z * (1.0f / 8.0f) );

		mLightProjectorCenteredPositions[index] = frustumCenter;

		float minX = (std::numeric_limits<float>::max)();
		float maxX = (std::numeric_limits<float>::min)();
		float minY = (std::numeric_limits<float>::max)();
		float maxY = (std::numeric_limits<float>::min)();
		float minZ = (std::numeric_limits<float>::max)();
		float maxZ = (std::numeric_limits<float>::min)();

		for (int j = 0; j < 8; j++) {

			// Transform the frustum coordinate from world to light space
			XMVECTOR frustumCornerVector = XMLoadFloat3(&frustumCorners[j]);
			frustumCornerVector = XMVector3Transform(frustumCornerVector, (light.LightMatrix(frustumCenter)));

			XMStoreFloat3(&frustumCorners[j], frustumCornerVector);

			minX = min(minX, frustumCorners[j].x);
			maxX = max(maxX, frustumCorners[j].x);
			minY = min(minY, frustumCorners[j].y);
			maxY = max(maxY, frustumCorners[j].y);
			minZ = min(minZ, frustumCorners[j].z);
			maxZ = max(maxZ, frustumCorners[j].z);
		}

		//set orthographic proj with proper boundaries
		XMMATRIX projectionMatrix = XMMatrixOrthographicLH(maxX - minX, maxY - minY,  maxZ , minZ );
		return projectionMatrix;
	}
}