#pragma once

#include "Common.h"
#include "PostProcessingMaterial.h"

using namespace Library;

namespace Rendering
{
	class ColorGradingMaterial : public PostProcessingMaterial
	{
		RTTI_DECLARATIONS(PostProcessingMaterial, ColorGradingMaterial)

		MATERIAL_VARIABLE_DECLARATION(lut)


	public:
		ColorGradingMaterial();

		virtual void Initialize(Effect* effect) override;
	};
}