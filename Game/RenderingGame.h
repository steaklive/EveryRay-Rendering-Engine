#pragma once

#include "..\Library\Game.h"
#include "..\Library\Common.h"
#include "..\Library\DemoLevel.h"

using namespace Library;

namespace DirectX
{
	class SpriteBatch;
	class SpriteFont;
}

namespace Library
{
	class FpsComponent;
	class Mouse;
	class Keyboard;
	class FirstPersonCamera;
	class Grid;
	class RenderStateHelper;

	class Effect;
	class FullScreenRenderTarget;
	class FullScreenQuad;
}

namespace Rendering
{	
	class AmbientLightingDemo;
	class ShadowMappingDemo;
	class PhysicallyBasedRenderingDemo;
	class InstancingDemo;
	class FrustumCullingDemo;
	class SubsurfaceScatteringDemo;
	class VolumetricLightingDemo;
	class CollisionTestDemo;
	class WaterSimulationDemo;

	class RenderingGame : public Game
	{
	public:
		RenderingGame(HINSTANCE instance, const std::wstring& windowClass, const std::wstring& windowTitle, int showCommand);
		~RenderingGame();

		virtual void Initialize() override;
		virtual void Update(const GameTime& gameTime) override;
		virtual void Draw(const GameTime& gameTime) override;

		void UpdateColorFilterMaterial();
	
	protected:
		virtual void Shutdown() override;
	
	private:
		static const XMVECTORF32 BackgroundColor;
		static const float BrightnessModulationRate;

		LPDIRECTINPUT8 mDirectInput;
		Keyboard* mKeyboard;
		Mouse* mMouse;
		XMFLOAT2 mMouseTextPosition;
		FirstPersonCamera* mCamera;

		Grid* mGrid;
		bool mShowGrid;

		//Demo scenes
		AmbientLightingDemo* mAmbientLightingDemo;
		ShadowMappingDemo* mShadowMappingDemo;
		PhysicallyBasedRenderingDemo* mPBRDemo;
		InstancingDemo* mInstancingDemo;
		FrustumCullingDemo* mFrustumCullingDemo;
		SubsurfaceScatteringDemo* mSubsurfaceScatteringDemo;
		VolumetricLightingDemo* mVolumetricLightingDemo;
		CollisionTestDemo* mCollisionTestDemo;
		WaterSimulationDemo* mWaterSimulationDemo;
		RenderStateHelper* mRenderStateHelper;

		FpsComponent* mFpsComponent;
		SpriteBatch* mSpriteBatch;
		SpriteFont* mSpriteFont;

		FullScreenRenderTarget* mRenderTarget;
		FullScreenQuad* mFullScreenQuad;


		void SetLevel(int level);
		void UpdateImGui();
	};
}