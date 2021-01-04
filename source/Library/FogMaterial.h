#pragma once

#include "Common.h"
#include "PostProcessingMaterial.h"

using namespace Library;

namespace Rendering
{
	class FogMaterial : public PostProcessingMaterial
	{
		RTTI_DECLARATIONS(PostProcessingMaterial, FogMaterial)
		MATERIAL_VARIABLE_DECLARATION(DepthTexture)
		MATERIAL_VARIABLE_DECLARATION(FogColor)
		MATERIAL_VARIABLE_DECLARATION(FogNear)
		MATERIAL_VARIABLE_DECLARATION(FogFar)
		MATERIAL_VARIABLE_DECLARATION(FogDensity)


	public:
		FogMaterial();
		virtual void Initialize(Effect* effect) override;
	};
}