#pragma once

#include "GameComponent.h"

namespace Library
{
	class Camera;
	class ER_Editor;

	class DrawableGameComponent : public GameComponent {

		RTTI_DECLARATIONS(DrawableGameComponent, GameComponent)

	public:
		DrawableGameComponent();
		DrawableGameComponent(Game& game);
		DrawableGameComponent(Game& game, Camera& camera);
		virtual ~DrawableGameComponent();

		bool Visible() const;
		void SetVisible(bool visible);

		Camera* GetCamera();
		void SetCamera(Camera* camera);
		void SetEditor(ER_Editor* editor);

		virtual void Draw(const GameTime& gameTime);

	protected:
		bool mVisible;
		Camera* mCamera;
	
	private:
		DrawableGameComponent(const DrawableGameComponent& rhs);
		DrawableGameComponent& operator=(const DrawableGameComponent& rhs);
	};
}