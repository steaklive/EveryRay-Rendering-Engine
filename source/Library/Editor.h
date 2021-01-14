#pragma once

#include "GameComponent.h"
#define MAX_OBJECTS_COUNT 1000
#define MAX_LOD 3

namespace Rendering
{
	class RenderingObject;
}

namespace Library
{
	class GameTime;
	class Scene;

	class Editor : public GameComponent
	{
		RTTI_DECLARATIONS(Editor, GameComponent)

	public:
		Editor(Game& game);
		virtual ~Editor();

		virtual void Initialize() override;
		virtual void Update(const GameTime& gameTime) override;
		void LoadScene(Scene* scene);

		XMFLOAT4 GetBottomSkyColor() { return XMFLOAT4(bottomColorSky[0],bottomColorSky[1],bottomColorSky[2],bottomColorSky[3]); }
		XMFLOAT4 GetTopSkyColor() { return XMFLOAT4(topColorSky[0], topColorSky[1], topColorSky[2], topColorSky[3]); }
		bool IsSkyboxUsingCustomColor() { return mUseCustomSkyboxColor; }
	private:
		Scene* mScene;

		Editor(const Editor& rhs);
		Editor& operator=(const Editor& rhs);

		const char* editorObjectsNames[MAX_OBJECTS_COUNT];

		bool mUseCustomSkyboxColor = true;
		float bottomColorSky[4] = {217.0f / 255.0f, 217.0f / 255.0f, 218.0f / 255.0f, 1.0f};
		float topColorSky[4] = { 175.0f / 255.0f, 200.0f / 255.0f, 211.0f / 255.0f, 1.0f };
	};
}