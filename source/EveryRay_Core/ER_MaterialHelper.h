#pragma once

#include "Common.h"

namespace EveryRay_Core
{
	class ER_MaterialHelper
	{
	public:
		static const std::string basicColorMaterialName;
		static const std::string shadowMapMaterialName;
		static const std::string gbufferMaterialName;
		static const std::string renderToLightProbeMaterialName;
		static const std::string debugLightProbeMaterialName;
		static const std::string voxelizationMaterialName;
		static const std::string snowMaterialName;
		static const std::string fresnelOutlineMaterialName;

		static const std::string forwardLightingNonMaterialName;
	};
}