// A simple UI scene editor for modifying objects and some systems (skybox, etc.)
#pragma once

#include "ER_CoreComponent.h"
#define MAX_OBJECTS_IN_EDITOR_COUNT 1024
#define MAX_POINT_LIGHTS_IN_EDITOR_COUNT MAX_NUM_POINT_LIGHTS
#define MAX_SYMBOLS_PER_NAME 64

namespace EveryRay_Core
{
	class ER_RenderingObject;
	class ER_CoreTime;
	class ER_Scene;

	class ER_Editor : public ER_CoreComponent
	{
		RTTI_DECLARATIONS(ER_Editor, ER_CoreComponent)

	public:
		ER_Editor(ER_Core& game);
		virtual ~ER_Editor();

		void Initialize(ER_Scene* scene);
		virtual void Update(const ER_CoreTime& gameTime) override;

		bool IsSkyboxUsingCustomColor() { return mUseCustomSkyboxColor; }
		XMFLOAT4 GetBottomSkyColor() { return XMFLOAT4(mBottomColorSky[0],mBottomColorSky[1],mBottomColorSky[2],mBottomColorSky[3]); }
		XMFLOAT4 GetTopSkyColor() { return XMFLOAT4(mTopColorSky[0], mTopColorSky[1], mTopColorSky[2], mTopColorSky[3]); }
		float GetSkyMinHeight() { return mSkyMinHeight; }
		float GetSkyMaxHeight() { return mSkyMaxHeight; }
	private:
		ER_Scene* mScene = nullptr;

		ER_Editor(const ER_Editor& rhs);
		ER_Editor& operator=(const ER_Editor& rhs);

		char* mEditorObjectsNames[MAX_OBJECTS_IN_EDITOR_COUNT] = { nullptr };
		char* mEditorPointLightsNames[MAX_POINT_LIGHTS_IN_EDITOR_COUNT] = { nullptr };

		bool mUseCustomSkyboxColor = true;
		float mBottomColorSky[4] = {245.0f / 255.0f, 245.0f / 255.0f, 245.0f / 255.0f, 1.0f};
		float mTopColorSky[4] = { 0.0f / 255.0f, 133.0f / 255.0f, 191.0f / 255.0f, 1.0f };
		float mSkyMinHeight = 0.191f;
		float mSkyMaxHeight = 4.2f;
	};
}