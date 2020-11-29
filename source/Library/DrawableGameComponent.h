#pragma once

#include "GameComponent.h"

namespace Library
{
	class Camera;
	class Editor;

	class DrawableGameComponent : public GameComponent {

		RTTI_DECLARATIONS(DrawableGameComponent, GameComponent)

	public:
		DrawableGameComponent();
		DrawableGameComponent(Game& game);
		DrawableGameComponent(Game& game, Camera& camera);
		DrawableGameComponent(Game& game, Camera& camera, Editor& editor);
		virtual ~DrawableGameComponent();

		bool Visible() const;
		void SetVisible(bool visible);

		Camera* GetCamera();
		void SetCamera(Camera* camera);
		void SetEditor(Editor* editor);

		virtual void Draw(const GameTime& gameTime);

	protected:
		bool mVisible;
		Camera* mCamera;
		Editor* mEditor;
	
	private:
		DrawableGameComponent(const DrawableGameComponent& rhs);
		DrawableGameComponent& operator=(const DrawableGameComponent& rhs);
	};
}