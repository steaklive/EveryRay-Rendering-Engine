#include "stdafx.h"

#include "ER_ColorHelper.h"


namespace Library
{
	std::random_device ER_ColorHelper::sDevice;
	std::default_random_engine ER_ColorHelper::sGenerator(sDevice());
	std::uniform_real_distribution<float> ER_ColorHelper::sDistribution(0, 1);

	const XMVECTORF32 ER_ColorHelper::Black = { 0.0f, 0.0f, 0.0f, 1.0f };
	const XMVECTORF32 ER_ColorHelper::White = { 1.0f, 1.0f, 1.0f, 1.0f };
	const XMVECTORF32 ER_ColorHelper::Red = { 1.0f, 0.0f, 0.0f, 1.0f };
	const XMVECTORF32 ER_ColorHelper::Green = { 0.0f, 1.0f, 0.0f, 1.0f };
	const XMVECTORF32 ER_ColorHelper::Blue = { 0.0f, 0.0f, 1.0f, 1.0f };
	const XMVECTORF32 ER_ColorHelper::Yellow = { 1.0f, 1.0f, 0.0f, 1.0f };
	const XMVECTORF32 ER_ColorHelper::BlueGreen = { 0.0f, 1.0f, 1.0f, 1.0f };
	const XMVECTORF32 ER_ColorHelper::Purple = { 1.0f, 0.0f, 1.0f, 1.0f };
	const XMVECTORF32 ER_ColorHelper::CornflowerBlue = { 0.392f, 0.584f, 0.929f, 1.0f };
	const XMVECTORF32 ER_ColorHelper::Wheat = { 0.961f, 0.871f, 0.702f, 1.0f };
	const XMVECTORF32 ER_ColorHelper::LightGray = { 0.5f, 0.5f, 0.5f, 1.0f };
	const XMVECTORF32 ER_ColorHelper::Orange = { 1.0f, 0.65f, 0.0f, 1.0f };


	XMVECTORF32 ER_ColorHelper::RandomColor()
	{
		float r = sDistribution(sGenerator);
		float g = sDistribution(sGenerator);
		float b = sDistribution(sGenerator);

		return XMVECTORF32{ r, g, b, 1.0f };
	}
}