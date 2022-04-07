// Simple class for level management
// Works for now...
#pragma once
#include "Common.h"

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
    class ER_Skybox;
    class ER_GBuffer;
    class ER_ShadowMapper;
    class Terrain;
    class ER_FoliageManager;
    class VolumetricClouds;
    class Illumination;
    class ER_IlluminationProbeManager;
    class ER_PostProcessingStack;

	class DemoLevel
	{
	public:
        DemoLevel();
        ~DemoLevel();

        void Initialize(Game& game, Camera& camera, const std::string& sceneName, const std::string& sceneFolderPath);
		virtual void Destroy(Game& game);
		virtual void UpdateLevel(Game& game, const GameTime& time);
		virtual void DrawLevel(Game& game, const GameTime& time);

        Scene* mScene = nullptr;
		Editor* mEditor = nullptr;
        Keyboard* mKeyboard = nullptr;
        RenderStateHelper* mRenderStateHelper = nullptr;
        ER_Skybox* mSkybox = nullptr;
        ER_ShadowMapper* mShadowMapper = nullptr;
        DirectionalLight* mDirectionalLight = nullptr;
        ER_GBuffer* mGBuffer = nullptr;
        ER_FoliageManager* mFoliageSystem = nullptr;
        VolumetricClouds* mVolumetricClouds = nullptr;
        Illumination* mIllumination = nullptr;
        ER_IlluminationProbeManager* mIlluminationProbesManager = nullptr;
        ER_PostProcessingStack* mPostProcessingStack = nullptr; //TODO remove namespace
    private:
        void UpdateImGui();
        std::string mName;

		float mWindStrength = 1.0f;
		float mWindFrequency = 1.0f;
		float mWindGustDistance = 1.0f;
	};

}
