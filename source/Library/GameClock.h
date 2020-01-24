#pragma once

#include "stdafx.h"
#include <windows.h>
#include <exception>

namespace Library
{
	class GameTime;

	class GameClock
	{
	public:
		GameClock();

		const LARGE_INTEGER& StartTime() const;
		const LARGE_INTEGER& CurrentTime() const;
		const LARGE_INTEGER& LastTime() const;

		void Reset();
		double GetFrequency() const;
		void GetTime(LARGE_INTEGER& time) const;
		void UpdateGameTime(GameTime& gameTime);

	private:
		GameClock(const GameClock& rhs);
		GameClock& operator=(const GameClock& rhs);

		LARGE_INTEGER mStartTime;
		LARGE_INTEGER mCurrentTime;
		LARGE_INTEGER mLastTime;
		double mFrequency;
	};
}