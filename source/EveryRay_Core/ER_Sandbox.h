#pragma once
#include "Common.h"

namespace EveryRay_Core
{
    class ER_Core;
    class ER_CoreTime;
    class ER_Camera;
    class ER_Scene;
    class ER_Editor;
    class ER_Keyboard;
    class ER_DirectionalLight;
    class ER_PointLight;
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
    class ER_QuadRenderer;
    class ER_GPUCuller;

	class ER_Sandbox
	{
	public:
        ER_Sandbox();
        ~ER_Sandbox();

        void Initialize(ER_Core& game, ER_Camera& camera, const std::string& sceneName, const std::string& sceneFolderPath);
		virtual void Destroy(ER_Core& game);
		virtual void Update(ER_Core& game, const ER_CoreTime& time);
		virtual void Draw(ER_Core& game, const ER_CoreTime& time);

        ER_Scene* mScene = nullptr;
		ER_Editor* mEditor = nullptr;
        ER_Keyboard* mKeyboard = nullptr;
        ER_Skybox* mSkybox = nullptr;
        ER_ShadowMapper* mShadowMapper = nullptr;
        ER_DirectionalLight* mDirectionalLight = nullptr;
        ER_GBuffer* mGBuffer = nullptr;
        ER_FoliageManager* mFoliageSystem = nullptr;
        ER_VolumetricClouds* mVolumetricClouds = nullptr;
        ER_VolumetricFog* mVolumetricFog = nullptr;
        ER_Illumination* mIllumination = nullptr;
        ER_LightProbesManager* mLightProbesManager = nullptr;
        ER_Terrain* mTerrain = nullptr;
        ER_PostProcessingStack* mPostProcessingStack = nullptr;
        ER_QuadRenderer* mQuadRenderer = nullptr;
        ER_GPUCuller* mGPUCuller = nullptr;

        std::vector<ER_PointLight*> mPointLights;
    private:
        void UpdateImGui();
        std::string mName;

        XMMATRIX mDefaultSunRotationMatrix;

		float mWindStrength = 1.0f;
		float mWindFrequency = 1.0f;
		float mWindGustDistance = 1.0f;
	};

}
