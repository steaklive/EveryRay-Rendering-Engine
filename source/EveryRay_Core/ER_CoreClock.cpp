#include "stdafx.h"
#include "ER_CoreClock.h"
#include "ER_CoreTime.h"

namespace EveryRay_Core
{
	ER_CoreClock::ER_CoreClock()
		: mStartTime(), mCurrentTime(), mLastTime(), mFrequency()
	{
		mFrequency = GetFrequency();
		Reset();
	}

	const LARGE_INTEGER& ER_CoreClock::StartTime() const
	{
		return mStartTime;
	}

	const LARGE_INTEGER& ER_CoreClock::CurrentTime() const
	{
		return mCurrentTime;
	}

	const LARGE_INTEGER& ER_CoreClock::LastTime() const
	{
		return mLastTime;
	}

	void ER_CoreClock::Reset()
	{
		GetTime(mStartTime);
		mCurrentTime = mStartTime;
		mLastTime = mCurrentTime;
	}

	double ER_CoreClock::GetFrequency() const
	{
		LARGE_INTEGER frequency;

		if (QueryPerformanceFrequency(&frequency) == false)
		{
			throw std::exception("QueryPerformanceFrequency() failed.");
		}

		return (double)frequency.QuadPart;
	}

	void ER_CoreClock::GetTime(LARGE_INTEGER& time) const
	{
		QueryPerformanceCounter(&time);
	}

	void ER_CoreClock::UpdateGameTime(ER_CoreTime& gameTime)
	{
		GetTime(mCurrentTime);
		gameTime.SetTotalGameTime((mCurrentTime.QuadPart - mStartTime.QuadPart) / mFrequency);
		gameTime.SetElapsedGameTime((mCurrentTime.QuadPart - mLastTime.QuadPart) / mFrequency);

		mLastTime = mCurrentTime;
	}
}