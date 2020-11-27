#pragma once

#include "Common.h"

namespace Library
{
	class MaterialHelper
	{
	public:
		static const std::string lightingMaterialName;
		static const std::string shadowMapMaterialName;
		static const std::string deferredPrepassMaterialName;
		static const std::string ssrMaterialName;
		static const std::string parallaxMaterialName;
	};
}