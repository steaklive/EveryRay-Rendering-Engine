#include "stdafx.h"
#include "VignetteMaterial.h"
#include "GameException.h"

namespace Rendering
{
	RTTI_DEFINITIONS(VignetteMaterial)

	VignetteMaterial::VignetteMaterial()
		: PostProcessingMaterial(),
		MATERIAL_VARIABLE_INITIALIZATION(radius),
		MATERIAL_VARIABLE_INITIALIZATION(softness)
	{
	}

	MATERIAL_VARIABLE_DEFINITION(VignetteMaterial, radius)
	MATERIAL_VARIABLE_DEFINITION(VignetteMaterial, softness)

	void VignetteMaterial::Initialize(Effect* effect)
	{
		PostProcessingMaterial::Initialize(effect);

		MATERIAL_VARIABLE_RETRIEVE(radius)
		MATERIAL_VARIABLE_RETRIEVE(softness)
	}
}