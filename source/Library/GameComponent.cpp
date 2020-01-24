#include "stdafx.h"

#include "GameComponent.h"
#include "GameTime.h"

namespace Library
{
	RTTI_DEFINITIONS(GameComponent);

	GameComponent::GameComponent() : mGame(nullptr), mEnabled(true)
	{
	}

	GameComponent::GameComponent(Game& game) : mGame(&game), mEnabled(true)
	{	
	}
	GameComponent::~GameComponent()
	{
	}

	Game* GameComponent::GetGame()
	{
		return mGame;
	}

	void GameComponent::SetGame(Game& game)
	{
		mGame = &game;
	}

	bool GameComponent::Enabled() const
	{
		return mEnabled;
	}

	void GameComponent::SetEnabled(bool enabled)
	{
		mEnabled = enabled;
	}

	void GameComponent::Initialize()
	{
	}

	void GameComponent::Update(const GameTime& gameTime)
	{
	}
}