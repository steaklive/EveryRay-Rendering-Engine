#include "stdafx.h"

#include "ER_CoreComponent.h"
#include "ER_CoreTime.h"

namespace EveryRay_Core
{
	RTTI_DEFINITIONS(ER_CoreComponent);

	ER_CoreComponent::ER_CoreComponent() : mCore(nullptr), mEnabled(true)
	{
	}

	ER_CoreComponent::ER_CoreComponent(ER_Core& game) : mCore(&game), mEnabled(true)
	{	
	}
	ER_CoreComponent::~ER_CoreComponent()
	{
	}

	ER_Core* ER_CoreComponent::GetCore()
	{
		return mCore;
	}

	void ER_CoreComponent::SetCore(ER_Core& game)
	{
		mCore = &game;
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

	void ER_CoreComponent::Update(const ER_CoreTime& gameTime)
	{
	}
}