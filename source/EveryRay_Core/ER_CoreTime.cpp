#include "stdafx.h"
#include "ER_CoreTime.h"

namespace EveryRay_Core
{
	ER_CoreTime::ER_CoreTime()
		: mTotalGameTime(0.0), mElapsedGameTime(0.0)
	{
	}

	double ER_CoreTime::TotalGameTime() const
	{
		return mTotalGameTime;
	}

	void ER_CoreTime::SetTotalGameTime(double totalGameTime)
	{
		mTotalGameTime = totalGameTime;
	}

	double ER_CoreTime::ElapsedGameTime() const
	{
		return mElapsedGameTime;
	}

	void ER_CoreTime::SetElapsedGameTime(double elapsedGameTime)
	{
		mElapsedGameTime = elapsedGameTime;
	}
}