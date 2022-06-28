#pragma once

#include "GameComponent.h"

namespace Library
{
	class ER_Camera;
	class ER_Editor;

	class DrawableGameComponent : public GameComponent {

		RTTI_DECLARATIONS(DrawableGameComponent, GameComponent)

	public:
		DrawableGameComponent();
		DrawableGameComponent(Game& game);
		DrawableGameComponent(Game& game, ER_Camera& camera);
		virtual ~DrawableGameComponent();

		bool Visible() const;
		void SetVisible(bool visible);

		ER_Camera* GetCamera();
		void SetCamera(ER_Camera* camera);
		void SetEditor(ER_Editor* editor);

		virtual void Draw(const GameTime& gameTime);

	protected:
		bool mVisible;
		ER_Camera* mCamera;
	
	private:
		DrawableGameComponent(const DrawableGameComponent& rhs);
		DrawableGameComponent& operator=(const DrawableGameComponent& rhs);
	};
}