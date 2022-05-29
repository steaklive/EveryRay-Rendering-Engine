#include "stdafx.h"

#include "RenderingGame.h"

#include "..\JsonCpp\include\json\json.h"

#include <sstream>
#include <fstream>
#include <iostream>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include "..\Library\GameException.h"
#include "..\Library\Keyboard.h"
#include "..\Library\Mouse.h"
#include "..\Library\Utility.h"
#include "..\Library\FirstPersonCamera.h"
#include "..\Library\ColorHelper.h"
#include "..\Library\RenderStateHelper.h"
#include "..\Library\MatrixHelper.h"
#include "..\Library\ER_Sandbox.h"
#include "..\Library\Editor.h"
#include "..\Library\RasterizerStates.h"
#include "..\Library\SamplerStates.h"
#include "..\Library\ER_QuadRenderer.h"

// include scenes

// include IMGUI
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "ImGuizmo.h"

namespace Rendering
{
	const XMVECTORF32 RenderingGame::BackgroundColor = ColorHelper::Black;
	const XMVECTORF32 RenderingGame::BackgroundColor2 = ColorHelper::Red;
	
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
		mShowProfiler(false),
		mEditor(nullptr),
		mQuadRenderer(nullptr),
		mRenderStateHelper(nullptr)
	{
		mDepthStencilBufferEnabled = true;
		mMultiSamplingEnabled = false;
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

		{
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
		}

		mCamera = new FirstPersonCamera(*this, 1.5708f, this->AspectRatio(), nearPlaneDist, farPlaneDist );
		mCamera->SetPosition(0.0f, 20.0f, 65.0f);
		mCamera->SetMovementRate(movementRate);
		mCamera->SetFOV(fov*XM_PI / 180.0f);
		mCamera->SetNearPlaneDistance(nearPlaneDist);
		mCamera->SetFarPlaneDistance(farPlaneDist);
		components.push_back(mCamera);
		mServices.AddService(Camera::TypeIdClass(), mCamera);

		mEditor = new Editor(*this);
		components.push_back(mEditor);
		mServices.AddService(Editor::TypeIdClass(), mEditor);

		mQuadRenderer = new ER_QuadRenderer(*this);
		components.push_back(mQuadRenderer);
		mServices.AddService(ER_QuadRenderer::TypeIdClass(), mQuadRenderer);

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
		LoadGlobalLevelsConfig();
		SetLevel(mStartupSceneName);
	}

	void RenderingGame::LoadGlobalLevelsConfig()
	{
		Json::Reader reader;
		std::string path = Utility::GetFilePath("content\\levels\\global_scenes_config.json");
		std::ifstream globalConfig(path.c_str(), std::ifstream::binary);
		
		Json::Value root;

		if (!reader.parse(globalConfig, root)) {
			throw GameException(reader.getFormattedErrorMessages().c_str());
		}
		else
		{
			if (root["scenes"].size() == 0)
				throw GameException("No scenes defined in global_scenes_config.json");

			mNumParsedScenesFromConfig = root["scenes"].size();
			if (mNumParsedScenesFromConfig > MAX_SCENES_COUNT)
				throw GameException("Amount of parsed scenes is bigger than MAX_SCENES_COUNT. Increase MAX_SCENES_COUNT!");

			for (int i = 0; i < mNumParsedScenesFromConfig; i++)
				mDisplayedLevelNames[i] = (char*)malloc(sizeof(char) * 100);

			for (Json::Value::ArrayIndex i = 0; i != mNumParsedScenesFromConfig; i++)
			{
				strcpy(mDisplayedLevelNames[i], root["scenes"][i]["scene_name"].asCString());
				mScenesPaths.emplace(root["scenes"][i]["scene_name"].asString(), root["scenes"][i]["scene_path"].asString());
				mScenesNamesByIndices.push_back(root["scenes"][i]["scene_name"].asString());
			}

			if (!root.isMember("startup_scene"))
				throw GameException("No startup scene defined in global_scenes_config.json");
			else
			{
				mStartupSceneName = root["startup_scene"].asString();
				if (mScenesPaths.find(mStartupSceneName) == mScenesPaths.end())
					throw GameException("No startup scene defined in global_scenes_config.json");
			}
		}
	}

	void RenderingGame::SetLevel(const std::string& aSceneName)
	{
		mCurrentSceneName = aSceneName;
		mCamera->Reset();

		if (mCurrentSandbox)
		{
			mCurrentSandbox->Destroy(*this);
			DeleteObject(mCurrentSandbox);
		}

		mCurrentSandbox = new ER_Sandbox();
		if (mScenesPaths.find(aSceneName) != mScenesPaths.end())
			mCurrentSandbox->Initialize(*this, *mCamera, aSceneName, Utility::GetFilePath(mScenesPaths[aSceneName]));
		else
		{
			std::string message = "Scene was not found with this name: " + aSceneName;
			throw GameException(message.c_str());
		}
	}

	void RenderingGame::Update(const GameTime& gameTime)
	{
		assert(mCurrentSandbox);

		auto startUpdateTimer = std::chrono::high_resolution_clock::now();

		if (mKeyboard->WasKeyPressedThisFrame(DIK_ESCAPE))
			Exit();

		UpdateImGui();

		Game::Update(gameTime); //engine components (input, camera, etc.);
		mCurrentSandbox->Update(*this, gameTime); //level components (rendering systems, culling, etc.)

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
			ImGui::Begin("EveryRay Camera Editor");

			ImGui::Text("Camera Position: (%.1f,%.1f,%.1f)", mCamera->Position().x, mCamera->Position().y, mCamera->Position().z);
			if (ImGui::Button("Reset Position"))
				mCamera->SetPosition(XMFLOAT3(0, 0, 0));
			ImGui::SameLine();
			if (ImGui::Button("Reset Direction"))
				mCamera->SetDirection(XMFLOAT3(0, 0, 1));
			ImGui::SliderFloat("Camera Speed", &movementRate, 0.0f, 2000.0f);
			mCamera->SetMovementRate(movementRate);
			ImGui::SliderFloat("Camera FOV", &fov, 1.0f, 90.0f);
			mCamera->SetFOV(fov*XM_PI / 180.0f);
			ImGui::SliderFloat("Camera Near Plane", &nearPlaneDist, 0.5f, 150.0f);
			mCamera->SetNearPlaneDistance(nearPlaneDist);
			ImGui::SliderFloat("Camera Far Plane", &farPlaneDist, 150.0f, 200000.0f);
			mCamera->SetFarPlaneDistance(farPlaneDist);
			ImGui::Checkbox("CPU frustum culling", &Utility::IsMainCameraCPUFrustumCulling);
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

			if (ImGui::CollapsingHeader("Load level"))
			{
				if (ImGui::Combo("Level", &currentLevel, mDisplayedLevelNames, mNumParsedScenesFromConfig))
					SetLevel(mScenesNamesByIndices[currentLevel]);
			}
			if (ImGui::Button("Reload current level")) {
				SetLevel(mCurrentSceneName);
			}
		}
		ImGui::End();
		#pragma endregion
	}
	
	void RenderingGame::Shutdown()
	{
		DeleteObject(mKeyboard);
		DeleteObject(mEditor);
		DeleteObject(mQuadRenderer);
		DeleteObject(mMouse);
		DeleteObject(mCamera);
		DeleteObject(mRenderStateHelper);

		RasterizerStates::Release();
		SamplerStates::Release();

		//destroy imgui
		{
			ImGui_ImplDX11_Shutdown();
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();
		}

		Game::Shutdown();
	}
	
	void RenderingGame::Draw(const GameTime& gameTime)
	{
		assert(mCurrentSandbox);

		auto startRenderTimer = std::chrono::high_resolution_clock::now();

		mDirect3DDeviceContext->ClearRenderTargetView(mRenderTargetView, reinterpret_cast<const float*>(&BackgroundColor));
		mDirect3DDeviceContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		Game::Draw(gameTime); //TODO remove
		mCurrentSandbox->Draw(*this, gameTime);

		mRenderStateHelper->SaveAll();
		mRenderStateHelper->RestoreAll();

		HRESULT hr = mSwapChain->Present(0, 0);
		if (FAILED(hr))
			throw GameException("IDXGISwapChain::Present() failed.", hr);

		auto endRenderTimer = std::chrono::high_resolution_clock::now();
		mElapsedTimeRenderCPU = endRenderTimer - startRenderTimer;
	}

	void RenderingGame::CollectGPUTimestamps(ID3D11DeviceContext* pContext)
	{
		//TODO
	}
}

