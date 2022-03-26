#pragma once

#include "Common.h"

namespace Library
{
	class MaterialHelper
	{
	public:
		static const std::string basicColorMaterialName;
		static const std::string shadowMapMaterialName;
		static const std::string gbufferMaterialName;
		static const std::string renderToLightProbeMaterialName;
		static const std::string debugLightProbeMaterialName;
		static const std::string voxelizationMaterialName;

		static const std::string forwardLightingNonMaterialName;
	};
}