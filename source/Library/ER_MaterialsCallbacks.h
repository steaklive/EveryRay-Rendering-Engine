#pragma once
#include "Common.h"

namespace Rendering
{
	class RenderingObject;
}
namespace Library
{
	class Camera;
	class DirectionalLight;
	class ShadowMapper;

	struct ER_MaterialSystems
	{
		const Camera* mCamera;
		const DirectionalLight* mDirectionalLight;
		const ShadowMapper* mShadowMapper;
	};

	class ER_MaterialsCallbacks
	{
	public:
		static void UpdateDeferredPrepassMaterialVariables(ER_MaterialSystems neededSystems, Rendering::RenderingObject* obj, int meshIndex);
		static void UpdateForwardLightingMaterial(ER_MaterialSystems neededSystems, Rendering::RenderingObject* obj, int meshIndex);
		static void UpdateShadowMappingMaterialVariables(ER_MaterialSystems neededSystems, Rendering::RenderingObject* obj, int meshIndex, int cascadeIndex);
		static void UpdateVoxelizationGIMaterialVariables(ER_MaterialSystems neededSystems, Rendering::RenderingObject* obj, int meshIndex, int voxelCascadeIndex);
	};
}
