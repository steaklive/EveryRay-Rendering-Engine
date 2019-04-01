#include "stdafx.h"
#include "ColorGradingMaterial.h"
#include "GameException.h"

namespace Rendering
{
	RTTI_DEFINITIONS(ColorGradingMaterial)

	ColorGradingMaterial::ColorGradingMaterial()
		: PostProcessingMaterial(),
		MATERIAL_VARIABLE_INITIALIZATION(lut)

	{
	}

	MATERIAL_VARIABLE_DEFINITION(ColorGradingMaterial, lut)


	void ColorGradingMaterial::Initialize(Effect* effect)
	{
		PostProcessingMaterial::Initialize(effect);

		MATERIAL_VARIABLE_RETRIEVE(lut)

	}
}