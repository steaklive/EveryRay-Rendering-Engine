#include "stdafx.h"

#include "SubsurfaceScatteringDemo.h"
#include "..\Library\InstancingMaterial.h"
#include "..\Library\AmbientLightingMaterial.h"
#include "..\Library\SkinShadingColorMaterial.h"
#include "..\Library\SkinShadingScatterMaterial.h"
#include "..\Library\DepthMapMaterial.h"
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
#include "..\Library\DepthMap.h"
#include "..\Library\FullScreenQuad.h"
#include "..\Library\FullScreenRenderTarget.h"




#include "DDSTextureLoader.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"


#include <WICTextureLoader.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <sstream>
#include <functional>


namespace Rendering
{
	RTTI_DEFINITIONS(SubsurfaceScatteringDemo)

	const XMVECTORF32 BackgroundColor = ColorHelper::Black;

	static int currentLight = 0;

	SubsurfaceScatteringDemoLightInfo::LightData currentLightSource;
	SubsurfaceScatteringDemoLightInfo::LightData lightSource1;
	SubsurfaceScatteringDemoLightInfo::LightData lightSource2;
	SubsurfaceScatteringDemoLightInfo::LightData lightSource3;


	SubsurfaceScatteringDemo::SubsurfaceScatteringDemo(Game& game, Camera& camera)
		: 
		DrawableGameComponent(game, camera),

		mKeyboard(nullptr),

		mFrustum(XMMatrixIdentity()),
		mDebugFrustum(nullptr),

		mSkybox(nullptr),
		mDepthMap(nullptr),
		mDepthBiasState(nullptr), mDepthBias(0), mSlopeScaledDepthBias(2.0f),
		mDepthMapMaterial(nullptr),

		mDefaultPlaneObject(nullptr),
		mHeadObject(nullptr),

		mRenderTarget(nullptr),
		mQuad(nullptr),

		mShadowMapVertexBuffer(nullptr),

		mWorldMatrix(MatrixHelper::Identity),
		mRenderStateHelper(nullptr)
	{
	}

	SubsurfaceScatteringDemo::~SubsurfaceScatteringDemo()
	{
		DeleteObject(mRenderStateHelper);
		DeleteObject(mDefaultPlaneObject);
		DeleteObject(mHeadObject);

		DeleteObject(mSkybox);
		DeleteObject(mDepthMap);
		DeleteObject(mDepthMapMaterial);

		DeleteObject(mRenderTarget);
		DeleteObject(mQuad);

		ReleaseObject(mDepthBiasState);


		currentLightSource.Destroy();
		lightSource1.Destroy();
		lightSource2.Destroy();
		lightSource3.Destroy();

	}

	/////////////////////////////////////////////////////////////
	// 'DemoLevel' ugly methods...
	bool SubsurfaceScatteringDemo::IsComponent()
	{
		return mGame->IsInGameComponents<SubsurfaceScatteringDemo*>(mGame->components, this);
	}
	void SubsurfaceScatteringDemo::Create()
	{
		Initialize();
		mGame->components.push_back(this);
	}
	void SubsurfaceScatteringDemo::Destroy()
	{
		std::pair<bool, int> res = mGame->FindInGameComponents<SubsurfaceScatteringDemo*>(mGame->components, this);

		if (res.first)
		{
			mGame->components.erase(mGame->components.begin() + res.second);

			//very provocative...
			delete this;
		}

	}
	/////////////////////////////////////////////////////////////

	void SubsurfaceScatteringDemo::Initialize()
	{
		SetCurrentDirectory(Utility::ExecutableDirectory().c_str());

		/*#pragma region DEFAULT_PLANE_OBJECT_INITIALIZATION
		{
			mDefaultPlaneObject = new SubsurfaceScatteringDemoStructs::DefaultPlaneObject();

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
		}
#pragma endregion*/

		#pragma region HEAD_OBJECT_INITIALIZATION
		{
			mHeadObject = new SubsurfaceScatteringDemoStructs::HeadObject();

			//Head LPS model
			std::unique_ptr<Model> model_head(new Model(*mGame, "C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\lpshead\\lpshead.fbx", true));

			// Load the skin color effect
			Effect* skinColorEffect = new Effect(*mGame);
			skinColorEffect->CompileFromFile(L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Effects\\SkinShadingColor.fx");

			mHeadObject->SkinColorMaterial = new SkinShadingColorMaterial();
			mHeadObject->SkinColorMaterial->Initialize(skinColorEffect);

			Mesh* mesh_head = model_head->Meshes().at(0);
			mHeadObject->SkinColorMaterial->CreateVertexBuffer(mGame->Direct3DDevice(), *mesh_head, &mHeadObject->VertexBuffer);
			mesh_head->CreateIndexBuffer(&mHeadObject->IndexBuffer);
			mHeadObject->IndexCount = mesh_head->Indices().size();

			// load diffuse texture
			std::wstring textureName4 = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\lpshead\\DiffuseMap.dds";
			if (FAILED(DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textureName4.c_str(), nullptr, &mHeadObject->DiffuseTexture)))
			{
				throw GameException("Failed to load head's 'Diffuse Texture'.");
			}

			// load normal texture
			std::wstring textureName5 = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\lpshead\\NormalMap.dds";
			if (FAILED(DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textureName5.c_str(), nullptr, &mHeadObject->NormalTexture)))
			{
				throw GameException("Failed to load head's 'Normal Texture'.");
			}

			// load specular ao texture
			std::wstring textureName6 = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\lpshead\\spec.jpg";
			if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textureName6.c_str(), nullptr, &mHeadObject->SpecularAOTexture)))
			{
				throw GameException("Failed to load head's 'Specular AO Texture'.");
			}

			// load beckmann texture
			std::wstring textureName7 = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\lpshead\\BeckmannMap.dds";
			if (FAILED(DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textureName7.c_str(), nullptr, &mHeadObject->BeckmannTexture)))
			{
				throw GameException("Failed to load head's 'Beckmann Texture'.");
			}

			// load irradiance texture
			std::wstring textureName8 = L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\lpshead\\IrradianceMap.dds";
			if (FAILED(DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), textureName8.c_str(), nullptr, &mHeadObject->IrradianceTexture)))
			{
				throw GameException("Failed to load head's 'Irradiance Texture'.");
			}
		}

#pragma endregion

		//current light initiazlization
		currentLightSource.DirectionalLight = new DirectionalLight(*mGame);
		currentLightSource.ProxyModel = new ProxyModel(*mGame, *mCamera, "C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\DirectionalLightProxy.obj", 0.5f);
		currentLightSource.ProxyModel->Initialize();
		currentLightSource.ProxyModel->ApplyRotation(XMMatrixRotationY(XM_PIDIV2));
		currentLightSource.ProxyModel->SetPosition(0, 0, 0);

		//light 1 initialization
		lightSource1.DirectionalLight = new DirectionalLight(*mGame);
		lightSource1.ProxyModel = new ProxyModel(*mGame, *mCamera, "C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\DirectionalLightProxy.obj", 0.5f);
		lightSource1.ProxyModel->Initialize();
		lightSource1.ProxyModel->SetPosition(-2.0, 12.0f, 5.0f);
		
		lightSource1.DirectionalLight->ApplyRotation(XMMatrixRotationX(-XM_PI / 7)*XMMatrixRotationY(-XM_PI / 8));
		lightSource1.ProxyModel->ApplyRotation(XMMatrixRotationY(XM_PIDIV2)*XMMatrixRotationX(-XM_PI / 7)*XMMatrixRotationY(-XM_PI / 8));
		
		lightSource1.FarPlane = 10.0f;
		lightSource1.NearPlane = 0.0f;
		lightSource1.SpotLightAngle = 80.0f;


		//light 2 initialization
		lightSource2.DirectionalLight = new DirectionalLight(*mGame);
		lightSource2.ProxyModel = new ProxyModel(*mGame, *mCamera, "C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\DirectionalLightProxy.obj", 0.5f);
		lightSource2.ProxyModel->Initialize();
		lightSource2.ProxyModel->SetPosition(3.5, 9.0f, -6.0f);

		lightSource2.DirectionalLight->ApplyRotation(XMMatrixRotationY(XM_PI));
		lightSource2.ProxyModel->ApplyRotation(XMMatrixRotationY(XM_PIDIV2)*XMMatrixRotationY(XM_PI));

		lightSource2.FarPlane = 5.5f;
		lightSource2.NearPlane = 0.0f;
		lightSource2.SpotLightAngle = 90.0f;

		//light 3 initialization
		lightSource3.DirectionalLight = new DirectionalLight(*mGame);
		lightSource3.ProxyModel = new ProxyModel(*mGame, *mCamera, "C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\DirectionalLightProxy.obj", 0.5f);
		lightSource3.ProxyModel->Initialize();
		lightSource3.ProxyModel->SetPosition(0.0, 3.0f, 5.0f);

		lightSource3.DirectionalLight->ApplyRotation(XMMatrixRotationX(XM_PI/4));
		lightSource3.ProxyModel->ApplyRotation(XMMatrixRotationY(XM_PIDIV2)*XMMatrixRotationX(XM_PI/4));

		lightSource3.FarPlane = 10.0f;
		lightSource3.NearPlane = 0.0f;
		lightSource3.SpotLightAngle = 90.0f;


		mKeyboard = (Keyboard*)mGame->Services().GetService(Keyboard::TypeIdClass());
		assert(mKeyboard != nullptr);


		//Skybox initialization
		mSkybox = new Skybox(*mGame, *mCamera, L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\lpshead\\IrradianceMap.dds", 100.0f);
		mSkybox->Initialize();
		mSkybox->SetVisible(false);


		mDepthMap = new DepthMap(*mGame, 2048, 2048);
		UpdateDepthBiasState();

		//shader depth
		Effect* depthMapEffect = new Effect(*mGame);
		depthMapEffect->CompileFromFile(L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Effects\\DepthMap.fx");

		//material
		mDepthMapMaterial = new DepthMapMaterial();
		mDepthMapMaterial->Initialize(depthMapEffect);
		//mDepthMapMaterial->SetCurrentTechnique(mDepthMapMaterial.Get);
		std::unique_ptr<Model> model_head(new Model(*mGame, "C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Models\\lpshead\\lpshead.fbx", true));
		Mesh* mesh_head = model_head->Meshes().at(0);

		mDepthMapMaterial->CreateVertexBuffer(mGame->Direct3DDevice(), *mesh_head, &mShadowMapVertexBuffer);



		mRenderTarget = new FullScreenRenderTarget(*mGame);

		//shader subsurface scattering
		Effect* skinScatterEffect = new Effect(*mGame);
		skinScatterEffect->CompileFromFile(L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Effects\\SkinShadingSeparableSubsurface.fx");


		mHeadObject->SkinScatterMaterial = new SkinShadingScatterMaterial();
		mHeadObject->SkinScatterMaterial->Initialize(skinScatterEffect);


		mQuad = new FullScreenQuad(*mGame, *mHeadObject->SkinScatterMaterial);
		mQuad->Initialize();
		

		mRenderStateHelper = new RenderStateHelper(*mGame);
	}

	void SubsurfaceScatteringDemo::Update(const GameTime& gameTime)
	{
		UpdateLight();
		UpdateImGui();

		currentLightSource.ProxyModel->Update(gameTime);

	}

	void SubsurfaceScatteringDemo::UpdateLight()
	{
		switch (currentLight)
		{
		case 0:
			currentLightSource = lightSource1;
			break;
		case 1:
			currentLightSource = lightSource2;
			break;
		case 2:
			currentLightSource = lightSource3;
			break;
		}
	}

	void SubsurfaceScatteringDemo::UpdateImGui()
	{
#pragma region LEVEL_SPECIFIC_IMGUI
		ImGui::Begin("Separable Subsurface Scattering - Demo");

		ImGui::Separator();
		
		ImGui::TextWrapped("SSS settings");
		ImGui::Checkbox("Enable SSS", &mIsSSSEnabled);
		ImGui::Checkbox("Enable SSS blur", &mIsSSSBlurEnabled);
		ImGui::Checkbox("Follow Surface", &mIsSSSFollowSurface);
		ImGui::SliderFloat("SSS width", &mSSSWidth, 0.0f, 0.03f);
		ImGui::SliderFloat("SSS strength", &mSSSStrength, 0.0f, 1.0f);

		
		ImGui::Separator();
		
		ImGui::TextWrapped("Current light source");
		ImGui::RadioButton("Light 1", &currentLight, 0);
		ImGui::RadioButton("Light 2", &currentLight, 1);
		ImGui::RadioButton("Light 3", &currentLight, 2);
		
		ImGui::Separator();

		ImGui::TextWrapped("Shading settings");
		ImGui::SliderFloat("Ambient Factor", &mAmbientFactor, 0.0f, 1.0f);
		ImGui::SliderFloat("Attenuation", &mAttenuation, 0.0f, 256.0f);
		ImGui::SliderFloat("Bumpiness", &mBumpiness, 0.0f, 1.0f);
		ImGui::SliderFloat("Specular Intensity", &mSpecularIntensity, 0.0f, 3.0f);
		ImGui::SliderFloat("Specular Roughness", &mSpecularRoughness, 0.0f, 1.0f);
		ImGui::SliderFloat("Specular Fresnel", &mSpecularFresnel, 0.0f, 1.0f);
		
		ImGui::End();
#pragma endregion
	}

	void SubsurfaceScatteringDemo::Draw(const GameTime& gameTime)
	{
		ID3D11DeviceContext* direct3DDeviceContext = mGame->Direct3DDeviceContext();
		direct3DDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		//depth map
		{
			mDepthMap->Begin();

			//ID3D11DeviceContext* direct3DDeviceContext0 = mGame->Direct3DDeviceContext();
			//direct3DDeviceContext0->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			direct3DDeviceContext->ClearDepthStencilView(mDepthMap->DepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
			Pass* pass0 = mDepthMapMaterial->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout0 = mDepthMapMaterial->InputLayouts().at(pass0);
			direct3DDeviceContext->IASetInputLayout(inputLayout0);

			direct3DDeviceContext->RSSetState(mDepthBiasState);

			UINT stride0 = mDepthMapMaterial->VertexSize();
			UINT offset0 = 0;
			direct3DDeviceContext->IASetVertexBuffers(0, 1, &mShadowMapVertexBuffer, &stride0, &offset0);
			direct3DDeviceContext->IASetIndexBuffer(mHeadObject->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

			XMMATRIX lightProjMatrix = XMMatrixPerspectiveFovRH(currentLightSource.SpotLightAngle * 3.14f / 180.f, 1.0f, currentLightSource.NearPlane, currentLightSource.FarPlane);
			XMMATRIX lightViewMatrix = XMMatrixLookToRH(currentLightSource.ProxyModel->PositionVector(), currentLightSource.DirectionalLight->DirectionVector(), currentLightSource.DirectionalLight->UpVector());

			XMMATRIX modelWorldMatrix0 = XMMatrixIdentity();


			mDepthMapMaterial->WorldLightViewProjection() << modelWorldMatrix0 * lightViewMatrix * lightProjMatrix;

			pass0->Apply(0, direct3DDeviceContext);

			direct3DDeviceContext->DrawIndexed(mHeadObject->IndexCount, 0, 0);

			mDepthMap->End();
		}

		if (mIsSSSBlurEnabled)
		{
			mRenderTarget->Begin();
			//
			direct3DDeviceContext->ClearDepthStencilView(mRenderTarget->DepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
			direct3DDeviceContext->ClearRenderTargetView(mRenderTarget->RenderTargetView(), reinterpret_cast<const float*>(&BackgroundColor));
		}
		// Draw Head
		{
			Pass* pass = mHeadObject->SkinColorMaterial->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mHeadObject->SkinColorMaterial->InputLayouts().at(pass);
			direct3DDeviceContext->IASetInputLayout(inputLayout);

			UINT stride = mHeadObject->SkinColorMaterial->VertexSize();
			UINT offset = 0;
			direct3DDeviceContext->IASetVertexBuffers(0, 1, &mHeadObject->VertexBuffer, &stride, &offset);
			direct3DDeviceContext->IASetIndexBuffer(mHeadObject->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

			XMMATRIX worldMatrix = XMLoadFloat4x4(&mWorldMatrix);
			XMMATRIX wvp = worldMatrix * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();

			XMMATRIX lightProjMatrix = XMMatrixPerspectiveFovRH(currentLightSource.SpotLightAngle * 3.14f / 180.f, 1.0f, currentLightSource.NearPlane, currentLightSource.FarPlane);
			XMMATRIX lightViewMatrix = XMMatrixLookToRH(currentLightSource.ProxyModel->PositionVector(), currentLightSource.DirectionalLight->DirectionVector(), currentLightSource.DirectionalLight->UpVector());

			XMMATRIX scale = XMMatrixScaling(0.5f, -0.5f, 1.0f);
			XMMATRIX translation =  XMMatrixTranslation(0.5f, 0.5f, 0.0f);

			XMMATRIX lightViewProj = lightViewMatrix * lightProjMatrix *  scale * translation;

			XMFLOAT4 color = { 0.9, 0.9, 0.9, 1.0 };
			XMVECTOR ambientColor = XMLoadFloat4(&color);

			mHeadObject->SkinColorMaterial->cameraPosition() << mCamera->PositionVector();
			mHeadObject->SkinColorMaterial->lightposition() << currentLightSource.ProxyModel->PositionVector();
			mHeadObject->SkinColorMaterial->lightdirection() << currentLightSource.DirectionalLight->DirectionVector();
			mHeadObject->SkinColorMaterial->lightfalloffStart() << (float)cos(0.5f * currentLightSource.SpotLightAngle * 3.14f / 180.f);
			mHeadObject->SkinColorMaterial->lightfalloffWidth() << 0.05f;
			mHeadObject->SkinColorMaterial->lightcolor() << ambientColor;
			mHeadObject->SkinColorMaterial->lightattenuation() << 1.0f / mAttenuation;
			mHeadObject->SkinColorMaterial->farPlane() << currentLightSource.FarPlane;
			mHeadObject->SkinColorMaterial->bias() << mShadowBias;
			mHeadObject->SkinColorMaterial->lightviewProjection() << lightViewProj;
			mHeadObject->SkinColorMaterial->currWorldViewProj() << wvp;;
			mHeadObject->SkinColorMaterial->world() << worldMatrix;
			mHeadObject->SkinColorMaterial->id() << (int)1;
			mHeadObject->SkinColorMaterial->bumpiness() << mBumpiness;
			mHeadObject->SkinColorMaterial->specularIntensity() << mSpecularIntensity;
			mHeadObject->SkinColorMaterial->specularRoughness() << mSpecularRoughness;
			mHeadObject->SkinColorMaterial->specularFresnel() << mSpecularFresnel;
			mHeadObject->SkinColorMaterial->translucency() << 0.997f;
			mHeadObject->SkinColorMaterial->sssEnabled() << mIsSSSEnabled;
			mHeadObject->SkinColorMaterial->sssWidth() << mSSSWidth;
			mHeadObject->SkinColorMaterial->ambient() << mAmbientFactor;

			mHeadObject->SkinColorMaterial->shadowMap()		<< mDepthMap->OutputTexture();
			mHeadObject->SkinColorMaterial->diffuseTex()	<< mHeadObject->DiffuseTexture;
			mHeadObject->SkinColorMaterial->normalTex()		<< mHeadObject->NormalTexture;
			mHeadObject->SkinColorMaterial->specularAOTex() << mHeadObject->SpecularAOTexture;
			mHeadObject->SkinColorMaterial->beckmannTex()	<< mHeadObject->BeckmannTexture;
			mHeadObject->SkinColorMaterial->irradianceTex() << mHeadObject->IrradianceTexture;
			mHeadObject->SkinColorMaterial->ProjectiveTextureMatrix() << lightViewProj;

			pass->Apply(0, direct3DDeviceContext);

			direct3DDeviceContext->DrawIndexed(mHeadObject->IndexCount, 0, 0);

		}
		
		if (mIsSSSBlurEnabled)
		{
			mRenderTarget->End();
			
			//horizontal pass
			mQuad->SetActiveTechnique("SSS", "SSSSBlurX");
			mQuad->SetCustomUpdateMaterial(std::bind(&SubsurfaceScatteringDemo::UpdateScatterMaterial, this, 0));
			mQuad->Draw(gameTime);

			//vertical pass
			mQuad->SetActiveTechnique("SSS", "SSSSBlurY");
			mQuad->SetCustomUpdateMaterial(std::bind(&SubsurfaceScatteringDemo::UpdateScatterMaterial, this, 1));
			mQuad->Draw(gameTime);
		}

		//mRenderStateHelper->SaveAll();
		mRenderStateHelper->RestoreAll();

		#pragma region DRAW_IMGUI
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

#pragma endregion
	}

	void SubsurfaceScatteringDemo::UpdateDepthBiasState()
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

	void SubsurfaceScatteringDemo::UpdateScatterMaterial(int side)
	{

		mHeadObject->SkinScatterMaterial->ColorTexture() << mRenderTarget->OutputColorTexture();
		mHeadObject->SkinScatterMaterial->DepthTexture() << mDepthMap->OutputTexture();
		mHeadObject->SkinScatterMaterial->id() << 1;
		mHeadObject->SkinScatterMaterial->sssWidth() << mSSSWidth;
		mHeadObject->SkinScatterMaterial->sssStrength() << mSSSStrength;
		mHeadObject->SkinScatterMaterial->followSurface() << mIsSSSFollowSurface;


		//pass X
		if (side == 0)
			mHeadObject->SkinScatterMaterial->dir() << XMVECTOR{ 1.0f, 0.0f };
		//pass Y
		else
			mHeadObject->SkinScatterMaterial->dir() << XMVECTOR{ 0.0f, 1.0f };
		
	}


}