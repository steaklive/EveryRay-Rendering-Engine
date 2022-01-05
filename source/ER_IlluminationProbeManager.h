#pragma once
#include "Common.h"
#include "RenderingObject.h"
#include "CustomRenderTarget.h"

#define CUBEMAP_FACES_COUNT 6

namespace Library
{
	class Camera;
	class ShadowMapper;
	class DirectionalLight;
	class Skybox;
	class GameTime;

	class ER_LightProbe
	{
		using LightProbeRenderingObjectsInfo = std::map<std::string, Rendering::RenderingObject*>;
	public:
		ER_LightProbe(Game& game, DirectionalLight& light, ShadowMapper& shadowMapper, const XMFLOAT3& position, int size);
		~ER_LightProbe();

		void DrawProbe(Game& game, const GameTime& gameTime, const LightProbeRenderingObjectsInfo& objectsToRender, Skybox* skybox = nullptr);
		void UpdateProbe(const GameTime& gameTime);

		ID3D11ShaderResourceView* GetCubemapSRV() { return mCubemapFacesRT->getSRV(); }
	private:
		void UpdateStandardLightingPBRMaterialVariables(Rendering::RenderingObject* obj, int cubeFaceIndex);
		void PrecullObjectsPerFace();
		
		DirectionalLight& mDirectionalLight;
		ShadowMapper& mShadowMapper;

		LightProbeRenderingObjectsInfo mObjectsToRenderPerFace[CUBEMAP_FACES_COUNT];
		CustomRenderTarget* mCubemapFacesRT;
		Camera* mCubemapCameras[CUBEMAP_FACES_COUNT];

		XMFLOAT3 mPosition;
		int mSize;
	};

	class ER_ReflectionProbe
	{

	};

	class ER_IlluminationProbeManager
	{

	public:
		using ProbesRenderingObjectsInfo = std::map<std::string, Rendering::RenderingObject*>;
		ER_IlluminationProbeManager(Game& game, DirectionalLight& light, ShadowMapper& shadowMapper);
		~ER_IlluminationProbeManager();

		void Initialize();
		void ComputeProbes(Game& game, const GameTime& gameTime, ProbesRenderingObjectsInfo& aObjects, Skybox* skybox = nullptr);
		ER_LightProbe* GetLightProbe(int index) { return mLightProbes[i]; }
	private:
		void ComputeLightProbes(int aIndex = -1);
		void ComputeReflectionProbes(int aIndex = -1);

		std::vector<ER_LightProbe*> mLightProbes;
	};
}