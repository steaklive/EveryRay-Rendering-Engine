#pragma once

#include "Common.h"

#include <random>

namespace Library
{
	class ColorHelper
	{
	public:
		static const XMVECTORF32 Black;
		static const XMVECTORF32 White;
		static const XMVECTORF32 Red;
		static const XMVECTORF32 Green;
		static const XMVECTORF32 Blue;
		static const XMVECTORF32 Yellow;
		static const XMVECTORF32 BlueGreen;
		static const XMVECTORF32 Purple;
		static const XMVECTORF32 CornflowerBlue;
		static const XMVECTORF32 Wheat;
		static const XMVECTORF32 LightGray;
		static const XMVECTORF32 Orange;

		static XMVECTORF32 RandomColor();

	private:
		static std::random_device sDevice;
		static std::default_random_engine sGenerator;
		static std::uniform_real_distribution<float> sDistribution;

		ColorHelper();
		ColorHelper(const ColorHelper& rhs);
		ColorHelper& operator=(const ColorHelper& rhs);
	};
}