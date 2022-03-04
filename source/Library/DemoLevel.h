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
    class Editor;
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
    class ER_IlluminationProbeManager;

	class DemoLevel
	{
	public:
        DemoLevel();
        ~DemoLevel();

        void Initialize(Game& game, Camera& camera, const std::string& sceneName, const std::string& sceneFolderPath);
		virtual void Destroy();
		virtual void UpdateLevel(Game& game, const GameTime& time);
		virtual void DrawLevel(Game& game, const GameTime& time);
    protected:
        Scene* mScene = nullptr;
		Editor* mEditor = nullptr;
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
        ER_IlluminationProbeManager* mIlluminationProbesManager = nullptr;
        Rendering::PostProcessingStack* mPostProcessingStack = nullptr; //TODO remove namespace
    private:
        void UpdateImGui();
	};

}
