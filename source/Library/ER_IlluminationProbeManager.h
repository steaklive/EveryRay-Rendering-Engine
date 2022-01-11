#pragma once
#define CUBEMAP_FACES_COUNT 6

#include "Common.h"
#include "RenderingObject.h"
#include "CustomRenderTarget.h"
#include "DepthTarget.h"
#include "ConstantBuffer.h"

namespace Library
{
	class Camera;
	class ShadowMapper;
	class DirectionalLight;
	class Skybox;
	class GameTime;
	class QuadRenderer;

	enum ER_ProbeType
	{
		LIGHT_PROBE,
		REFLECTION_PROBE
	};

	namespace LightProbeCBufferData {
		struct DiffuseProbeConvolutionCB
		{
			XMFLOAT4 WorldPos;
			int FaceIndex;
			XMFLOAT3 pad;
		};
	}

	class ER_LightProbe
	{
		using LightProbeRenderingObjectsInfo = std::map<std::string, Rendering::RenderingObject*>;
	public:
		ER_LightProbe(Game& game, DirectionalLight& light, ShadowMapper& shadowMapper, const XMFLOAT3& position, int size);
		~ER_LightProbe();

		void Compute(Game& game, const GameTime& gameTime, const LightProbeRenderingObjectsInfo& objectsToRender, Skybox* skybox, const std::wstring& levelPath, bool forceRecompute = false);
		void UpdateProbe(const GameTime& gameTime);

		ID3D11ShaderResourceView* GetCubemapSRV() { return mCubemapFacesConvolutedRT->getSRV(); }
	private:
		void DrawProbe(Game& game, const GameTime& gameTime, const std::wstring& levelPath, const LightProbeRenderingObjectsInfo& objectsToRender, Skybox* skybox = nullptr);
		void UpdateStandardLightingPBRMaterialVariables(Rendering::RenderingObject* obj, int meshIndex, int cubeFaceIndex);
		void PrecullObjectsPerFace();
		void SaveProbeOnDisk(Game& game, const std::wstring& levelPath, ER_ProbeType aType);
		bool LoadProbeFromDisk(Game& game, const std::wstring& levelPath, ER_ProbeType aType);
		std::wstring GetConstructedProbeName(const std::wstring& levelPath, ER_ProbeType aType);
		
		DirectionalLight& mDirectionalLight;
		ShadowMapper& mShadowMapper;

		LightProbeRenderingObjectsInfo mObjectsToRenderPerFace[CUBEMAP_FACES_COUNT];
		CustomRenderTarget* mCubemapFacesRT;
		CustomRenderTarget* mCubemapFacesConvolutedRT;
		DepthTarget* mDepthBuffers[CUBEMAP_FACES_COUNT];
		Camera* mCubemapCameras[CUBEMAP_FACES_COUNT];
		QuadRenderer* mQuadRenderer;
		ConstantBuffer<LightProbeCBufferData::DiffuseProbeConvolutionCB> mDiffuseConvolutionCB;

		ID3D11VertexShader* mDiffuseConvolutionVS = nullptr;
		ID3D11PixelShader* mDiffuseConvolutionPS = nullptr;
		ID3D11SamplerState* mLinearSamplerState = nullptr;

		XMFLOAT3 mPosition;
		int mSize;
		bool mIsComputed = false;
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

		void SetLevelPath(const std::wstring& aPath) { mLevelPath = aPath; };
		void ComputeOrLoadProbes(Game& game, const GameTime& gameTime, ProbesRenderingObjectsInfo& aObjects, Skybox* skybox = nullptr);
		ER_LightProbe* GetLightProbe(int index) { return mLightProbes[index]; }
	private:
		void ComputeLightProbes(int aIndex = -1);
		void ComputeReflectionProbes(int aIndex = -1);

		std::vector<ER_LightProbe*> mLightProbes;

		std::wstring mLevelPath;
	};
}