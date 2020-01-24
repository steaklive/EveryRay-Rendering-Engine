#pragma once

#include "Common.h"
#include "PostProcessingMaterial.h"

using namespace Library;

namespace Rendering
{
	class MotionBlurMaterial : public PostProcessingMaterial
	{
		RTTI_DECLARATIONS(PostProcessingMaterial, MotionBlurMaterial)
			MATERIAL_VARIABLE_DECLARATION(DepthTexture)
			MATERIAL_VARIABLE_DECLARATION(WorldViewProjection)
			MATERIAL_VARIABLE_DECLARATION(InvWorldViewProjection)
			MATERIAL_VARIABLE_DECLARATION(preWorldViewProjection)
			MATERIAL_VARIABLE_DECLARATION(preInvWorldViewProjection)
			MATERIAL_VARIABLE_DECLARATION(WorldInverseTranspose)
			MATERIAL_VARIABLE_DECLARATION(amount)

	public:
		MotionBlurMaterial();

		virtual void Initialize(Effect* effect) override;
	};
}