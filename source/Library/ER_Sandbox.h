#pragma once
#include "Common.h"

namespace Library
{
    class Game;
    class GameTime;
    class ER_Camera;
    class ER_Scene;
    class ER_Editor;
    class Keyboard;
    class RenderStateHelper;
    class DirectionalLight;
    class ER_Skybox;
    class ER_GBuffer;
    class ER_ShadowMapper;
    class ER_Terrain;
    class ER_FoliageManager;
    class ER_VolumetricClouds;
	class ER_VolumetricFog;
    class ER_Illumination;
    class ER_LightProbesManager;
    class ER_PostProcessingStack;

	class ER_Sandbox
	{
	public:
        ER_Sandbox();
        ~ER_Sandbox();

        void Initialize(Game& game, ER_Camera& camera, const std::string& sceneName, const std::string& sceneFolderPath);
		virtual void Destroy(Game& game);
		virtual void Update(Game& game, const GameTime& time);
		virtual void Draw(Game& game, const GameTime& time);

        ER_Scene* mScene = nullptr;
		ER_Editor* mEditor = nullptr;
        Keyboard* mKeyboard = nullptr;
        RenderStateHelper* mRenderStateHelper = nullptr;
        ER_Skybox* mSkybox = nullptr;
        ER_ShadowMapper* mShadowMapper = nullptr;
        DirectionalLight* mDirectionalLight = nullptr;
        ER_GBuffer* mGBuffer = nullptr;
        ER_FoliageManager* mFoliageSystem = nullptr;
        ER_VolumetricClouds* mVolumetricClouds = nullptr;
        ER_VolumetricFog* mVolumetricFog = nullptr;
        ER_Illumination* mIllumination = nullptr;
        ER_LightProbesManager* mLightProbesManager = nullptr;
        ER_Terrain* mTerrain = nullptr;
        ER_PostProcessingStack* mPostProcessingStack = nullptr;
    private:
        void UpdateImGui();
        std::string mName;

		float mWindStrength = 1.0f;
		float mWindFrequency = 1.0f;
		float mWindGustDistance = 1.0f;
	};

}
