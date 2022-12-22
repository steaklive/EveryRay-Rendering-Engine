#pragma once
#define MAX_SCENES_COUNT 25

#include "ER_Core.h"
#include "Common.h"

namespace EveryRay_Core
{
	class ER_Mouse;
	class ER_Keyboard;
	class ER_Gamepad;
	class ER_CameraFPS;
	class ER_Editor;
	class ER_QuadRenderer;
	
	enum GraphicsQualityPreset
	{
		ULTRA_LOW = 0,
		LOW,
		MEDIUM,
		HIGH,
		ULTRA_HIGH,
		GRAPHICS_PRESETS_COUNT
	};

	class ER_RuntimeCore : public ER_Core
	{
	public:
		ER_RuntimeCore(ER_RHI* aRHI, HINSTANCE instance, const std::wstring& windowClass, const std::wstring& windowTitle, int showCommand, bool isFullscreen);
		~ER_RuntimeCore();

		virtual void Initialize() override;
		virtual void Update(const ER_CoreTime& gameTime) override;
		virtual void Draw(const ER_CoreTime& gameTime) override;	

		virtual ER_RHI_GPUTexture* AddOrGetGPUTextureFromCache(const std::wstring& aFullPath, bool* didExist = nullptr, bool is3D = false, bool skipFallback = false, bool* statusFlag = nullptr, bool isSilent = false) override;
		virtual void AddGPUTextureToCache(const std::wstring& aFullPath, ER_RHI_GPUTexture* aTexture) override;
		virtual bool RemoveGPUTextureFromCache(const std::wstring& aFullPath, bool removeKey = false) override;
		virtual void ReplaceGPUTextureFromCache(const std::wstring& aFullPath, ER_RHI_GPUTexture* aTex) override; // WARNING: dangerous!
		virtual bool IsGPUTextureInCache(const std::wstring& aFullPath) override;

	protected:
		virtual void Shutdown() override;
	
	private:
		void LoadGlobalLevelsConfig();
		void LoadGraphicsConfig();
		void SetLevel(const std::string& aSceneName, bool isFirstLoad = false);
		void UpdateImGui();

		static const XMVECTORF32 BackgroundColor;
		static const XMVECTORF32 BackgroundColor2;

		LPDIRECTINPUT8 mDirectInput;
		ER_Keyboard* mKeyboard;
		ER_Mouse* mMouse;
		ER_Gamepad* mGamepad;
		ER_CameraFPS* mCamera;
		ER_Editor* mEditor;
		ER_QuadRenderer* mQuadRenderer;

		ER_RHI_Viewport mMainViewport;

		std::chrono::duration<double> mElapsedTimeUpdateCPU;
		std::chrono::duration<double> mElapsedTimeRenderCPU;

		std::map<std::wstring, ER_RHI_GPUTexture*> mObjectsTextureCache; // all textures from ER_RenderingObjects in the level

		std::map<std::string, std::string> mScenesPaths;
		std::vector<std::string> mScenesNamesByIndices;
		char* mDisplayedLevelNames[MAX_SCENES_COUNT];
		unsigned int mNumParsedScenesFromConfig;

		std::string mStartupSceneName;
		std::string mCurrentSceneName;

		bool mShowProfiler;
		bool mShowCameraSettings = true;
		bool mIsRHIReset = false;

		GraphicsQualityPreset mCurrentGfxQuality;
	};
}