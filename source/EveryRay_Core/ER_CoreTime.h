#pragma once
#include "stdafx.h"

namespace EveryRay_Core
{
	class ER_CoreTime
	{
	public:
		ER_CoreTime();

		double TotalGameTime() const;
		void SetTotalGameTime(double totalGameTime);

		double ElapsedGameTime() const;
		void SetElapsedGameTime(double elapsedGameTime);

	private:
		double mTotalGameTime;
		double mElapsedGameTime;
	};
}