#pragma once

#include "..\Library\Game.h"
#include "..\Library\Common.h"
#include "..\Library\DemoLevel.h"
#include <chrono>

using namespace Library;
namespace Library
{
	class FpsComponent;
	class Mouse;
	class Keyboard;
	class FirstPersonCamera;
	class RenderStateHelper;
	class Editor;
}

namespace Rendering
{	
	class TestSceneDemo;
	class SubsurfaceScatteringDemo;
	//class VolumetricLightingDemo;
	class CollisionTestDemo;
	//class WaterSimulationDemo;
	class ParallaxMappingDemo;
	class TerrainDemo;

	class RenderingGame : public Game
	{
	public:
		RenderingGame(HINSTANCE instance, const std::wstring& windowClass, const std::wstring& windowTitle, int showCommand);
		~RenderingGame();

		virtual void Initialize() override;
		virtual void Update(const GameTime& gameTime) override;
		virtual void Draw(const GameTime& gameTime) override;

		void CollectRenderingTimestamps(ID3D11DeviceContext * pContext);	
	protected:
		virtual void Shutdown() override;
	
	private:
		void SetLevel(int level);
		void UpdateImGui();

		static const XMVECTORF32 BackgroundColor;
		static const XMVECTORF32 BackgroundColor2;
		static const float BrightnessModulationRate;

		LPDIRECTINPUT8 mDirectInput;
		Keyboard* mKeyboard;
		Mouse* mMouse;
		XMFLOAT2 mMouseTextPosition;
		FirstPersonCamera* mCamera;
		Editor* mEditor;
		
		//Demo scenes
		TestSceneDemo* mTestSceneDemo;
		SubsurfaceScatteringDemo* mSubsurfaceScatteringDemo;
		//VolumetricLightingDemo* mVolumetricLightingDemo;
		CollisionTestDemo* mCollisionTestDemo;
		//WaterSimulationDemo* mWaterSimulationDemo;
		TerrainDemo* mTerrainDemo;
		ParallaxMappingDemo* mParallaxOcclusionDemo;
		RenderStateHelper* mRenderStateHelper;

		std::chrono::duration<double> mElapsedTimeUpdateCPU;
		std::chrono::duration<double> mElapsedTimeRenderCPU;

		int mCurrentLevelIndex = -1;
		bool mShowProfiler;
		bool mShowCameraSettings = true;
	};
}