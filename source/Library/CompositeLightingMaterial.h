#pragma once

#include "Common.h"
#include "PostProcessingMaterial.h"

using namespace Library;

namespace Rendering
{
	class CompositeLightingMaterial : public PostProcessingMaterial
	{
		RTTI_DECLARATIONS(PostProcessingMaterial, CompositeLightingMaterial)

		MATERIAL_VARIABLE_DECLARATION(InputDirectTexture)
		MATERIAL_VARIABLE_DECLARATION(InputIndirectTexture)


	public:
		CompositeLightingMaterial();

		virtual void Initialize(Effect* effect) override;
	};
}