#include "stdafx.h"

#include "RenderingGame.h"

#include <sstream>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include "..\Library\GameException.h"
#include "..\Library\Keyboard.h"
#include "..\Library\Mouse.h"
#include "..\Library\FpsComponent.h"
#include "..\Library\Utility.h"
#include "..\Library\FirstPersonCamera.h"
#include "..\Library\ColorHelper.h"
#include "..\Library\Grid.h"
#include "..\Library\RenderStateHelper.h"
#include "..\Library\RasterizerStates.h"
#include "..\Library\SamplerStates.h"
#include "..\Library\Skybox.h"
#include "..\Library\MatrixHelper.h"
#include "..\Library\PointLightingMaterial.h"
#include "..\Library\FullScreenRenderTarget.h"
#include "..\Library\FullScreenQuad.h"
#include "..\Library\Effect.h"
#include "..\Library\ColorFilteringMaterial.h"
#include "..\Library\DemoLevel.h"
#include "..\Library\PostProcessingStack.h"

// include scenes
#include "SponzaMainDemo.h"
#include "ShadowMappingDemo.h"
#include "PhysicallyBasedRenderingDemo.h"
#include "InstancingDemo.h"
#include "FrustumCullingDemo.h"
#include "SubsurfaceScatteringDemo.h"
#include "VolumetricLightingDemo.h"
#include "CollisionTestDemo.h"
#include "WaterSimulationDemo.h"
#include "TerrainDemo.h"
#include "TestSceneDemo.h"

// include IMGUI
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "ImGuizmo.h"

namespace Rendering
{
	const XMVECTORF32 RenderingGame::BackgroundColor = ColorHelper::Black;
	const XMVECTORF32 RenderingGame::BackgroundColor2 = ColorHelper::Red;
	const float RenderingGame::BrightnessModulationRate = 1.0f;
	
	const char* displayedLevelNames[] =
	{
		"Sponza Demo Scene",
		"Terrain Demo Scene",
		"Physically Based Rendering",
		"Separable Subsurface Scattering",
		"Volumetric Lighting",
		"Collision Detection",
		"Cascaded Shadow Mapping",
		"Water Simulation",
		"TEST_SCENE",
	};

	DemoLevel* demoLevel;
	static int currentLevel = 0;

	static float fov = 60.0f;
	static float movementRate = 10.0f;
	static float nearPlaneDist = 0.5f;
	static float farPlaneDist = 600.0f;
	bool showWorldGrid = false;

	RenderingGame::RenderingGame(HINSTANCE instance, const std::wstring& windowClass, const std::wstring& windowTitle, int showCommand)
		: Game(instance, windowClass, windowTitle, showCommand),
		mDirectInput(nullptr),
		mKeyboard(nullptr),
		mMouse(nullptr),
		mMouseTextPosition(0.0f, 40.0f),
		mShowProfiler(false), 

		//scenes
		mTestSceneDemo(nullptr),
		mSponzaMainDemo(nullptr),
		mShadowMappingDemo(nullptr),
		mPBRDemo(nullptr),
		mInstancingDemo(nullptr),
		mFrustumCullingDemo(nullptr),
		mSubsurfaceScatteringDemo(nullptr),
		mVolumetricLightingDemo(nullptr),
		mCollisionTestDemo(nullptr),
		mWaterSimulationDemo(nullptr),
		mTerrainDemo(nullptr),
		mRenderStateHelper(nullptr)

	{
		mDepthStencilBufferEnabled = true;
		mMultiSamplingEnabled = true;
	}

	RenderingGame::~RenderingGame()
	{
	}

	void RenderingGame::Initialize()
	{
		SetCurrentDirectory(Utility::ExecutableDirectory().c_str());

		RasterizerStates::Initialize(mDirect3DDevice);
		SamplerStates::BorderColor = ColorHelper::Black;
		SamplerStates::Initialize(mDirect3DDevice);

		if (FAILED(DirectInput8Create(mInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (LPVOID*)&mDirectInput, nullptr)))
		{
			throw GameException("DirectInput8Create() failed");
		}
		
		mKeyboard = new Keyboard(*this, mDirectInput);
		components.push_back(mKeyboard);
		mServices.AddService(Keyboard::TypeIdClass(), mKeyboard);

		mMouse = new Mouse(*this, mDirectInput);
		components.push_back(mMouse);
		mServices.AddService(Mouse::TypeIdClass(), mMouse);

		mCamera = new FirstPersonCamera(*this, 1.5708f, this->AspectRatio(), nearPlaneDist, farPlaneDist );
		mCamera->SetPosition(0.0f, 20.0f, 65.0f);
		mCamera->SetMovementRate(movementRate);
		mCamera->SetFOV(fov*XM_PI / 180.0f);
		mCamera->SetNearPlaneDistance(nearPlaneDist);
		mCamera->SetFarPlaneDistance(farPlaneDist);
		components.push_back(mCamera);
		mServices.AddService(Camera::TypeIdClass(), mCamera);

		//Render State Helper
		mRenderStateHelper = new RenderStateHelper(*this);

		#pragma region INITIALIZE_IMGUI

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO(); (void)io;

		ImGui_ImplWin32_Init(mWindowHandle);
		ImGui_ImplDX11_Init(mDirect3DDevice,mDirect3DDeviceContext);
		ImGui::StyleEveryRayColor();


#pragma endregion

		Game::Initialize();
		SetLevel(1);
	}


	// ...*sniff-sniff*
	// ...why does it smell like shit here??...
	// ...oh, amazing level management... multiple inheritance, switch... mhm...
	// ...also have you heard of Data-Oriented Design?..
	void RenderingGame::SetLevel(int level)
	{
		mCamera->Reset();

		if (demoLevel != nullptr)
		{
			demoLevel->Destroy();
			demoLevel = nullptr;
		}
		if (demoLevel == nullptr)
		{
			switch (level)
			{
			case 0:
				demoLevel = new SponzaMainDemo(*this, *mCamera);
				break;
			case 1:
				demoLevel = new TerrainDemo(*this, *mCamera);
				break;
			case 2:
				demoLevel = new PhysicallyBasedRenderingDemo(*this, *mCamera);
				break;
			case 3:
				demoLevel = new SubsurfaceScatteringDemo(*this, *mCamera);
				break;
			case 4:
				demoLevel = new VolumetricLightingDemo(*this, *mCamera);
				break;
			case 5:
				demoLevel = new CollisionTestDemo(*this, *mCamera);
				break;
			case 6:
				demoLevel = new ShadowMappingDemo(*this, *mCamera);
				break;
			case 7:
				demoLevel = new WaterSimulationDemo(*this, *mCamera);
				break;
			case 8:
				demoLevel = new TestSceneDemo(*this, *mCamera);
				break;
			}
		}
		demoLevel->Create();
	}

	void RenderingGame::Update(const GameTime& gameTime)
	{
		auto startUpdateTimer = std::chrono::high_resolution_clock::now();

		if (mKeyboard->WasKeyPressedThisFrame(DIK_ESCAPE))
		{
			Exit();
		}

		UpdateImGui();
		Game::Update(gameTime);
		demoLevel->UpdateLevel(gameTime);

		auto endUpdateTimer = std::chrono::high_resolution_clock::now();
		mElapsedTimeUpdateCPU = endUpdateTimer - startUpdateTimer;

		farPlaneDist = mCamera->FarPlaneDistance();
		nearPlaneDist = mCamera->NearPlaneDistance();
	}
	
	void RenderingGame::UpdateImGui()
	{
		#pragma region ENGINE_SPECIFIC_IMGUI

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGuizmo::BeginFrame();
		if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Backspace)))
			Utility::IsEditorMode = !Utility::IsEditorMode;
		ImGuizmo::Enable(Utility::IsEditorMode);
		if (Utility::IsEditorMode)
		{
			ImGui::Begin("EveryRay Editor");

			ImGui::Text("Camera Position: (%.1f,%.1f,%.1f)", mCamera->Position().x, mCamera->Position().y, mCamera->Position().z);
			if (ImGui::Button("Reset Position"))
				mCamera->SetPosition(XMFLOAT3(0, 0, 0));
			ImGui::SameLine();
			if (ImGui::Button("Reset Direction"))
				mCamera->SetDirection(XMFLOAT3(0, 0, 1));
			ImGui::SliderFloat("Camera Speed", &movementRate, 10.0f, 2000.0f);
			mCamera->SetMovementRate(movementRate);
			ImGui::SliderFloat("Camera FOV", &fov, 1.0f, 90.0f);
			mCamera->SetFOV(fov*XM_PI / 180.0f);
			ImGui::SliderFloat("Camera Near Plane", &nearPlaneDist, 0.5f, 150.0f);
			mCamera->SetNearPlaneDistance(nearPlaneDist);
			ImGui::SliderFloat("Camera Far Plane", &farPlaneDist, 150.0f, 200000.0f);
			mCamera->SetFarPlaneDistance(farPlaneDist);
			ImGui::Checkbox("Enable culling", &Utility::IsCameraCulling);

			ImGui::Separator();

			ImGui::Checkbox("Enable light editor", &Utility::IsLightEditor);
			
			ImGui::End();
		}
			
		ImGui::SetNextWindowBgAlpha(0.9f); // Transparent background
		if (ImGui::Begin("EveryRay - Rendering Engine: Configuration"))
		{
			ImGui::TextColored(ImVec4(0.52f, 0.78f, 0.04f, 1), "Welcome to EveryRay!");
			ImGui::Separator();
			
			ImGui::TextColored(ImVec4(0.95f, 0.5f, 0.0f, 1), "FPS: (%.1f FPS), %.3f ms/frame", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
			
			if (ImGui::IsMousePosValid())
				ImGui::Text("Mouse Position: (%.1f,%.1f)", ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
			else
				ImGui::Text("Mouse Position: <invalid>");
			
			ImGui::Separator();

			ImGui::Checkbox("Switch to Editor", &Utility::IsEditorMode);

			ImGui::Checkbox("Show Profiler", &mShowProfiler);
			if (mShowProfiler)
			{
				ImGui::Begin("EveryRay Profiler");
				
				if (ImGui::CollapsingHeader("CPU Time"))
				{
					ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1), "Render: %f ms", mElapsedTimeRenderCPU.count() * 1000);
					ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1), "Update: %f ms", mElapsedTimeUpdateCPU.count() * 1000);
				}
				if (ImGui::CollapsingHeader("GPU Time"))
				{
				}
				ImGui::End();
			}
			ImGui::Separator();

			if (ImGui::CollapsingHeader("Load Demo Level"))
			{
				if (ImGui::Combo("Level", &currentLevel, displayedLevelNames, IM_ARRAYSIZE(displayedLevelNames)))
					SetLevel(currentLevel);
			}
		}
		ImGui::End();
		#pragma endregion
	}
	
	void RenderingGame::Shutdown()
	{
		DeleteObject(mSponzaMainDemo);
		DeleteObject(mShadowMappingDemo);
		DeleteObject(mPBRDemo);
		//DeleteObject(mInstancingDemo);
		//DeleteObject(mFrustumCullingDemo);
		DeleteObject(mSubsurfaceScatteringDemo);
		DeleteObject(mVolumetricLightingDemo);
		DeleteObject(mCollisionTestDemo);
		DeleteObject(mWaterSimulationDemo);
		DeleteObject(mTerrainDemo);
		DeleteObject(mTestSceneDemo);

		DeleteObject(mKeyboard);
		DeleteObject(mMouse);
		DeleteObject(mCamera);
		DeleteObject(mRenderStateHelper);

		RasterizerStates::Release();
		SamplerStates::Release();

		//destroy imgui
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		Game::Shutdown();
	}
	
	void RenderingGame::Draw(const GameTime& gameTime)
	{
		auto startRenderTimer = std::chrono::high_resolution_clock::now();

		mDirect3DDeviceContext->ClearRenderTargetView(mRenderTargetView, reinterpret_cast<const float*>(&BackgroundColor));
		mDirect3DDeviceContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		Game::Draw(gameTime);
		demoLevel->DrawLevel(gameTime);

		mRenderStateHelper->SaveAll();
		mRenderStateHelper->RestoreAll();

		HRESULT hr = mSwapChain->Present(0, 0);
		if (FAILED(hr))
		{
			throw GameException("IDXGISwapChain::Present() failed.", hr);
		}

		auto endRenderTimer = std::chrono::high_resolution_clock::now();
		mElapsedTimeRenderCPU = endRenderTimer - startRenderTimer;
	}

	void RenderingGame::CollectRenderingTimestamps(ID3D11DeviceContext* pContext)
	{
		//TODO
	}
}

