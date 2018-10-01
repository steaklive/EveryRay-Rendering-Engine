#pragma once

#include "Common.h"
#include "PostProcessingMaterial.h"

using namespace Library;

namespace Rendering
{
	class ColorFilterMaterial : public PostProcessingMaterial
	{
		RTTI_DECLARATIONS(PostProcessingMaterial, ColorFilterMaterial)

			MATERIAL_VARIABLE_DECLARATION(ColorFilter)

	public:
		ColorFilterMaterial();

		virtual void Initialize(Effect* effect) override;
	};
}