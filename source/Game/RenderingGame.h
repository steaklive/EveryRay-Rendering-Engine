#pragma once
#define MAX_SCENES_COUNT 25

#include "..\Library\Game.h"
#include "..\Library\Common.h"
#include <chrono>

using namespace Library;
namespace Library
{
	class Mouse;
	class Keyboard;
	class FirstPersonCamera;
	class RenderStateHelper;
	class ER_Editor;
	class ER_QuadRenderer;
}

namespace Rendering
{	
	class RenderingGame : public Game
	{
	public:
		RenderingGame(HINSTANCE instance, const std::wstring& windowClass, const std::wstring& windowTitle, int showCommand);
		~RenderingGame();

		virtual void Initialize() override;
		virtual void Update(const GameTime& gameTime) override;
		virtual void Draw(const GameTime& gameTime) override;

		void CollectGPUTimestamps(ID3D11DeviceContext * pContext);	
	protected:
		virtual void Shutdown() override;
	
	private:
		void LoadGlobalLevelsConfig();
		void SetLevel(const std::string& aSceneName);
		void UpdateImGui();

		static const XMVECTORF32 BackgroundColor;
		static const XMVECTORF32 BackgroundColor2;

		LPDIRECTINPUT8 mDirectInput;
		Keyboard* mKeyboard;
		Mouse* mMouse;
		FirstPersonCamera* mCamera;
		ER_Editor* mEditor;
		ER_QuadRenderer* mQuadRenderer;

		RenderStateHelper* mRenderStateHelper; //TODO move to Game.cpp

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