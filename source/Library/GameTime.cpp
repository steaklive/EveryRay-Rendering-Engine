#include "stdafx.h"
#include "GameTime.h"

namespace Library
{
	GameTime::GameTime()
		: mTotalGameTime(0.0), mElapsedGameTime(0.0)
	{
	}

	double GameTime::TotalGameTime() const
	{
		return mTotalGameTime;
	}

	void GameTime::SetTotalGameTime(double totalGameTime)
	{
		mTotalGameTime = totalGameTime;
	}

	double GameTime::ElapsedGameTime() const
	{
		return mElapsedGameTime;
	}

	void GameTime::SetElapsedGameTime(double elapsedGameTime)
	{
		mElapsedGameTime = elapsedGameTime;
	}
}