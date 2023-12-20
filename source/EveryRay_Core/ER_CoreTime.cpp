//
#include "ER_CoreTime.h"

namespace EveryRay_Core
{
	ER_CoreTime::ER_CoreTime()
		: mTotalCoreTime(0.0), mElapsedCoreTime(0.0)
	{
	}

	double ER_CoreTime::TotalCoreTime() const
	{
		return mTotalCoreTime;
	}

	void ER_CoreTime::SetTotalCoreTime(double totalGameTime)
	{
		mTotalCoreTime = totalGameTime;
	}

	double ER_CoreTime::ElapsedCoreTime() const
	{
		return mElapsedCoreTime;
	}

	void ER_CoreTime::SetElapsedCoreTime(double elapsedGameTime)
	{
		mElapsedCoreTime = elapsedGameTime;
	}
}