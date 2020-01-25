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

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <WICTextureLoader.h>
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
		mPostProcessingStack(nullptr), 
		mSceneRT(nullptr),
		mColorGradingRT(nullptr),
		mSponzaLightingRenderingObject(nullptr),
		mSponzaShadowRenderingObject(nullptr)
	{
	}

	SponzaMainDemo::~SponzaMainDemo()
	{
		mSponzaLightingRenderingObject->MeshMaterialVariablesUpdateEvent->RemoverAllListeners();
		mSponzaShadowRenderingObject->MeshMaterialVariablesUpdateEvent->RemoverAllListeners();
		DeleteObject(mSponzaLightingRenderingObject);
		DeleteObject(mSponzaShadowRenderingObject);

		DeleteObject(mRenderStateHelper);
		DeleteObject(mShadowMap);
		DeleteObject(mShadowProjector);
		ReleaseObject(mShadowRasterizerState);
		DeleteObject(mDirectionalLight);
		DeleteObject(mProxyModel);
		DeleteObject(mSkybox);
		DeleteObject(mPostProcessingStack);
		//DeleteObject(mSceneRT);
		//DeleteObject(mColorGradingRT);
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

		mSponzaLightingRenderingObject = new RenderingObject("Statue Light", *mGame, std::unique_ptr<Model>(new Model(*mGame, Utility::GetFilePath("content\\models\\Sponza\\sponza.fbx"), true)));
		mSponzaShadowRenderingObject = new RenderingObject("Statue Shadow", *mGame, std::unique_ptr<Model>(new Model(*mGame, Utility::GetFilePath("content\\models\\Sponza\\sponza.fbx"), true)));

		// Initialize the material - lighting
		Effect* lightingEffect = new Effect(*mGame);
		lightingEffect->CompileFromFile(Utility::GetFilePath(L"content\\effects\\StandardLighting.fx"));

		// Initialize the material - shadow
		Effect* effectShadow = new Effect(*mGame);
		effectShadow->CompileFromFile(Utility::GetFilePath(L"content\\effects\\DepthMap.fx"));

		mSponzaLightingRenderingObject->LoadMaterial(new StandardLightingMaterial(), lightingEffect);
		mSponzaLightingRenderingObject->LoadRenderBuffers();

		mSponzaShadowRenderingObject->LoadMaterial(new DepthMapMaterial(), effectShadow);
		mSponzaShadowRenderingObject->LoadRenderBuffers();

		mKeyboard = (Keyboard*)mGame->Services().GetService(Keyboard::TypeIdClass());
		assert(mKeyboard != nullptr);

		mSkybox = new Skybox(*mGame, *mCamera, Utility::GetFilePath(L"content\\textures\\Sky_Type_4.dds"), 100);
		mSkybox->Initialize();

		//directional light
		mDirectionalLight = new DirectionalLight(*mGame);

		//directional gizmo model
		mProxyModel = new ProxyModel(*mGame, *mCamera, Utility::GetFilePath("content\\models\\DirectionalLightProxy.obj"), 0.5f);
		mProxyModel->Initialize();
		mProxyModel->ApplyRotation(XMMatrixRotationY(XM_PIDIV2));
		mProxyModel->SetPosition(0.0f, 500.0, 0.0f);

		mRenderStateHelper = new RenderStateHelper(*mGame);

		mSponzaLightingRenderingObject->MeshMaterialVariablesUpdateEvent->AddListener("Standard Lighting Material Update", [&](int meshIndex) { UpdateStandardLightingMaterialVariables(meshIndex); });
		mSponzaShadowRenderingObject->MeshMaterialVariablesUpdateEvent->AddListener("Shadow Map Material Update", [&](int meshIndex) { UpdateDepthMapMaterialVariables(meshIndex); });

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

		//shadows
		mShadowProjector = new Projector(*mGame);
		mShadowProjector->Initialize();

		mShadowMap = new DepthMap(*mGame, 4096, 4096);

		mProxyModel->ApplyRotation(XMMatrixRotationX(-XMConvertToRadians(70.0f)) * XMMatrixRotationY(-XMConvertToRadians(25.0f)));
		mDirectionalLight->ApplyRotation(XMMatrixRotationAxis(mDirectionalLight->RightVector(), -XMConvertToRadians(70.0f)) * XMMatrixRotationAxis(mDirectionalLight->UpVector(), -XMConvertToRadians(25.0f)));
		mShadowProjector->ApplyRotation(XMMatrixRotationAxis(mShadowProjector->RightVector(), -XMConvertToRadians(70.0f)) * XMMatrixRotationAxis(mDirectionalLight->UpVector(), -XMConvertToRadians(25.0f)));

		mCamera->SetPosition(XMFLOAT3(-76.6f, 8.4f, 8.8f));
		mCamera->ApplyRotation(XMMatrixRotationAxis(mCamera->RightVector(), XMConvertToRadians(18.0f)) * XMMatrixRotationAxis(mCamera->UpVector(), -XMConvertToRadians(70.0f)));
	
		mSceneRT = new FullScreenRenderTarget(*mGame);
		mColorGradingRT = new FullScreenRenderTarget(*mGame);

		mPostProcessingStack = new PostProcessingStack(*mGame, *mCamera);
		mPostProcessingStack->Initialize(true, true, false);
		mPostProcessingStack->SetColorGradingRT(mSceneRT);
		mPostProcessingStack->SetVignetteRT(mColorGradingRT);
	}

	void SponzaMainDemo::Update(const GameTime& gameTime)
	{
		UpdateImGui();
		mShadowProjector->Update(gameTime);
		mProxyModel->Update(gameTime);
		mSkybox->Update(gameTime);
		mPostProcessingStack->Update();

		UpdateDirectionalLightAndProjector(gameTime);

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

		ImGui::Separator();

		ImGui::Checkbox("Show Post Processing Stack", &mPostProcessingStack->isWindowOpened);
		if (mPostProcessingStack->isWindowOpened)
			mPostProcessingStack->ShowPostProcessingWindow();

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
		mSponzaShadowRenderingObject->Draw();
		mShadowMap->End();
		mRenderStateHelper->RestoreRasterizerState();
		#pragma endregion

		#pragma region DRAW_SCENE
		
		mSceneRT->Begin();
		mGame->Direct3DDeviceContext()->ClearRenderTargetView(mSceneRT->RenderTargetView(), clear_color);
		mGame->Direct3DDeviceContext()->ClearDepthStencilView(mSceneRT->DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0);

		//skybox
		mSkybox->Draw(gameTime);

		//lighting
		mSponzaLightingRenderingObject->Draw();

		//gizmo
		mProxyModel->Draw(gameTime);
		
		mSceneRT->End();
		#pragma endregion

		#pragma region DRAW_POST_EFFECTS

		//color grading
		mColorGradingRT->Begin();
		mGame->Direct3DDeviceContext()->ClearRenderTargetView(mColorGradingRT->RenderTargetView(), clear_color);
		mGame->Direct3DDeviceContext()->ClearDepthStencilView(mColorGradingRT->DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0);
		mPostProcessingStack->DrawColorGrading(gameTime);
		mColorGradingRT->End();

		//vignette
		mPostProcessingStack->DrawVignette(gameTime);

		#pragma endregion

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
		XMMATRIX modelToShadowMatrix = XMMatrixIdentity() * mShadowProjector->ViewMatrix() * mShadowProjector->ProjectionMatrix() * XMLoadFloat4x4(&projectedShadowMatrixTransform);

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
		static_cast<StandardLightingMaterial*>(mSponzaLightingRenderingObject->GetMeshMaterial())->SpecularTexture() << mSponzaLightingRenderingObject->GetTextureData(meshIndex).RoughnessMap;
		static_cast<StandardLightingMaterial*>(mSponzaLightingRenderingObject->GetMeshMaterial())->ShadowTexture() << mShadowMap->OutputTexture();

	}

	void SponzaMainDemo::UpdateDepthMapMaterialVariables(int meshIndex)
	{
		XMMATRIX wlvp = XMMatrixIdentity() * mShadowProjector->ViewMatrix() * mShadowProjector->ProjectionMatrix();
		static_cast<DepthMapMaterial*>(mSponzaShadowRenderingObject->GetMeshMaterial())->WorldLightViewProjection() << wlvp;
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
}