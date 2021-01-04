#include "stdafx.h"
#include "FogMaterial.h"
#include "GameException.h"

namespace Rendering
{
	RTTI_DEFINITIONS(FogMaterial)

	FogMaterial::FogMaterial()
		: PostProcessingMaterial(),
		MATERIAL_VARIABLE_INITIALIZATION(DepthTexture),
		MATERIAL_VARIABLE_INITIALIZATION(FogColor),
		MATERIAL_VARIABLE_INITIALIZATION(FogNear),
		MATERIAL_VARIABLE_INITIALIZATION(FogFar),
		MATERIAL_VARIABLE_INITIALIZATION(FogDensity)

	{
	}
	MATERIAL_VARIABLE_DEFINITION(FogMaterial, DepthTexture)
	MATERIAL_VARIABLE_DEFINITION(FogMaterial, FogColor)
	MATERIAL_VARIABLE_DEFINITION(FogMaterial, FogNear)
	MATERIAL_VARIABLE_DEFINITION(FogMaterial, FogFar)
	MATERIAL_VARIABLE_DEFINITION(FogMaterial, FogDensity)

	void FogMaterial::Initialize(Effect* effect)
	{
		PostProcessingMaterial::Initialize(effect);
		MATERIAL_VARIABLE_RETRIEVE(DepthTexture)
		MATERIAL_VARIABLE_RETRIEVE(FogColor)
		MATERIAL_VARIABLE_RETRIEVE(FogNear)
		MATERIAL_VARIABLE_RETRIEVE(FogFar)
		MATERIAL_VARIABLE_RETRIEVE(FogDensity)
	}
}