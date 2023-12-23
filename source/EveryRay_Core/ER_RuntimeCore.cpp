#include "ER_RuntimeCore.h"
#include "ER_CoreException.h"
#include "ER_Keyboard.h"
#include "ER_Mouse.h"
#include "ER_Gamepad.h"
#include "ER_Utility.h"
#include "ER_Settings.h"
#include "ER_CameraFPS.h"
#include "ER_ColorHelper.h"
#include "ER_MatrixHelper.h"
#include "ER_Sandbox.h"
#include "ER_Editor.h"
#include "ER_QuadRenderer.h"
#include "ER_Model.h"

#include "..\JsonCpp\include\json\json.h"

namespace EveryRay_Core
{
	static int currentLevel = 0;
	static std::mutex renderingObjectsTextureCacheMutex;
	static std::mutex renderingObjects3DModelsCacheMutex;

	ER_RuntimeCore::ER_RuntimeCore(ER_RHI* aRHI, HINSTANCE instance, const std::wstring& windowClass, const std::wstring& windowTitle, int showCommand, bool isFullscreen)
		: ER_Core(aRHI, instance, windowClass, windowTitle, showCommand, isFullscreen),
		mDirectInput(nullptr),
		mKeyboard(nullptr),
		mMouse(nullptr),
		mGamepad(nullptr),
		mShowProfiler(false),
		mEditor(nullptr),
		mQuadRenderer(nullptr)
	{
		LoadGraphicsConfig();

		mMainViewport.TopLeftX = 0.0f;
		mMainViewport.TopLeftY = 0.0f;
		mMainViewport.Width = static_cast<float>(mScreenWidth);
		mMainViewport.Height = static_cast<float>(mScreenHeight);
		mMainViewport.MinDepth = 0.0f;
		mMainViewport.MaxDepth = 1.0f;

		mMainRect = { 0, 0, static_cast<LONG>(mScreenWidth), static_cast<LONG>(mScreenHeight) };
	}

	ER_RuntimeCore::~ER_RuntimeCore()
	{
	}

	void ER_RuntimeCore::Initialize()
	{
		{
			if (FAILED(DirectInput8Create(mInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (LPVOID*)&mDirectInput, nullptr)))
			{
				throw ER_CoreException("DirectInput8Create() failed");
			}

			mKeyboard = new ER_Keyboard(*this, mDirectInput);
			mCoreComponents.push_back(mKeyboard);
			mCoreServices.AddService(ER_Keyboard::TypeIdClass(), mKeyboard);

			mMouse = new ER_Mouse(*this, mDirectInput);
			mCoreComponents.push_back(mMouse);
			mCoreServices.AddService(ER_Mouse::TypeIdClass(), mMouse);

			mGamepad = new ER_Gamepad(*this);
			mCoreComponents.push_back(mGamepad);
			mCoreServices.AddService(ER_Gamepad::TypeIdClass(), mGamepad);
		}

		mCamera = new ER_CameraFPS(*this, 1.5708f, this->AspectRatio(), 0.5f, 100000.0f);
		mCamera->SetPosition(0.0f, 20.0f, 65.0f);
		mCamera->SetMovementRate(10.0f);
		mCamera->SetFOV(60.0f * XM_PI / 180.0f);
		mCamera->SetNearPlaneDistance(0.5f);
		mCamera->SetFarPlaneDistance(100000.0f);
		mCoreComponents.push_back(mCamera);
		mCoreServices.AddService(ER_Camera::TypeIdClass(), mCamera);

		mEditor = new ER_Editor(*this);
		mCoreComponents.push_back(mEditor);
		mCoreServices.AddService(ER_Editor::TypeIdClass(), mEditor);

		mQuadRenderer = new ER_QuadRenderer(*this);
		mCoreComponents.push_back(mQuadRenderer);
		mCoreServices.AddService(ER_QuadRenderer::TypeIdClass(), mQuadRenderer);

		#pragma region INITIALIZE_IMGUI

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO(); (void)io;

		ImGui_ImplWin32_Init(mWindowHandle);
		if (mRHI)
			mRHI->InitImGui();
#pragma endregion

		ER_Core::Initialize();
		LoadGlobalLevelsConfig();
		SetLevel(mStartupSceneName, true);
	}

	void ER_RuntimeCore::LoadGlobalLevelsConfig()
	{
		Json::Reader reader;
		std::string path = ER_Utility::GetFilePath("content\\levels\\global_scenes_config.json");
		std::ifstream globalConfig(path.c_str(), std::ifstream::binary);
		
		Json::Value root;

		if (!reader.parse(globalConfig, root)) {
			throw ER_CoreException(reader.getFormattedErrorMessages().c_str());
		}
		else
		{
			if (root["scenes"].size() == 0)
				throw ER_CoreException("No scenes defined in global_scenes_config.json");

			mNumParsedScenesFromConfig = root["scenes"].size();
			if (mNumParsedScenesFromConfig > MAX_SCENES_COUNT)
				throw ER_CoreException("Amount of parsed scenes is bigger than MAX_SCENES_COUNT. Increase MAX_SCENES_COUNT!");

			for (int i = 0; i < static_cast<int>(mNumParsedScenesFromConfig); i++)
				mDisplayedLevelNames[i] = (char*)malloc(sizeof(char) * 100);

			for (Json::Value::ArrayIndex i = 0; i != mNumParsedScenesFromConfig; i++)
			{
				strcpy(mDisplayedLevelNames[i], root["scenes"][i]["scene_name"].asCString());
				mScenesPaths.emplace(root["scenes"][i]["scene_name"].asString(), root["scenes"][i]["scene_path"].asString());
				mScenesNamesByIndices.push_back(root["scenes"][i]["scene_name"].asString());
			}

			if (!root.isMember("startup_scene"))
				throw ER_CoreException("No startup scene defined in global_scenes_config.json");
			else
			{
				mStartupSceneName = root["startup_scene"].asString();
				if (mScenesPaths.find(mStartupSceneName) == mScenesPaths.end())
					throw ER_CoreException("No startup scene defined in global_scenes_config.json");
			}
		}
	}

	void ER_RuntimeCore::LoadGraphicsConfig()
	{
		Json::Reader reader;
		std::string path = ER_Utility::GetFilePath("graphics_config.json");
		std::ifstream graphicsConfig(path.c_str(), std::ifstream::binary);

		Json::Value root;

		if (!reader.parse(graphicsConfig, root)) {
			throw ER_CoreException(reader.getFormattedErrorMessages().c_str());
		}
		else
		{
			int numPresets = root["presets"].size();
			if (numPresets == 0)
				throw ER_CoreException("No presets defined in graphics_config.json");

			if (numPresets != (int)GraphicsQualityPreset::GRAPHICS_PRESETS_COUNT)
				throw ER_CoreException("No presets defined in graphics_config.json");

			std::map<std::string, int> presetNames;
			for (Json::Value::ArrayIndex i = 0; i != numPresets; i++)
				presetNames.emplace(root["presets"][i]["preset_name"].asString(), i);

			if (!root.isMember("current_preset"))
				throw ER_CoreException("No current preset specified in graphics_config.json");

			std::string currentPreset = root["current_preset"].asString();
			auto it = presetNames.find(currentPreset);
			if (it == presetNames.end())
				throw ER_CoreException("Current preset is not recognized in graphics_config.json. Maybe a typo?");

			std::string logMessage = "[ER Logger][ER_Core] Selected graphics preset: " + currentPreset + "\n";
			ER_OUTPUT_LOG(ER_Utility::ToWideString(logMessage).c_str());

			int currentPresetIndex = it->second;
			//set quality
			{
				mCurrentGfxQuality = (GraphicsQualityPreset)currentPresetIndex;

				if (!root["presets"][currentPresetIndex].isMember("resolution_width") || !root["presets"][currentPresetIndex].isMember("resolution_height"))
					throw ER_CoreException("Current preset has no specified resolution in graphics_config.json");

				mScreenWidth = root["presets"][currentPresetIndex]["resolution_width"].asUInt();
				mScreenHeight = root["presets"][currentPresetIndex]["resolution_height"].asUInt();

				ER_Settings::TexturesQuality = root["presets"][currentPresetIndex]["texture_quality"].asInt();
				ER_Settings::FoliageQuality = root["presets"][currentPresetIndex]["foliage_quality"].asInt();
				ER_Settings::ShadowsQuality = root["presets"][currentPresetIndex]["shadow_quality"].asInt();
				ER_Settings::GlobalIlluminationQuality = root["presets"][currentPresetIndex]["gi_quality"].asInt();
				ER_Settings::AntiAliasingQuality = root["presets"][currentPresetIndex]["aa_quality"].asInt();
				ER_Settings::SubsurfaceScatteringQuality = root["presets"][currentPresetIndex]["sss_quality"].asInt();
				ER_Settings::VolumetricFogQuality = root["presets"][currentPresetIndex]["volumetric_fog_quality"].asInt();
				ER_Settings::VolumetricCloudsQuality = root["presets"][currentPresetIndex]["volumetric_clouds_quality"].asInt();
			}
		}
	}

	void ER_RuntimeCore::SetLevel(const std::string& aSceneName, bool isFirstLoad)
	{
		mCurrentSceneName = aSceneName;
		mCamera->Reset();

		if (mCurrentSandbox)
		{
			mCurrentSandbox->Destroy(*this);
			DeleteObject(mCurrentSandbox);
		}

		for (auto& it : mRenderingObjectsTextureCache)
			DeleteObject(it.second);
		mRenderingObjectsTextureCache.erase(mRenderingObjectsTextureCache.begin(), mRenderingObjectsTextureCache.end());

		mRenderingObjects3DModelsCache.erase(mRenderingObjects3DModelsCache.begin(), mRenderingObjects3DModelsCache.end());

		if (mRHI && !isFirstLoad)
		{
			mRHI->ResetRHI(mScreenWidth, mScreenHeight, mIsFullscreen);
			mRHI->ResetReplacementMippedTexturesPool();
			mRHI->ResetDescriptorManager();

			mIsRHIReset = true;
		}

		mCurrentSandbox = new ER_Sandbox();
		if (mScenesPaths.find(aSceneName) != mScenesPaths.end())
			mCurrentSandbox->Initialize(*this, *mCamera, aSceneName, ER_Utility::GetFilePath(mScenesPaths[aSceneName]));
		else
		{
			std::string message = "Scene was not found with this name: " + aSceneName;
			throw ER_CoreException(message.c_str());
		}
	}

	void ER_RuntimeCore::Update(const ER_CoreTime& gameTime)
	{
		assert(mCurrentSandbox);
		if (mIsRHIReset)
			mIsRHIReset = false;

		auto startUpdateTimer = std::chrono::high_resolution_clock::now();

		if (mKeyboard->WasKeyPressedThisFrame(DIK_ESCAPE))
			Exit();

		int updateCommandList = mRHI->GetPrepareGraphicsCommandListIndex() - 1;
		mRHI->BeginGraphicsCommandList(updateCommandList);

		UpdateImGui();

		ER_Core::Update(gameTime); //engine components (input, camera, etc.);
		mCurrentSandbox->Update(*this, gameTime); //level components (rendering systems, culling, etc.)

		if (!mIsRHIReset)
		{
			mRHI->EndGraphicsCommandList(updateCommandList);
			mRHI->ExecuteCommandLists(updateCommandList); // it will wait for GPU on a copy fence in this method, too
		}
		auto endUpdateTimer = std::chrono::high_resolution_clock::now();
		mElapsedTimeUpdateCPU = endUpdateTimer - startUpdateTimer;
	}
	
	void ER_RuntimeCore::UpdateImGui()
	{
		#pragma region ENGINE_SPECIFIC_IMGUI

		if (mRHI)
			mRHI->StartNewImGuiFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGuizmo::BeginFrame();
		if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Backspace)) || mGamepad->WasButtonPressedThisFrame(GamepadButtons::GamepadBackButton))
			ER_Utility::IsEditorMode = !ER_Utility::IsEditorMode;
		ImGuizmo::Enable(ER_Utility::IsEditorMode);
			
		ImGui::SetNextWindowBgAlpha(0.9f); // Transparent background
		if (ImGui::Begin("EveryRay - Rendering Engine"))
		{
			ImGui::TextColored(ImVec4(0.52f, 0.78f, 0.04f, 1), "Welcome to EveryRay!");
			ImGui::Separator();
			
			ImGui::TextColored(ImVec4(0.95f, 0.5f, 0.0f, 1), "FPS: (%.1f FPS), %.3f ms/frame", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
			if (ImGui::IsMousePosValid())
				ImGui::Text("Mouse Position: (%.1f,%.1f)", ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
			else
				ImGui::Text("Mouse Position: <invalid>");
			
			ImGui::Separator();
			ImGui::Checkbox("Editor Mode", &ER_Utility::IsEditorMode);
			
			if (ImGui::CollapsingHeader("Profiler"))
			{
				ImGui::TextColored(ImVec4(0.95f, 0.5f, 0.0f, 1), "CPU Render: %f ms", mElapsedTimeRenderCPU.count() * 1000);
				ImGui::TextColored(ImVec4(0.95f, 0.5f, 0.0f, 1), "CPU Update: %f ms", mElapsedTimeUpdateCPU.count() * 1000);
			}
			
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
	
	void ER_RuntimeCore::Shutdown()
	{
		mRHI->WaitForGpuOnGraphicsFence();

		DeleteObject(mKeyboard);
		DeleteObject(mEditor);
		DeleteObject(mQuadRenderer);
		DeleteObject(mMouse);
		DeleteObject(mCamera);

		if (mCurrentSandbox)
		{
			mCurrentSandbox->Destroy(*this);
			DeleteObject(mCurrentSandbox);
		}

		//destroy imgui
		{
			if (mRHI)
				mRHI->ShutdownImGui();
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();
		}

		ER_Core::Shutdown();
	}
	
	void ER_RuntimeCore::Draw(const ER_CoreTime& gameTime)
	{
		assert(mCurrentSandbox);
		assert(mRHI);

		auto startRenderTimer = std::chrono::high_resolution_clock::now();

		mRHI->BeginGraphicsCommandList();
		mRHI->SetGPUDescriptorHeap(ER_RHI_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

		mRHI->ClearMainRenderTarget(clearColorBlack);
		mRHI->ClearMainDepthStencilTarget(1.0f, 0);

		mRHI->SetViewport(mMainViewport);
		mRHI->SetRect(mMainRect);

		mRHI->SetDepthStencilState(ER_DEPTH_ONLY_WRITE_COMPARISON_LESS_EQUAL);
		mRHI->SetRasterizerState(ER_RHI_RASTERIZER_STATE::ER_NO_CULLING);
		mRHI->SetBlendState(ER_RHI_BLEND_STATE::ER_NO_BLEND);

		mCurrentSandbox->Draw(*this, gameTime);

		mRHI->TransitionMainRenderTargetToPresent();
		mRHI->EndGraphicsCommandList();
		mRHI->ExecuteCommandLists();
		mRHI->PresentGraphics();

		auto endRenderTimer = std::chrono::high_resolution_clock::now();
		mElapsedTimeRenderCPU = endRenderTimer - startRenderTimer;
	}

	ER_Model* ER_RuntimeCore::AddOrGet3DModelFromCache(const std::string& aFullPath, bool* didExist /*= nullptr*/, bool isSilent /*= false*/)
	{
		const std::lock_guard<std::mutex> lock(renderingObjects3DModelsCacheMutex);
		
		auto it = mRenderingObjects3DModelsCache.find(aFullPath);
		if (it != mRenderingObjects3DModelsCache.end())
		{
			if (didExist)
				*didExist = true;
			return &(it->second);
		}
		else
		{
			if (didExist)
				*didExist = false;

			auto result = mRenderingObjects3DModelsCache.emplace(std::piecewise_construct,
				std::forward_as_tuple(aFullPath), std::forward_as_tuple(*this, aFullPath, true, isSilent)); // this is ugly and we might consider updating to c++17 (try_emplace)
			if (!result.first->second.IsLoaded())
			{
				std::string msg = "[ER Logger][ER_Core] Error! Could not load a new 3D model to models cache: " + aFullPath + '\n';
				ER_OUTPUT_LOG(ER_Utility::ToWideString(msg).c_str());

				mRenderingObjects3DModelsCache.erase(aFullPath);

				return nullptr;
			}
			else
			{
				std::string msg = "[ER Logger][ER_Core] Added new 3D model to models cache: " + aFullPath + '\n';
				ER_OUTPUT_LOG(ER_Utility::ToWideString(msg).c_str());

				return &(result.first->second);
			}
		}

		return nullptr;
	}

	ER_RHI_GPUTexture* ER_RuntimeCore::AddOrGetGPUTextureFromCache(const std::wstring& aFullPath, bool* didExist, bool is3D /*= false*/, bool skipFallback /*= false*/, bool* statusFlag /*= nullptr*/, bool isSilent /*= false*/)
	{
		const std::lock_guard<std::mutex> lock(renderingObjectsTextureCacheMutex);

		auto it = mRenderingObjectsTextureCache.find(aFullPath);
		if (it != mRenderingObjectsTextureCache.end())
		{
			if (didExist)
				*didExist = true;
			return it->second;
		}
		else
		{
			if (didExist)
				*didExist = false;

			auto result = mRenderingObjectsTextureCache.emplace(aFullPath, mRHI->CreateGPUTexture(aFullPath));
			result.first->second->CreateGPUTextureResource(mRHI, aFullPath, true, is3D, skipFallback, statusFlag, isSilent);
			if (statusFlag && *statusFlag == false)
			{
				DeleteObject(result.first->second);
				mRenderingObjectsTextureCache.erase(aFullPath);
				return nullptr;
			}
			else
			{
				std::wstring msg = L"[ER Logger][ER_Core] Added new texture to rendering objects' texture cache: " + aFullPath + L'\n';
				ER_OUTPUT_LOG(msg.c_str());

				return result.first->second;
			}
		}

		return nullptr;
	}

	void ER_RuntimeCore::AddGPUTextureToCache(const std::wstring& aFullPath, ER_RHI_GPUTexture* aTexture)
	{
		mRenderingObjectsTextureCache.emplace(aFullPath, aTexture);
	}

	bool ER_RuntimeCore::RemoveGPUTextureFromCache(const std::wstring& aFullPath, bool removeKey)
	{
		auto it = mRenderingObjectsTextureCache.find(aFullPath);
		if (it != mRenderingObjectsTextureCache.end())
		{
			DeleteObject(it->second);
			if (removeKey)
				mRenderingObjectsTextureCache.erase(it->first);

			return true;
		}
		else
			return false;
	}

	void ER_RuntimeCore::ReplaceGPUTextureFromCache(const std::wstring& aFullPath, ER_RHI_GPUTexture* aTex)
	{
		auto it = mRenderingObjectsTextureCache.find(aFullPath);
		if (it != mRenderingObjectsTextureCache.end())
			it->second = aTex;
	}

	bool ER_RuntimeCore::IsGPUTextureInCache(const std::wstring& aFullPath)
	{
		auto it = mRenderingObjectsTextureCache.find(aFullPath);
		return (it != mRenderingObjectsTextureCache.end());
	}

}

