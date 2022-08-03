#pragma once
#include "Common.h"
#include "ER_LightProbesManager.h"

namespace Library
{
	class ER_RenderingObject;
	class ER_Camera;
	class ER_DirectionalLight;
	class ER_ShadowMapper;

	struct ER_MaterialSystems
	{
		const ER_Camera* mCamera = nullptr;
		const ER_DirectionalLight* mDirectionalLight = nullptr;
		const ER_ShadowMapper* mShadowMapper = nullptr;
		const ER_LightProbesManager* mProbesManager = nullptr;
	};
}
