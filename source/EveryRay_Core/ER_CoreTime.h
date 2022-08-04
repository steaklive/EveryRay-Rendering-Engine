#pragma once
#include "stdafx.h"

namespace EveryRay_Core
{
	class ER_CoreTime
	{
	public:
		ER_CoreTime();

		double TotalCoreTime() const;
		void SetTotalCoreTime(double totalGameTime);

		double ElapsedCoreTime() const;
		void SetElapsedCoreTime(double elapsedGameTime);

	private:
		double mTotalCoreTime;
		double mElapsedCoreTime;
	};
}