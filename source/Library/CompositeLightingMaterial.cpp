#include "stdafx.h"
#include "CompositeLightingMaterial.h"
#include "GameException.h"

namespace Rendering
{
	RTTI_DEFINITIONS(CompositeLightingMaterial)

	CompositeLightingMaterial::CompositeLightingMaterial()
		: 
		PostProcessingMaterial(),
		MATERIAL_VARIABLE_INITIALIZATION(InputDirectTexture),
		MATERIAL_VARIABLE_INITIALIZATION(InputIndirectTexture),
		MATERIAL_VARIABLE_INITIALIZATION(DebugVoxel),
		MATERIAL_VARIABLE_INITIALIZATION(DebugAO)
	{
	}

	MATERIAL_VARIABLE_DEFINITION(CompositeLightingMaterial, InputDirectTexture)
	MATERIAL_VARIABLE_DEFINITION(CompositeLightingMaterial, InputIndirectTexture)
	MATERIAL_VARIABLE_DEFINITION(CompositeLightingMaterial, DebugVoxel)
	MATERIAL_VARIABLE_DEFINITION(CompositeLightingMaterial, DebugAO)

	void CompositeLightingMaterial::Initialize(Effect* effect)
	{
		PostProcessingMaterial::Initialize(effect);

		MATERIAL_VARIABLE_RETRIEVE(InputDirectTexture)
		MATERIAL_VARIABLE_RETRIEVE(InputIndirectTexture)
		MATERIAL_VARIABLE_RETRIEVE(DebugVoxel)
		MATERIAL_VARIABLE_RETRIEVE(DebugAO)
	}
}