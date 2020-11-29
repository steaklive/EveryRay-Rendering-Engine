#pragma once

#include "GameComponent.h"
#define MAX_OBJECTS_COUNT 1000

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

	private:
		Scene* mScene;

		Editor(const Editor& rhs);
		Editor& operator=(const Editor& rhs);

		const char* editorObjectsNames[MAX_OBJECTS_COUNT];
	};
}