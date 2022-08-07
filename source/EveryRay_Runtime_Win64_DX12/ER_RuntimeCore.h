#pragma once
#define MAX_SCENES_COUNT 25

#include "..\EveryRay_Core\ER_Core.h"
#include "..\EveryRay_Core\Common.h"

using namespace EveryRay_Core;
namespace EveryRay_Core
{
	class ER_Mouse;
	class ER_Keyboard;
	class ER_CameraFPS;
	class RenderStateHelper;
	class ER_Editor;
	class ER_QuadRenderer;
}

namespace EveryRay_Runtime
{	
	class ER_RuntimeCore : public ER_Core
	{
	public:
		ER_RuntimeCore(ER_RHI* aRHI, HINSTANCE instance, const std::wstring& windowClass, const std::wstring& windowTitle, int showCommand, UINT width, UINT height, bool isFullscreen);
		~ER_RuntimeCore();

		virtual void Initialize() override;
		virtual void Update(const ER_CoreTime& gameTime) override;
		virtual void Draw(const ER_CoreTime& gameTime) override;	
	protected:
		virtual void Shutdown() override;
	
	private:
		void LoadGlobalLevelsConfig();
		void SetLevel(const std::string& aSceneName);
		void UpdateImGui();

		static const XMVECTORF32 BackgroundColor;
		static const XMVECTORF32 BackgroundColor2;

		LPDIRECTINPUT8 mDirectInput;
		ER_Keyboard* mKeyboard;
		ER_Mouse* mMouse;
		ER_CameraFPS* mCamera;
		ER_Editor* mEditor;
		ER_QuadRenderer* mQuadRenderer;

		ER_RHI_Viewport mMainViewport;

		std::chrono::duration<double> mElapsedTimeUpdateCPU;
		std::chrono::duration<double> mElapsedTimeRenderCPU;

		std::map<std::string, std::string> mScenesPaths;
		std::vector<std::string> mScenesNamesByIndices;
		char* mDisplayedLevelNames[MAX_SCENES_COUNT];
		unsigned int mNumParsedScenesFromConfig;

		std::string mStartupSceneName;
		std::string mCurrentSceneName;

		bool mShowProfiler;
		bool mShowCameraSettings = true;
	};
}