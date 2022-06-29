#include "stdafx.h"

#include "ER_CoreComponent.h"
#include "GameTime.h"

namespace Library
{
	RTTI_DEFINITIONS(ER_CoreComponent);

	ER_CoreComponent::ER_CoreComponent() : mGame(nullptr), mEnabled(true)
	{
	}

	ER_CoreComponent::ER_CoreComponent(Game& game) : mGame(&game), mEnabled(true)
	{	
	}
	ER_CoreComponent::~ER_CoreComponent()
	{
	}

	Game* ER_CoreComponent::GetGame()
	{
		return mGame;
	}

	void ER_CoreComponent::SetGame(Game& game)
	{
		mGame = &game;
	}

	bool ER_CoreComponent::Enabled() const
	{
		return mEnabled;
	}

	void ER_CoreComponent::SetEnabled(bool enabled)
	{
		mEnabled = enabled;
	}

	void ER_CoreComponent::Initialize()
	{
	}

	void ER_CoreComponent::Update(const GameTime& gameTime)
	{
	}
}