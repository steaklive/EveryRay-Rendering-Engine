// Simple class for level management
// Works for now...
#pragma once
#include "Common.h"

namespace Rendering
{
    class PostProcessingStack;
}
namespace Library
{
    class Game;
    class GameTime;
    class Camera;
    class Scene;
    class Keyboard;
    class RenderStateHelper;
    class DirectionalLight;
    class Skybox;
    class Grid;
    class GBuffer;
    class ShadowMapper;
    class Terrain;
    class FoliageSystem;
    class VolumetricClouds;
    class Illumination;

	class DemoLevel
	{
	public:
		virtual void Destroy();
		virtual void Create();
		virtual void UpdateLevel(const GameTime& time);
		virtual void DrawLevel(const GameTime& time);
		virtual bool IsComponent() = 0;

        void Initialize(Game& game, Camera& camera, const std::string& sceneName);
    protected:
        Scene* mScene = nullptr;
        Keyboard* mKeyboard = nullptr;
        RenderStateHelper* mRenderStateHelper = nullptr;
        Skybox* mSkybox = nullptr;
        Grid* mGrid = nullptr;
        ShadowMapper* mShadowMapper = nullptr;
        DirectionalLight* mDirectionalLight = nullptr;
        GBuffer* mGBuffer = nullptr;
        FoliageSystem* mFoliageSystem = nullptr;
        VolumetricClouds* mVolumetricClouds = nullptr;
        Illumination* mIllumination = nullptr;

        Rendering::PostProcessingStack* mPostProcessingStack = nullptr;
    private:
        void UpdateImGui();
	};

}
