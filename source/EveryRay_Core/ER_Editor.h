// A simple UI scene editor for modifying objects and some systems (skybox, etc.)
#pragma once

#include "ER_CoreComponent.h"
#define MAX_OBJECTS_IN_EDITOR_COUNT 1024
#define MAX_POINT_LIGHTS_IN_EDITOR_COUNT MAX_NUM_POINT_LIGHTS
#define MAX_SYMBOLS_PER_NAME 64

namespace EveryRay_Core
{
	class ER_CameraFPS;
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
	private:
		ER_Scene* mScene = nullptr;
		ER_CameraFPS* mCamera = nullptr;

		ER_Editor(const ER_Editor& rhs);
		ER_Editor& operator=(const ER_Editor& rhs);

		char* mEditorObjectsNames[MAX_OBJECTS_IN_EDITOR_COUNT] = { nullptr };
		char* mEditorPointLightsNames[MAX_POINT_LIGHTS_IN_EDITOR_COUNT] = { nullptr };
	};
}