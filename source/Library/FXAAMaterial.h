#pragma once

#include "Common.h"
#include "PostProcessingMaterial.h"

using namespace Library;

namespace Rendering
{
	class FXAAMaterial : public PostProcessingMaterial
	{
		RTTI_DECLARATIONS(PostProcessingMaterial, FXAAMaterial)

		MATERIAL_VARIABLE_DECLARATION(Width)
		MATERIAL_VARIABLE_DECLARATION(Height)

	public:
		FXAAMaterial();

		virtual void Initialize(Effect* effect) override;
	};
}