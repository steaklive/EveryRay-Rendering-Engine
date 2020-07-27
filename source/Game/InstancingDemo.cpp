// DEPRECATED!!!

#include "stdafx.h"

#include "InstancingDemo.h"
#include "..\Library\InstancingMaterial.h"
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
#include "..\Library\RenderingObject.h"
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
	RTTI_DEFINITIONS(InstancingDemo)

	InstancingDemoLightInfo::LightData light0;
	InstancingDemoLightInfo::LightData light1;
	InstancingDemoLightInfo::LightData light2;
	InstancingDemoLightInfo::LightData light3;

	InstancingDemo::InstancingDemo(Game& game, Camera& camera)
		: DrawableGameComponent(game, camera),
		mInstanceCount(0),
		mKeyboard(nullptr),
		mProxyModel0(nullptr),
		mProxyModel1(nullptr),
		mProxyModel2(nullptr),
		mProxyModel3(nullptr),
		mWorldMatrix(MatrixHelper::Identity),
		mRenderStateHelper(nullptr)
	{
	}

	InstancingDemo::~InstancingDemo()
	{
		mStatueRenderingObject->MeshMaterialVariablesUpdateEvent->RemoverAllListeners();

		DeleteObject(mRenderStateHelper);

		light0.Destroy();
		light1.Destroy();
		light2.Destroy();
		light3.Destroy();

		DeleteObject(mProxyModel0);
		DeleteObject(mProxyModel1);
		DeleteObject(mProxyModel2);
		DeleteObject(mProxyModel3);

		DeleteObject(mStatueRenderingObject);
	}

	// 'DemoLevel' ugly methods...
	bool InstancingDemo::IsComponent()
	{
		return mGame->IsInGameComponents<InstancingDemo*>(mGame->components, this);
	}
	void InstancingDemo::Create()
	{
		Initialize();
		mGame->components.push_back(this);
	}
	void InstancingDemo::Destroy()
	{
		std::pair<bool, int> res = mGame->FindInGameComponents<InstancingDemo*>(mGame->components, this);

		if (res.first)
		{
			mGame->components.erase(mGame->components.begin() + res.second);

			//very provocative...
			delete this;
		}
	}


	void InstancingDemo::Initialize()
	{
		SetCurrentDirectory(Utility::ExecutableDirectory().c_str());

		// Load the model
		mStatueRenderingObject = new RenderingObject("Statue", *mGame, *mCamera, std::unique_ptr<Model>(new Model(*mGame, Utility::GetFilePath("content\\models\\statue\\statue.fbx"), true)));

		// Load the effect
		Effect* effect = new Effect(*mGame);
		effect->CompileFromFile(Utility::GetFilePath(L"content\\effects\\Instancing.fx"));

		mStatueRenderingObject->LoadMaterial(new InstancingMaterial(), effect, "instancing");
		mStatueRenderingObject->LoadRenderBuffers();

		// Load Instance buffers
		std::vector<InstancingMaterial::InstancedData> instanceData;
		UINT axisInstanceCount = 5;
		float offset = 15.0f;
		float zOffset = ((axisInstanceCount - 1)*offset) / 2.0f;

		// Set positions for every instance
		for (UINT x = 0; x < axisInstanceCount; x++)
		{
			float xPosition = x * offset;

			for (UINT z = 0; z < axisInstanceCount; z++)
			{
				float zPosition = z * offset;
				instanceData.push_back(InstancingMaterial::InstancedData(XMMatrixTranslation(-xPosition, 0, -zPosition + zOffset)));
				instanceData.push_back(InstancingMaterial::InstancedData(XMMatrixTranslation(xPosition, 0, -zPosition + zOffset)));
			}
		}

		mStatueRenderingObject->LoadInstanceBuffers(instanceData, "instancing");
		
		// Setup lights data
		light0.pointLight = new PointLight(*mGame);
		light0.pointLight->SetRadius(light0.lightRadius);
		light0.pointLight->SetPosition(0.0f, 0.0f, 0.0f);
		light0.lightColor[0] = 1.0f;
		light0.lightColor[1] = 0.3f;
		light0.lightColor[2] = 0.4f;
		light0.pointLight->SetColor(light0.lightColor[0], light0.lightColor[1], light0.lightColor[2], 1.0f);
		light0.height = 5.0f;
		light0.orbitRadius = 15.0f;
		light0.movementSpeed = 1.0f;

		light1.pointLight = new PointLight(*mGame);
		light1.pointLight->SetRadius(light1.lightRadius);
		light1.pointLight->SetPosition(0.0f, 0.0f, 0.0f);
		light1.lightColor[0] = 0.8f;
		light1.lightColor[1] = 0.3f;
		light1.lightColor[2] = 0.1f;
		light1.pointLight->SetColor(light1.lightColor[0], light1.lightColor[1], light1.lightColor[2], 1.0f);
		light1.height = 15.0f;
		light1.orbitRadius = 35.0f;
		light1.movementSpeed = 1.5f;

		light2.pointLight = new PointLight(*mGame);
		light2.pointLight->SetRadius(light2.lightRadius);
		light2.pointLight->SetPosition(0.0f, 0.0f, 0.0f);
		light2.lightColor[0] = 0.2f;
		light2.lightColor[1] = 0.3f;
		light2.lightColor[2] = 1.0f;
		light2.pointLight->SetColor(light2.lightColor[0], light2.lightColor[1], light2.lightColor[2], 1.0f);
		light2.height = 25.0f;
		light2.orbitRadius = 55.0f;
		light2.movementSpeed = 1.2f;

		light3.pointLight = new PointLight(*mGame);
		light3.pointLight->SetRadius(light3.lightRadius);
		light3.pointLight->SetPosition(0.0f, 0.0f, 0.0f);
		light3.lightColor[0] = 0.7f;
		light3.lightColor[1] = 0.4f;
		light3.lightColor[2] = 0.6f;
		light3.pointLight->SetColor(light3.lightColor[0], light3.lightColor[1], light3.lightColor[2], 1.0f);
		light3.height = 35.0f;
		light3.orbitRadius = 65.0f;
		light3.movementSpeed = 1.4f;

		mProxyModel0 = new ProxyModel(*mGame, *mCamera, Utility::GetFilePath("content\\models\\PointLightProxy.obj"), 0.5f);
		mProxyModel0->Initialize();

		mProxyModel1 = new ProxyModel(*mGame, *mCamera, Utility::GetFilePath("content\\models\\PointLightProxy.obj"), 0.5f);
		mProxyModel1->Initialize();

		mProxyModel2 = new ProxyModel(*mGame, *mCamera, Utility::GetFilePath("content\\models\\PointLightProxy.obj"), 0.5f);
		mProxyModel2->Initialize();

		mProxyModel3 = new ProxyModel(*mGame, *mCamera, Utility::GetFilePath("content\\models\\PointLightProxy.obj"), 0.5f);
		mProxyModel3->Initialize();

		mKeyboard = (Keyboard*)mGame->Services().GetService(Keyboard::TypeIdClass());
		assert(mKeyboard != nullptr);

		mRenderStateHelper = new RenderStateHelper(*mGame);
		mStatueRenderingObject->MeshMaterialVariablesUpdateEvent->AddListener("Instancing Material Update", [&](int meshIndex) { UpdateInstancingMaterialVariables(meshIndex); });
	}

	void InstancingDemo::Update(const GameTime& gameTime)
	{
		UpdatePointLight(gameTime);

		mProxyModel0->Update(gameTime);
		mProxyModel1->Update(gameTime);
		mProxyModel2->Update(gameTime);
		mProxyModel3->Update(gameTime);

		UpdateImGui();
	}
	
	void InstancingDemo::UpdateImGui()
	{
		#pragma region LEVEL_SPECIFIC_IMGUI
		ImGui::Begin("GPU Instancing - Demo");

		if (ImGui::CollapsingHeader("Light 0 Properties"))
		{
			//static float radius = light0.lightRadius;
			ImGui::SliderFloat("Light Radius", &light0.lightRadius, 0, 500);
			light0.pointLight->SetRadius(light0.lightRadius);

			ImGui::SliderFloat("Orbit Radius", &light0.orbitRadius, 1.0, 100);

			ImGui::SliderFloat("Height", &light0.height, 1.0, 60.0);

			ImGui::SliderFloat("Movement Speed", &light0.movementSpeed, 1.0, 10.0);

			ImGui::ColorEdit3("Color", light0.lightColor);
			light0.pointLight->SetColor(XMCOLOR(light0.lightColor[0], light0.lightColor[1], light0.lightColor[2], 1));

		}
		if (ImGui::CollapsingHeader("Light 1 Properties"))
		{
			ImGui::SliderFloat("Light Radius", &light1.lightRadius, 0, 500);
			light1.pointLight->SetRadius(light1.lightRadius);

			ImGui::SliderFloat("Orbit Radius", &light1.orbitRadius, 1.0, 100);

			ImGui::SliderFloat("Height", &light1.height, 1.0, 60.0);

			ImGui::SliderFloat("Movement Speed", &light1.movementSpeed, 1.0, 10.0);

			ImGui::ColorEdit3("Color", light1.lightColor);
			light1.pointLight->SetColor(XMCOLOR(light1.lightColor[0], light1.lightColor[1], light1.lightColor[2], 1));


		}
		if (ImGui::CollapsingHeader("Light 2 Properties"))
		{
			ImGui::SliderFloat("Light Radius", &light2.lightRadius, 0, 500);
			light2.pointLight->SetRadius(light2.lightRadius);

			ImGui::SliderFloat("Orbit Radius", &light2.orbitRadius, 1.0, 100);

			ImGui::SliderFloat("Height", &light2.height, 1.0, 60.0);

			ImGui::SliderFloat("Movement Speed", &light2.movementSpeed, 1.0, 10.0);


			ImGui::ColorEdit3("Color", light2.lightColor);
			light2.pointLight->SetColor(XMCOLOR(light2.lightColor[0], light2.lightColor[1], light2.lightColor[2], 1));


		}
		if (ImGui::CollapsingHeader("Light 3 Properties"))
		{
			ImGui::SliderFloat("Light Radius", &light3.lightRadius, 0, 500);
			light3.pointLight->SetRadius(light3.lightRadius);

			ImGui::SliderFloat("Orbit Radius", &light3.orbitRadius, 1.0, 100);

			ImGui::SliderFloat("Height", &light3.height, 1.0, 60.0);

			ImGui::SliderFloat("Movement Speed", &light3.movementSpeed, 1.0, 10.0);

			ImGui::ColorEdit3("Color", light3.lightColor);
			light3.pointLight->SetColor(XMCOLOR(light3.lightColor[0], light3.lightColor[1], light3.lightColor[2], 1));

		}

		ImGui::End();
#pragma endregion
	}
	
	void InstancingDemo::Draw(const GameTime& gameTime)
	{
		ID3D11DeviceContext* direct3DDeviceContext = mGame->Direct3DDeviceContext();
		direct3DDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		mStatueRenderingObject->Draw("instancing", -1, false);

		mProxyModel0->Draw(gameTime);
		mProxyModel1->Draw(gameTime);
		mProxyModel2->Draw(gameTime);
		mProxyModel3->Draw(gameTime);

		mRenderStateHelper->SaveAll();
		mRenderStateHelper->RestoreAll();

		#pragma region DRAW_IMGUI
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

#pragma endregion
	}

	void InstancingDemo::UpdateInstancingMaterialVariables(int meshIndex)
	{
		static_cast<InstancingMaterial*>(mStatueRenderingObject->GetMaterials()["instancing"])->ViewProjection() << mCamera->ViewMatrix() * mCamera->ProjectionMatrix();
		static_cast<InstancingMaterial*>(mStatueRenderingObject->GetMaterials()["instancing"])->LightColor0() << light0.pointLight->ColorVector();
		static_cast<InstancingMaterial*>(mStatueRenderingObject->GetMaterials()["instancing"])->LightPosition0() << light0.pointLight->PositionVector();
		static_cast<InstancingMaterial*>(mStatueRenderingObject->GetMaterials()["instancing"])->LightColor1() << light1.pointLight->ColorVector();
		static_cast<InstancingMaterial*>(mStatueRenderingObject->GetMaterials()["instancing"])->LightPosition1() << light1.pointLight->PositionVector();
		static_cast<InstancingMaterial*>(mStatueRenderingObject->GetMaterials()["instancing"])->LightColor2() << light2.pointLight->ColorVector();
		static_cast<InstancingMaterial*>(mStatueRenderingObject->GetMaterials()["instancing"])->LightPosition2() << light2.pointLight->PositionVector();
		static_cast<InstancingMaterial*>(mStatueRenderingObject->GetMaterials()["instancing"])->LightColor3() << light3.pointLight->ColorVector();
		static_cast<InstancingMaterial*>(mStatueRenderingObject->GetMaterials()["instancing"])->LightPosition3() << light3.pointLight->PositionVector();
		static_cast<InstancingMaterial*>(mStatueRenderingObject->GetMaterials()["instancing"])->LightRadius0() << light0.pointLight->Radius();
		static_cast<InstancingMaterial*>(mStatueRenderingObject->GetMaterials()["instancing"])->LightRadius1() << light1.pointLight->Radius();
		static_cast<InstancingMaterial*>(mStatueRenderingObject->GetMaterials()["instancing"])->LightRadius2() << light2.pointLight->Radius();
		static_cast<InstancingMaterial*>(mStatueRenderingObject->GetMaterials()["instancing"])->LightRadius3() << light3.pointLight->Radius();
		static_cast<InstancingMaterial*>(mStatueRenderingObject->GetMaterials()["instancing"])->ColorTexture() << mStatueRenderingObject->GetTextureData(meshIndex).AlbedoMap;
		static_cast<InstancingMaterial*>(mStatueRenderingObject->GetMaterials()["instancing"])->NormalTexture() << mStatueRenderingObject->GetTextureData(meshIndex).NormalMap;
		static_cast<InstancingMaterial*>(mStatueRenderingObject->GetMaterials()["instancing"])->CameraPosition() << mCamera->PositionVector();
	}

	void InstancingDemo::UpdatePointLight(const GameTime& gameTime)
	{
		float elapsedTime = (float)gameTime.ElapsedGameTime();
		
		light0.pointLight->SetPosition(
			XMFLOAT3(
				light0.orbitRadius*cos((float)gameTime.TotalGameTime()*light0.movementSpeed),
				light0.height,
				light0.orbitRadius*sin((float)gameTime.TotalGameTime()*light0.movementSpeed)
			)
		);

		light1.pointLight->SetPosition(
			XMFLOAT3(
				light1.orbitRadius*cos(-(float)gameTime.TotalGameTime()*light1.movementSpeed),
				light1.height,
				light1.orbitRadius*sin(-(float)gameTime.TotalGameTime()*light1.movementSpeed)
			)
		);

		light2.pointLight->SetPosition(
			XMFLOAT3(
				light2.orbitRadius*cos((float)gameTime.TotalGameTime()*light2.movementSpeed),
				light2.height,
				light2.orbitRadius*sin((float)gameTime.TotalGameTime()*light2.movementSpeed)
			)
		);

		light3.pointLight->SetPosition(
			XMFLOAT3(
				light3.orbitRadius*cos(-(float)gameTime.TotalGameTime()*light3.movementSpeed),
				light3.height,
				light3.orbitRadius*sin(-(float)gameTime.TotalGameTime()*light3.movementSpeed)
			)
		);
		

		mProxyModel0->SetPosition(light0.pointLight->Position());
		mProxyModel1->SetPosition(light1.pointLight->Position());
		mProxyModel2->SetPosition(light2.pointLight->Position());
		mProxyModel3->SetPosition(light3.pointLight->Position());

	}

}