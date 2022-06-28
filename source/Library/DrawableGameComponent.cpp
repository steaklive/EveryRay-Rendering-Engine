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

	DrawableGameComponent::DrawableGameComponent(Game& game, ER_Camera& camera)
		: GameComponent(game), mVisible(true), mCamera(&camera)
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

	ER_Camera* DrawableGameComponent::GetCamera()
	{
		return mCamera;
	}

	void DrawableGameComponent::SetCamera(ER_Camera* camera)
	{
		mCamera = camera;
	}
	void DrawableGameComponent::Draw(const GameTime& gameTime)
	{
	}


}