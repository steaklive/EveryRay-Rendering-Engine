#include "stdafx.h"

#include "DrawableGameComponent.h"

namespace Library {
	RTTI_DEFINITIONS(DrawableGameComponent)

	DrawableGameComponent::DrawableGameComponent() :GameComponent(), mVisible(true), mCamera(nullptr)
	{
	}
	DrawableGameComponent::DrawableGameComponent(Game& game)
		: GameComponent(game), mVisible(true), mCamera(nullptr)
	{
	}

	DrawableGameComponent::DrawableGameComponent(Game& game, Camera& camera)
		: GameComponent(game), mVisible(true), mCamera(&camera)
	{
	}

	DrawableGameComponent::DrawableGameComponent(Game& game, Camera& camera, Editor& editor)
		: GameComponent(game), mVisible(true), mCamera(&camera), mEditor(&editor)
	{
	}

	DrawableGameComponent::~DrawableGameComponent()
	{
	}

	bool DrawableGameComponent::Visible() const
	{
		return mVisible;
	}

	void DrawableGameComponent::SetVisible(bool visible)
	{
		mVisible = visible;
	}

	Camera* DrawableGameComponent::GetCamera()
	{
		return mCamera;
	}

	void DrawableGameComponent::SetCamera(Camera* camera)
	{
		mCamera = camera;
	}
	void DrawableGameComponent::SetEditor(Editor* editor)
	{
		mEditor = editor;
	}
	void DrawableGameComponent::Draw(const GameTime& gameTime)
	{
	}


}