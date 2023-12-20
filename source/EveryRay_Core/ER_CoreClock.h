#pragma once

//
#include <windows.h>
#include <exception>

namespace EveryRay_Core
{
	class ER_CoreTime;

	class ER_CoreClock
	{
	public:
		ER_CoreClock();

		const LARGE_INTEGER& StartTime() const;
		const LARGE_INTEGER& CurrentTime() const;
		const LARGE_INTEGER& LastTime() const;

		void Reset();
		double GetFrequency() const;
		void GetTime(LARGE_INTEGER& time) const;
		void UpdateGameTime(ER_CoreTime& gameTime);

	private:
		ER_CoreClock(const ER_CoreClock& rhs);
		ER_CoreClock& operator=(const ER_CoreClock& rhs);

		LARGE_INTEGER mStartTime;
		LARGE_INTEGER mCurrentTime;
		LARGE_INTEGER mLastTime;
		double mFrequency;
	};
}