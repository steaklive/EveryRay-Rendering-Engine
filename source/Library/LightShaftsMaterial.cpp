#include "stdafx.h"
#include "LightShaftsMaterial.h"
#include "GameException.h"

namespace Rendering
{
	RTTI_DEFINITIONS(LightShaftsMaterial)

	LightShaftsMaterial::LightShaftsMaterial()
		: PostProcessingMaterial()
		, MATERIAL_VARIABLE_INITIALIZATION(DepthTexture)
		, MATERIAL_VARIABLE_INITIALIZATION(OcclusionTexture)
		, MATERIAL_VARIABLE_INITIALIZATION(SunPosNDC)
		, MATERIAL_VARIABLE_INITIALIZATION(Density)
		, MATERIAL_VARIABLE_INITIALIZATION(Weight)
		, MATERIAL_VARIABLE_INITIALIZATION(Exposure)
		, MATERIAL_VARIABLE_INITIALIZATION(Decay)
		, MATERIAL_VARIABLE_INITIALIZATION(MaxSunDistanceDelta)
		, MATERIAL_VARIABLE_INITIALIZATION(Intensity)
	{
	}

	MATERIAL_VARIABLE_DEFINITION(LightShaftsMaterial, DepthTexture)
	MATERIAL_VARIABLE_DEFINITION(LightShaftsMaterial, OcclusionTexture)
	MATERIAL_VARIABLE_DEFINITION(LightShaftsMaterial, SunPosNDC)
	MATERIAL_VARIABLE_DEFINITION(LightShaftsMaterial, Density)
	MATERIAL_VARIABLE_DEFINITION(LightShaftsMaterial, Weight)
	MATERIAL_VARIABLE_DEFINITION(LightShaftsMaterial, Exposure)
	MATERIAL_VARIABLE_DEFINITION(LightShaftsMaterial, Decay)
	MATERIAL_VARIABLE_DEFINITION(LightShaftsMaterial, MaxSunDistanceDelta)
	MATERIAL_VARIABLE_DEFINITION(LightShaftsMaterial, Intensity)

	void LightShaftsMaterial::Initialize(Effect* effect)
	{
		PostProcessingMaterial::Initialize(effect);

		MATERIAL_VARIABLE_RETRIEVE(DepthTexture)
		MATERIAL_VARIABLE_RETRIEVE(OcclusionTexture)
		MATERIAL_VARIABLE_RETRIEVE(SunPosNDC)
		MATERIAL_VARIABLE_RETRIEVE(Density)
		MATERIAL_VARIABLE_RETRIEVE(Weight)
		MATERIAL_VARIABLE_RETRIEVE(Exposure)
		MATERIAL_VARIABLE_RETRIEVE(Decay)
		MATERIAL_VARIABLE_RETRIEVE(MaxSunDistanceDelta)
		MATERIAL_VARIABLE_RETRIEVE(Intensity)
	}
}