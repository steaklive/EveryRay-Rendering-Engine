#pragma once
#include "Common.h"
#include <chrono>

namespace EveryRay_Core
{
	typedef std::chrono::high_resolution_clock::time_point TimePoint;

	class ER_CPUProfiler
	{
	public:
		ER_CPUProfiler();
		~ER_CPUProfiler();

		void BeginCPUTime(const std::string& aEventName, bool toLog = true);
		void EndCPUTime(const std::string& aEventName);

	private:
		std::map<std::string, TimePoint> mEventsCPUTime;
	};
}