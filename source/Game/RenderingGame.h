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
}

namespace Rendering
{	
	class TestSceneDemo;
	class SponzaMainDemo;
	class ShadowMappingDemo;
	class PhysicallyBasedRenderingDemo;
	class InstancingDemo;
	class FrustumCullingDemo;
	class SubsurfaceScatteringDemo;
	class VolumetricLightingDemo;
	class CollisionTestDemo;
	class WaterSimulationDemo;
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
		
		//Demo scenes
		TestSceneDemo* mTestSceneDemo;
		SponzaMainDemo* mSponzaMainDemo;
		ShadowMappingDemo* mShadowMappingDemo;
		PhysicallyBasedRenderingDemo* mPBRDemo;
		InstancingDemo* mInstancingDemo;
		FrustumCullingDemo* mFrustumCullingDemo;
		SubsurfaceScatteringDemo* mSubsurfaceScatteringDemo;
		VolumetricLightingDemo* mVolumetricLightingDemo;
		CollisionTestDemo* mCollisionTestDemo;
		WaterSimulationDemo* mWaterSimulationDemo;
		TerrainDemo* mTerrainDemo;
		ParallaxMappingDemo* mParallaxOcclusionDemo;
		RenderStateHelper* mRenderStateHelper;

		std::chrono::duration<double> mElapsedTimeUpdateCPU;
		std::chrono::duration<double> mElapsedTimeRenderCPU;

		bool mShowProfiler;
		bool mShowCameraSettings = true;
	};
}