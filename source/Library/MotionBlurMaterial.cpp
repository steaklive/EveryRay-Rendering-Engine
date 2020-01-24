#include "stdafx.h"
#include "MotionBlurMaterial.h"
#include "GameException.h"

namespace Rendering
{
	RTTI_DEFINITIONS(MotionBlurMaterial)

	MotionBlurMaterial::MotionBlurMaterial()
		: PostProcessingMaterial(),
		MATERIAL_VARIABLE_INITIALIZATION(DepthTexture),
		MATERIAL_VARIABLE_INITIALIZATION(WorldViewProjection),
		MATERIAL_VARIABLE_INITIALIZATION(InvWorldViewProjection),
		MATERIAL_VARIABLE_INITIALIZATION(preWorldViewProjection),
		MATERIAL_VARIABLE_INITIALIZATION(preInvWorldViewProjection),
		MATERIAL_VARIABLE_INITIALIZATION(WorldInverseTranspose),
		MATERIAL_VARIABLE_INITIALIZATION(amount)

	{
	}
	MATERIAL_VARIABLE_DEFINITION(MotionBlurMaterial, DepthTexture)
	MATERIAL_VARIABLE_DEFINITION(MotionBlurMaterial, WorldViewProjection)
	MATERIAL_VARIABLE_DEFINITION(MotionBlurMaterial, InvWorldViewProjection)
	MATERIAL_VARIABLE_DEFINITION(MotionBlurMaterial, preWorldViewProjection)
	MATERIAL_VARIABLE_DEFINITION(MotionBlurMaterial, preInvWorldViewProjection)
		MATERIAL_VARIABLE_DEFINITION(MotionBlurMaterial, WorldInverseTranspose)
		MATERIAL_VARIABLE_DEFINITION(MotionBlurMaterial, amount)

	void MotionBlurMaterial::Initialize(Effect* effect)
	{
		PostProcessingMaterial::Initialize(effect);
		MATERIAL_VARIABLE_RETRIEVE(DepthTexture)
		MATERIAL_VARIABLE_RETRIEVE(WorldViewProjection)
		MATERIAL_VARIABLE_RETRIEVE(InvWorldViewProjection)
		MATERIAL_VARIABLE_RETRIEVE(preWorldViewProjection)
		MATERIAL_VARIABLE_RETRIEVE(preInvWorldViewProjection)
			MATERIAL_VARIABLE_RETRIEVE(WorldInverseTranspose)
			MATERIAL_VARIABLE_RETRIEVE(amount)
	}
}