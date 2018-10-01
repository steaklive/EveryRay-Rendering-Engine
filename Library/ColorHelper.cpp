#include "stdafx.h"

#include "ColorHelper.h"


namespace Library
{
	std::random_device ColorHelper::sDevice;
	std::default_random_engine ColorHelper::sGenerator(sDevice());
	std::uniform_real_distribution<float> ColorHelper::sDistribution(0, 1);

	const XMVECTORF32 ColorHelper::Black = { 0.0f, 0.0f, 0.0f, 1.0f };
	const XMVECTORF32 ColorHelper::White = { 1.0f, 1.0f, 1.0f, 1.0f };
	const XMVECTORF32 ColorHelper::Red = { 1.0f, 0.0f, 0.0f, 1.0f };
	const XMVECTORF32 ColorHelper::Green = { 0.0f, 1.0f, 0.0f, 1.0f };
	const XMVECTORF32 ColorHelper::Blue = { 0.0f, 0.0f, 1.0f, 1.0f };
	const XMVECTORF32 ColorHelper::Yellow = { 1.0f, 1.0f, 0.0f, 1.0f };
	const XMVECTORF32 ColorHelper::BlueGreen = { 0.0f, 1.0f, 1.0f, 1.0f };
	const XMVECTORF32 ColorHelper::Purple = { 1.0f, 0.0f, 1.0f, 1.0f };
	const XMVECTORF32 ColorHelper::CornflowerBlue = { 0.392f, 0.584f, 0.929f, 1.0f };
	const XMVECTORF32 ColorHelper::Wheat = { 0.961f, 0.871f, 0.702f, 1.0f };
	const XMVECTORF32 ColorHelper::LightGray = { 0.5f, 0.5f, 0.5f, 1.0f };
	const XMVECTORF32 ColorHelper::Orange = { 1.0f, 0.65f, 0.0f, 1.0f };


	XMVECTORF32 ColorHelper::RandomColor()
	{
		float r = sDistribution(sGenerator);
		float g = sDistribution(sGenerator);
		float b = sDistribution(sGenerator);

		return XMVECTORF32{ r, g, b, 1.0f };
	}
}