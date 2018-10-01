#include "stdafx.h"
#include "GameClock.h"
#include "GameTime.h"

namespace Library
{
	GameClock::GameClock()
		: mStartTime(), mCurrentTime(), mLastTime(), mFrequency()
	{
		mFrequency = GetFrequency();
		Reset();
	}

	const LARGE_INTEGER& GameClock::StartTime() const
	{
		return mStartTime;
	}

	const LARGE_INTEGER& GameClock::CurrentTime() const
	{
		return mCurrentTime;
	}

	const LARGE_INTEGER& GameClock::LastTime() const
	{
		return mLastTime;
	}

	void GameClock::Reset()
	{
		GetTime(mStartTime);
		mCurrentTime = mStartTime;
		mLastTime = mCurrentTime;
	}

	double GameClock::GetFrequency() const
	{
		LARGE_INTEGER frequency;

		if (QueryPerformanceFrequency(&frequency) == false)
		{
			throw std::exception("QueryPerformanceFrequency() failed.");
		}

		return (double)frequency.QuadPart;
	}

	void GameClock::GetTime(LARGE_INTEGER& time) const
	{
		QueryPerformanceCounter(&time);
	}

	void GameClock::UpdateGameTime(GameTime& gameTime)
	{
		GetTime(mCurrentTime);
		gameTime.SetTotalGameTime((mCurrentTime.QuadPart - mStartTime.QuadPart) / mFrequency);
		gameTime.SetElapsedGameTime((mCurrentTime.QuadPart - mLastTime.QuadPart) / mFrequency);

		mLastTime = mCurrentTime;
	}
}