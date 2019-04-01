#pragma once

#include "Common.h"
#include "PostProcessingMaterial.h"

using namespace Library;

namespace Rendering
{
	class VignetteMaterial : public PostProcessingMaterial
	{
		RTTI_DECLARATIONS(PostProcessingMaterial, VignetteMaterial)

		MATERIAL_VARIABLE_DECLARATION(radius)
		MATERIAL_VARIABLE_DECLARATION(softness)


	public:
		VignetteMaterial();

		virtual void Initialize(Effect* effect) override;
	};
}