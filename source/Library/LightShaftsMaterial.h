#pragma once

#include "Common.h"
#include "PostProcessingMaterial.h"

using namespace Library;

namespace Rendering
{
	class LightShaftsMaterial : public PostProcessingMaterial
	{
		RTTI_DECLARATIONS(PostProcessingMaterial, LightShaftsMaterial)
		MATERIAL_VARIABLE_DECLARATION(DepthTexture)
		MATERIAL_VARIABLE_DECLARATION(OcclusionTexture)
		MATERIAL_VARIABLE_DECLARATION(SunPosNDC)
		MATERIAL_VARIABLE_DECLARATION(Density)
		MATERIAL_VARIABLE_DECLARATION(Weight)
		MATERIAL_VARIABLE_DECLARATION(Exposure)
		MATERIAL_VARIABLE_DECLARATION(Decay)
		MATERIAL_VARIABLE_DECLARATION(MaxSunDistanceDelta)
		MATERIAL_VARIABLE_DECLARATION(Intensity)

	public:
		LightShaftsMaterial();

		virtual void Initialize(Effect* effect) override;
	};
}