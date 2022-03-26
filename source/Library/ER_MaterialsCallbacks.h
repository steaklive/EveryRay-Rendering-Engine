#pragma once
#include "Common.h"
#include "ER_IlluminationProbeManager.h"

namespace Rendering
{
	class RenderingObject;
}
namespace Library
{
	class Camera;
	class DirectionalLight;
	class ER_ShadowMapper;

	struct ER_MaterialSystems
	{
		const Camera* mCamera = nullptr;
		const DirectionalLight* mDirectionalLight = nullptr;
		const ER_ShadowMapper* mShadowMapper = nullptr;
		const ER_IlluminationProbeManager* mProbesManager = nullptr;
	};
}
