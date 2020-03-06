#include "stdafx.h"

#include "SponzaMainDemo.h"
#include "..\Library\Game.h"
#include "..\Library\GameException.h"
#include "..\Library\VectorHelper.h"
#include "..\Library\MatrixHelper.h"
#include "..\Library\ColorHelper.h"
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
#include "..\Library\DepthMapMaterial.h"
#include "..\Library\PostProcessingStack.h"
#include "..\Library\FullScreenRenderTarget.h"
#include "..\Library\IBLRadianceMap.h"

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

	const float SponzaMainDemo::AmbientModulationRate = UCHAR_MAX;
	const XMFLOAT2 SponzaMainDemo::LightRotationRate = XMFLOAT2(XM_PI / 2, XM_PI / 2);

	SponzaMainDemo::SponzaMainDemo(Game& game, Camera& camera)
		: DrawableGameComponent(game, camera),
		mKeyboard(nullptr),
		mWorldMatrix(MatrixHelper::Identity),
		mRenderStateHelper(nullptr), 
		mShadowMap(nullptr), 
		mShadowProjector(nullptr),
		mShadowRasterizerState(nullptr),
		mDirectionalLight(nullptr),
		mProxyModel(nullptr),
		mLightFrustum(XMMatrixIdentity()),
		mSkybox(nullptr),
		mSponzaLightingRenderingObject(nullptr), mSpherePBRObject(nullptr),
		mIrradianceTextureSRV(nullptr),
		mRadianceTextureSRV(nullptr),
		mIntegrationMapTextureSRV(nullptr),
		mGrid(nullptr)
	{
	}

	SponzaMainDemo::~SponzaMainDemo()
	{
		mSponzaLightingRenderingObject->MeshMaterialVariablesUpdateEvent->RemoverAllListeners();
		DeleteObject(mSponzaLightingRenderingObject);
		mSpherePBRObject->MeshMaterialVariablesUpdateEvent->RemoverAllListeners();
		DeleteObject(mSpherePBRObject);

		DeleteObject(mRenderStateHelper);
		DeleteObject(mShadowMap);
		DeleteObject(mShadowProjector);
		DeleteObject(mDirectionalLight);
		DeleteObject(mProxyModel);
		DeleteObject(mSkybox);
		DeleteObject(mGrid);
		DeleteObject(mPostProcessingStack);

		ReleaseObject(mShadowRasterizerState);
		
		ReleaseObject(mIrradianceTextureSRV);
		ReleaseObject(mRadianceTextureSRV);
		ReleaseObject(mIntegrationMapTextureSRV);
	}
#pragma region COMPONENT_METHODS
	/////////////////////////////////////////////////////////////
	// 'DemoLevel' ugly methods...
	bool SponzaMainDemo::IsComponent()
	{
		return mGame->IsInGameComponents<SponzaMainDemo*>(mGame->components, this);
	}
	void SponzaMainDemo::Create()
	{
		Initialize();
		mGame->components.push_back(this);
	}
	void SponzaMainDemo::Destroy()
	{
		std::pair<bool, int> res = mGame->FindInGameComponents<SponzaMainDemo*>(mGame->components, this);

		if (res.first)
		{
			mGame->components.erase(mGame->components.begin() + res.second);

			//very provocative...
			delete this;
		}

	}
	/////////////////////////////////////////////////////////////  
#pragma endregion
	void SponzaMainDemo::Initialize()
	{
		SetCurrentDirectory(Utility::ExecutableDirectory().c_str());

		mSponzaLightingRenderingObject = new RenderingObject("Statue Light", *mGame, *mCamera, std::unique_ptr<Model>(new Model(*mGame, Utility::GetFilePath("content\\models\\Sponza\\sponza.fbx"), true)), false);
		mSpherePBRObject = new RenderingObject("PBR Sphere 1", *mGame, *mCamera, std::unique_ptr<Model>(new Model(*mGame, Utility::GetFilePath("content\\models\\sphere.fbx"), true)), true);

		// Initialize the material - lighting
		Effect* lightingEffect = new Effect(*mGame);
		lightingEffect->CompileFromFile(Utility::GetFilePath(L"content\\effects\\StandardLighting.fx"));

		// Initialize the material - shadow
		Effect* effectShadow = new Effect(*mGame);
		effectShadow->CompileFromFile(Utility::GetFilePath(L"content\\effects\\DepthMap.fx"));

		mSponzaLightingRenderingObject->LoadMaterial(new StandardLightingMaterial(), lightingEffect);
		mSponzaLightingRenderingObject->LoadMaterial(new DepthMapMaterial(), effectShadow);
		mSponzaLightingRenderingObject->LoadRenderBuffers();
		mSponzaLightingRenderingObject->GetMeshMaterial()->SetCurrentTechnique(mSponzaLightingRenderingObject->GetMeshMaterial()->GetEffect()->TechniquesByName().at("standard_lighting_pbr"));
		mSponzaLightingRenderingObject->MeshMaterialVariablesUpdateEvent->AddListener("Standard Lighting Material Update", [&](int meshIndex) { UpdateStandardLightingMaterialVariables(meshIndex); });
		mSponzaLightingRenderingObject->MeshMaterialVariablesUpdateEvent->AddListener("Shadow Map Material Update", [&](int meshIndex) { UpdateDepthMapMaterialVariables(meshIndex); });
		
		mSpherePBRObject->LoadMaterial(new StandardLightingMaterial(), lightingEffect);
		mSpherePBRObject->LoadMaterial(new DepthMapMaterial(), effectShadow);
		mSpherePBRObject->LoadRenderBuffers();
		mSpherePBRObject->GetMeshMaterial()->SetCurrentTechnique(mSpherePBRObject->GetMeshMaterial()->GetEffect()->TechniquesByName().at("standard_lighting_pbr"));
		mSpherePBRObject->LoadCustomMeshTextures(0,
			Utility::GetFilePath(L"content\\textures\\PBR\\Gold\\GoldMetal_albedo.jpg"),
			Utility::GetFilePath(L"content\\textures\\PBR\\Gold\\GoldMetal_nrm.jpg"),
			Utility::GetFilePath(L""),
			Utility::GetFilePath(L"content\\textures\\PBR\\Gold\\GoldMetal_rgh.jpg"),
			Utility::GetFilePath(L"content\\textures\\PBR\\Gold\\GoldMetal_mtl.jpg"),
			Utility::GetFilePath(L""),
			Utility::GetFilePath(L""),
			Utility::GetFilePath(L"")
		);
		mSpherePBRObject->MeshMaterialVariablesUpdateEvent->AddListener("Standard Lighting PBR Material Update", [&](int meshIndex) { UpdateStandardLightingPBRMaterialVariables(meshIndex); });
		mSpherePBRObject->MeshMaterialVariablesUpdateEvent->AddListener("Shadow Map Material Update", [&](int meshIndex) { UpdateDepthMapMaterialPBRVariables(meshIndex); });


		mKeyboard = (Keyboard*)mGame->Services().GetService(Keyboard::TypeIdClass());
		assert(mKeyboard != nullptr);

		mSkybox = new Skybox(*mGame, *mCamera, Utility::GetFilePath(L"content\\textures\\Sky_Type_4.dds"), 100);
		mSkybox->Initialize();

		mGrid = new Grid(*mGame, *mCamera, 100, 64, XMFLOAT4(0.961f, 0.871f, 0.702f, 1.0f));
		mGrid->Initialize();
		mGrid->SetColor((XMFLOAT4)ColorHelper::LightGray);

		//directional light
		mDirectionalLight = new DirectionalLight(*mGame);

		//directional gizmo model
		mProxyModel = new ProxyModel(*mGame, *mCamera, Utility::GetFilePath("content\\models\\DirectionalLightProxy.obj"), 0.5f);
		mProxyModel->Initialize();
		mProxyModel->ApplyRotation(XMMatrixRotationY(XM_PIDIV2));
		mProxyModel->SetPosition(0.0f, 500.0, 0.0f);

		mRenderStateHelper = new RenderStateHelper(*mGame);

		D3D11_RASTERIZER_DESC rasterizerStateDesc;
		ZeroMemory(&rasterizerStateDesc, sizeof(rasterizerStateDesc));
		rasterizerStateDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerStateDesc.CullMode = D3D11_CULL_BACK;
		rasterizerStateDesc.DepthClipEnable = true;
		rasterizerStateDesc.DepthBias = 0.05f;
		rasterizerStateDesc.SlopeScaledDepthBias = 3.0f;
		rasterizerStateDesc.FrontCounterClockwise = false;
		HRESULT hr = mGame->Direct3DDevice()->CreateRasterizerState(&rasterizerStateDesc, &mShadowRasterizerState);
		if (FAILED(hr))
		{
			throw GameException("ID3D11Device::CreateRasterizerState() failed.", hr);
		}


		mPostProcessingStack = new PostProcessingStack(*mGame, *mCamera);
		mPostProcessingStack->Initialize(true, false, true, true, true);

		//shadows
		mShadowProjector = new Projector(*mGame);
		mShadowProjector->Initialize();
		mShadowMap = new DepthMap(*mGame, 4096, 4096);

		mProxyModel->ApplyRotation(XMMatrixRotationX(-XMConvertToRadians(70.0f)) * XMMatrixRotationY(-XMConvertToRadians(25.0f)));
		mDirectionalLight->ApplyRotation(XMMatrixRotationAxis(mDirectionalLight->RightVector(), -XMConvertToRadians(70.0f)) * XMMatrixRotationAxis(mDirectionalLight->UpVector(), -XMConvertToRadians(25.0f)));
		mShadowProjector->ApplyRotation(XMMatrixRotationAxis(mShadowProjector->RightVector(), -XMConvertToRadians(70.0f)) * XMMatrixRotationAxis(mDirectionalLight->UpVector(), -XMConvertToRadians(25.0f)));

		mCamera->SetPosition(XMFLOAT3(-76.6f, 8.4f, 8.8f));
		mCamera->ApplyRotation(XMMatrixRotationAxis(mCamera->RightVector(), XMConvertToRadians(18.0f)) * XMMatrixRotationAxis(mCamera->UpVector(), -XMConvertToRadians(70.0f)));
	
	
		//***********************************//
		//			Initialize IBL			 //
		//***********************************//
		//									 //
		// Load Pre-Convoluted Irradiance Map
		if (FAILED(DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), Utility::GetFilePath(L"content\\textures\\PBR\\Skyboxes\\sponzaCubeDiffuseHDR.dds").c_str(), nullptr, &mIrradianceTextureSRV)))
			throw GameException("Failed to create Irradiance Map.");

		// Create an IBL Radiance Map from Environment Map
		mIBLRadianceMap.reset(new IBLRadianceMap(*mGame, Utility::GetFilePath(L"content\\textures\\PBR\\Skyboxes\\sponzaCubemap.dds")));
		mIBLRadianceMap->Initialize();
		mIBLRadianceMap->Create(*mGame);

		mRadianceTextureSRV = *mIBLRadianceMap->GetShaderResourceViewAddress();
		if (mRadianceTextureSRV == nullptr)
			throw GameException("Failed to create Radiance Map.");
		mIBLRadianceMap.release();
		mIBLRadianceMap.reset(nullptr);

		// Load a pre-computed Integration Map
		if (FAILED(DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), Utility::GetFilePath(L"content\\textures\\PBR\\Skyboxes\\sponzaCubeBrdf.dds").c_str(), nullptr, &mIntegrationMapTextureSRV)))
			throw GameException("Failed to create Integration Texture.");
		//									 //
		//***********************************//
		//									 //
		//***********************************//
	}

	void SponzaMainDemo::Update(const GameTime& gameTime)
	{
		UpdateImGui();
		mShadowProjector->Update(gameTime);
		mProxyModel->Update(gameTime);
		mSkybox->Update(gameTime);
		mGrid->Update(gameTime);
		mSpherePBRObject->Update(gameTime);

		UpdateDirectionalLightAndProjector(gameTime);

		mPostProcessingStack->Update();

		XMMATRIX projectionMatrix = XMMatrixPerspectiveFovRH(XM_PIDIV4, mGame->AspectRatio(), 0.01f, 500.0f);
		XMMATRIX viewMatrix = XMMatrixLookToRH(mProxyModel->PositionVector(), mDirectionalLight->DirectionVector(), mDirectionalLight->UpVector());
		mLightFrustum.SetMatrix(XMMatrixMultiply(viewMatrix, projectionMatrix));
		mShadowProjector->SetProjectionMatrix(GetProjectionMatrixFromFrustum(mLightFrustum, *mDirectionalLight));
	}

	void SponzaMainDemo::UpdateImGui()
	{
		ImGui::Begin("Sponza Demo Scene");

		ImGui::SliderFloat3("Sun Color", mSunColor, 0.0f, 1.0f);
		ImGui::SliderFloat3("Ambient Color", mAmbientColor, 0.0f, 1.0f);

		ImGui::Checkbox("Show Post Processing Stack", &mPostProcessingStack->isWindowOpened);
		if (mPostProcessingStack->isWindowOpened) mPostProcessingStack->ShowPostProcessingWindow();

		ImGui::Separator();
		mSpherePBRObject->UpdateGizmos();

		ImGui::End();
	}
	void SponzaMainDemo::UpdateDirectionalLightAndProjector(const GameTime& gameTime)
	{
		float elapsedTime = (float)gameTime.ElapsedGameTime();

		#pragma region UPDATE_POSITIONS

		mShadowProjector->SetPosition(XMVECTOR{ mLightFrustumCenter.x,mLightFrustumCenter.y , mLightFrustumCenter.z } /*+movement*/);
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

		XMMATRIX lightMatrix = mDirectionalLight->LightMatrix(XMFLOAT3(0, 0, 0));

		//rotation matrices for frustums and AABBs
		for (size_t i = 0; i < 1; i++)
		{
			XMMATRIX projectorRotationMatrix = XMMatrixIdentity();
			if (rotationAmount.x != 0)
			{
				projectorRotationMatrix = XMMatrixRotationY(rotationAmount.x);
			}
			if (rotationAmount.y != 0)
			{
				XMMATRIX projectorRotationAxisMatrix = XMMatrixRotationAxis(mShadowProjector->RightVector(), rotationAmount.y);
				projectorRotationMatrix *= projectorRotationAxisMatrix;
			}
			if (rotationAmount.x != Vector2Helper::Zero.x || rotationAmount.y != Vector2Helper::Zero.y)
			{
				mShadowProjector->ApplyRotation(projectorRotationMatrix);
			}
		}
#pragma endregion

	}

	void SponzaMainDemo::Draw(const GameTime& gameTime)
	{
		float clear_color[4] = { 0.0f, 1.0f, 1.0f, 0.0f };

		mRenderStateHelper->SaveRasterizerState();

		ID3D11DeviceContext* direct3DDeviceContext = mGame->Direct3DDeviceContext();
		direct3DDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		#pragma region DRAW_SHADOWS
		//shadow
		mShadowMap->Begin();
		direct3DDeviceContext->ClearDepthStencilView(mShadowMap->DepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		direct3DDeviceContext->RSSetState(mShadowRasterizerState);
		mSponzaLightingRenderingObject->Draw(1, true);
		mSpherePBRObject->Draw(1, true);
		mShadowMap->End();
		mRenderStateHelper->RestoreRasterizerState();
		#pragma endregion

		mPostProcessingStack->Begin();

		#pragma region DRAW_SCENE
		
		//skybox
		mSkybox->Draw(gameTime);

		//grid
		if (Utility::IsEditorMode)
			mGrid->Draw(gameTime);

		//lighting
		mSponzaLightingRenderingObject->Draw(0);

		//PBR Sphere
		mSpherePBRObject->Draw(0);

		//gizmo
		mProxyModel->Draw(gameTime);
		#pragma endregion

		mPostProcessingStack->End(gameTime);

		mRenderStateHelper->SaveAll();

		#pragma region DRAW_IMGUI
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

#pragma endregion
	}

	void SponzaMainDemo::UpdateStandardLightingMaterialVariables(int meshIndex)
	{
		XMMATRIX worldMatrix = XMLoadFloat4x4(&mWorldMatrix);
		XMMATRIX wvp = worldMatrix * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();

		XMFLOAT4X4 projectedShadowMatrixTransform = MatrixHelper::Zero;
		projectedShadowMatrixTransform._11 = 0.5f;
		projectedShadowMatrixTransform._22 = -0.5f;
		projectedShadowMatrixTransform._33 = 1.0f;
		projectedShadowMatrixTransform._41 = 0.5f;
		projectedShadowMatrixTransform._42 = 0.5f;
		projectedShadowMatrixTransform._44 = 1.0f;
		XMMATRIX modelToShadowMatrix = worldMatrix * mShadowProjector->ViewMatrix() * mShadowProjector->ProjectionMatrix() * XMLoadFloat4x4(&projectedShadowMatrixTransform);

		static_cast<StandardLightingMaterial*>(mSponzaLightingRenderingObject->GetMeshMaterial())->WorldViewProjection() << wvp;
		static_cast<StandardLightingMaterial*>(mSponzaLightingRenderingObject->GetMeshMaterial())->World() << worldMatrix;
		static_cast<StandardLightingMaterial*>(mSponzaLightingRenderingObject->GetMeshMaterial())->ModelToShadow() << modelToShadowMatrix;
		static_cast<StandardLightingMaterial*>(mSponzaLightingRenderingObject->GetMeshMaterial())->CameraPosition() << mCamera->PositionVector();
		
		static_cast<StandardLightingMaterial*>(mSponzaLightingRenderingObject->GetMeshMaterial())->SunDirection() << XMVectorNegate(mDirectionalLight->DirectionVector());
		static_cast<StandardLightingMaterial*>(mSponzaLightingRenderingObject->GetMeshMaterial())->SunColor() << XMVECTOR{ mSunColor[0],mSunColor[1], mSunColor[2] , 1.0f };
		static_cast<StandardLightingMaterial*>(mSponzaLightingRenderingObject->GetMeshMaterial())->AmbientColor() << XMVECTOR{ mAmbientColor[0], mAmbientColor[1], mAmbientColor[2], 1.0f };
		static_cast<StandardLightingMaterial*>(mSponzaLightingRenderingObject->GetMeshMaterial())->ShadowTexelSize() << XMVECTOR{ 1.0f / 4096.0f, 1.0f, 1.0f , 1.0f };
		
		static_cast<StandardLightingMaterial*>(mSponzaLightingRenderingObject->GetMeshMaterial())->AlbedoTexture() << mSponzaLightingRenderingObject->GetTextureData(meshIndex).AlbedoMap;
		static_cast<StandardLightingMaterial*>(mSponzaLightingRenderingObject->GetMeshMaterial())->NormalTexture() << mSponzaLightingRenderingObject->GetTextureData(meshIndex).NormalMap;
		static_cast<StandardLightingMaterial*>(mSponzaLightingRenderingObject->GetMeshMaterial())->SpecularTexture() << mSponzaLightingRenderingObject->GetTextureData(meshIndex).SpecularMap;
		static_cast<StandardLightingMaterial*>(mSponzaLightingRenderingObject->GetMeshMaterial())->RoughnessTexture() << mSponzaLightingRenderingObject->GetTextureData(meshIndex).RoughnessMap;
		static_cast<StandardLightingMaterial*>(mSponzaLightingRenderingObject->GetMeshMaterial())->MetallicTexture() << mSponzaLightingRenderingObject->GetTextureData(meshIndex).MetallicMap;
		static_cast<StandardLightingMaterial*>(mSponzaLightingRenderingObject->GetMeshMaterial())->ShadowTexture() << mShadowMap->OutputTexture();
		static_cast<StandardLightingMaterial*>(mSponzaLightingRenderingObject->GetMeshMaterial())->IrradianceTexture() << mIrradianceTextureSRV;
		static_cast<StandardLightingMaterial*>(mSponzaLightingRenderingObject->GetMeshMaterial())->RadianceTexture() << mRadianceTextureSRV;
		static_cast<StandardLightingMaterial*>(mSponzaLightingRenderingObject->GetMeshMaterial())->IntegrationTexture() << mIntegrationMapTextureSRV;
	}	
	void SponzaMainDemo::UpdateStandardLightingPBRMaterialVariables(int meshIndex)
	{
		XMMATRIX worldMatrix = XMLoadFloat4x4(&(mSpherePBRObject->GetTransformMatrix()));
		XMMATRIX wvp = worldMatrix * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();

		XMFLOAT4X4 projectedShadowMatrixTransform = MatrixHelper::Zero;
		projectedShadowMatrixTransform._11 = 0.5f;
		projectedShadowMatrixTransform._22 = -0.5f;
		projectedShadowMatrixTransform._33 = 1.0f;
		projectedShadowMatrixTransform._41 = 0.5f;
		projectedShadowMatrixTransform._42 = 0.5f;
		projectedShadowMatrixTransform._44 = 1.0f;
		XMMATRIX modelToShadowMatrix = worldMatrix * mShadowProjector->ViewMatrix() * mShadowProjector->ProjectionMatrix() * XMLoadFloat4x4(&projectedShadowMatrixTransform);

		static_cast<StandardLightingMaterial*>(mSpherePBRObject->GetMeshMaterial())->WorldViewProjection() << wvp;
		static_cast<StandardLightingMaterial*>(mSpherePBRObject->GetMeshMaterial())->World() << worldMatrix;
		static_cast<StandardLightingMaterial*>(mSpherePBRObject->GetMeshMaterial())->ModelToShadow() << modelToShadowMatrix;
		static_cast<StandardLightingMaterial*>(mSpherePBRObject->GetMeshMaterial())->CameraPosition() << mCamera->PositionVector();
		
		static_cast<StandardLightingMaterial*>(mSpherePBRObject->GetMeshMaterial())->SunDirection() << XMVectorNegate(mDirectionalLight->DirectionVector());
		static_cast<StandardLightingMaterial*>(mSpherePBRObject->GetMeshMaterial())->SunColor() << XMVECTOR{ mSunColor[0],mSunColor[1], mSunColor[2] , 1.0f };
		static_cast<StandardLightingMaterial*>(mSpherePBRObject->GetMeshMaterial())->AmbientColor() << XMVECTOR{ mAmbientColor[0], mAmbientColor[1], mAmbientColor[2], 1.0f };
		static_cast<StandardLightingMaterial*>(mSpherePBRObject->GetMeshMaterial())->ShadowTexelSize() << XMVECTOR{ 1.0f / 4096.0f, 1.0f, 1.0f , 1.0f };
		
		static_cast<StandardLightingMaterial*>(mSpherePBRObject->GetMeshMaterial())->AlbedoTexture() << mSpherePBRObject->GetTextureData(meshIndex).AlbedoMap;
		static_cast<StandardLightingMaterial*>(mSpherePBRObject->GetMeshMaterial())->NormalTexture() << mSpherePBRObject->GetTextureData(meshIndex).NormalMap;
		static_cast<StandardLightingMaterial*>(mSpherePBRObject->GetMeshMaterial())->SpecularTexture() << mSpherePBRObject->GetTextureData(meshIndex).SpecularMap;
		static_cast<StandardLightingMaterial*>(mSpherePBRObject->GetMeshMaterial())->RoughnessTexture() << mSpherePBRObject->GetTextureData(meshIndex).RoughnessMap;
		static_cast<StandardLightingMaterial*>(mSpherePBRObject->GetMeshMaterial())->MetallicTexture() << mSpherePBRObject->GetTextureData(meshIndex).MetallicMap;
		static_cast<StandardLightingMaterial*>(mSpherePBRObject->GetMeshMaterial())->ShadowTexture() << mShadowMap->OutputTexture();
		static_cast<StandardLightingMaterial*>(mSpherePBRObject->GetMeshMaterial())->IrradianceTexture() << mIrradianceTextureSRV;
		static_cast<StandardLightingMaterial*>(mSpherePBRObject->GetMeshMaterial())->RadianceTexture() << mRadianceTextureSRV;
		static_cast<StandardLightingMaterial*>(mSpherePBRObject->GetMeshMaterial())->IntegrationTexture() << mIntegrationMapTextureSRV;
	}

	void SponzaMainDemo::UpdateDepthMapMaterialVariables(int meshIndex)
	{
		XMMATRIX wlvp = XMMatrixIdentity() * mShadowProjector->ViewMatrix() * mShadowProjector->ProjectionMatrix();
		static_cast<DepthMapMaterial*>(mSponzaLightingRenderingObject->GetMaterials()[1])->WorldLightViewProjection() << wlvp;
	}

	void SponzaMainDemo::UpdateDepthMapMaterialPBRVariables(int meshIndex)
	{
		XMMATRIX worldMatrix = XMLoadFloat4x4(&(mSpherePBRObject->GetTransformMatrix()));
		XMMATRIX wlvp = worldMatrix * mShadowProjector->ViewMatrix() * mShadowProjector->ProjectionMatrix();
		static_cast<DepthMapMaterial*>(mSpherePBRObject->GetMaterials()[1])->WorldLightViewProjection() << wlvp;
	}

	XMMATRIX SponzaMainDemo::GetProjectionMatrixFromFrustum(Frustum& cameraFrustum, DirectionalLight& light)
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
			frustumCenter.z * (1.0f / 8.0f));

		mLightFrustumCenter = frustumCenter;

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
		return XMMatrixOrthographicLH(maxX - minX, maxY - minY, maxZ, minZ);
	}

	//void SponzaMainDemo::CheckMouseIntersections()
	//{
	//	float pointX, pointY;
	//	XMMATRIX viewMatrix, inverseViewMatrix, worldMatrix, translateMatrix, inverseWorldMatrix;
	//	XMFLOAT3 direction, origin, rayOrigin, rayDirection;
	//	bool intersect, result;
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