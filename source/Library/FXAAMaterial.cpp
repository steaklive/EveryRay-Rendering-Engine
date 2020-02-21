#include "stdafx.h"
#include "FXAAMaterial.h"
#include "GameException.h"

namespace Rendering
{
	RTTI_DEFINITIONS(FXAAMaterial)

		FXAAMaterial::FXAAMaterial()
		: PostProcessingMaterial(),
		MATERIAL_VARIABLE_INITIALIZATION(Width),
		MATERIAL_VARIABLE_INITIALIZATION(Height)
	{
	}

	MATERIAL_VARIABLE_DEFINITION(FXAAMaterial, Width)
	MATERIAL_VARIABLE_DEFINITION(FXAAMaterial, Height)

	void FXAAMaterial::Initialize(Effect* effect)
	{
		PostProcessingMaterial::Initialize(effect);

		MATERIAL_VARIABLE_RETRIEVE(Width)
		MATERIAL_VARIABLE_RETRIEVE(Height)
	}
}