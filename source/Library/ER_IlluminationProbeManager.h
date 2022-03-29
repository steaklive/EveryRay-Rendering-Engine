#pragma once
#define CUBEMAP_FACES_COUNT 6

#define DIFFUSE_PROBE_SIZE 32 //cubemap dimension
#define DISTANCE_BETWEEN_DIFFUSE_PROBES 15

#define SPECULAR_PROBE_MIP_COUNT 6
#define SPECULAR_PROBE_SIZE 128 //cubemap dimension
#define DISTANCE_BETWEEN_SPECULAR_PROBES 30

#define MAX_PROBES_IN_VOLUME_PER_AXIS 6 // == cbrt(2048 / CUBEMAP_FACES_COUNT), 2048 - tex. array limit (DX11)
#define PROBE_COUNT_PER_CELL 8 // 3D cube cell of probes in each vertex
#define NUM_PROBE_VOLUME_CASCADES 2 // 3D volumes of cells

// we want to skip some probes to widen the coverage area
static const int VolumeProbeIndexSkips[NUM_PROBE_VOLUME_CASCADES] = { 1, 5 }; //skips between probes (in indices), i.e 1 - no skips
static const int DiffuseProbesVolumeCascadeSizes[NUM_PROBE_VOLUME_CASCADES] =
{ 
	MAX_PROBES_IN_VOLUME_PER_AXIS * DISTANCE_BETWEEN_DIFFUSE_PROBES * VolumeProbeIndexSkips[0] / 2,
	MAX_PROBES_IN_VOLUME_PER_AXIS * DISTANCE_BETWEEN_DIFFUSE_PROBES * VolumeProbeIndexSkips[1] / 2
};
static const int SpecularProbesVolumeCascadeSizes[NUM_PROBE_VOLUME_CASCADES] = 
{ 
	MAX_PROBES_IN_VOLUME_PER_AXIS * DISTANCE_BETWEEN_SPECULAR_PROBES * VolumeProbeIndexSkips[0] / 2,
	MAX_PROBES_IN_VOLUME_PER_AXIS * DISTANCE_BETWEEN_SPECULAR_PROBES * VolumeProbeIndexSkips[1] / 2
};

#include "Common.h"
#include "ER_RenderingObject.h"
#include "ER_GPUTexture.h"
#include "DepthTarget.h"
#include "ConstantBuffer.h"

namespace Library
{
	class Camera;
	class ER_ShadowMapper;
	class DirectionalLight;
	class Skybox;
	class GameTime;
	class QuadRenderer;
	class Scene;
	class RenderableAABB;
	class ER_GPUBuffer;
	class ER_LightProbe;

	enum ER_ProbeType
	{
		DIFFUSE_PROBE = 0,
		SPECULAR_PROBE = 1,
		PROBE_TYPES_COUNT = 2
	};

	struct ER_LightProbeCell
	{
		XMFLOAT3 position;
		std::vector<int> lightProbeIndices;
		int index;
	};

	class ER_IlluminationProbeManager
	{
	public:
		using ProbesRenderingObjectsInfo = std::map<std::string, ER_RenderingObject*>;
		ER_IlluminationProbeManager(Game& game, Camera& camera, Scene* scene, DirectionalLight& light, ER_ShadowMapper& shadowMapper);
		~ER_IlluminationProbeManager();

		bool AreProbesReady() { return mDiffuseProbesReady && mSpecularProbesReady; }
		void SetLevelPath(const std::wstring& aPath) { mLevelPath = aPath; };
		void ComputeOrLoadProbes(Game& game, const GameTime& gameTime, ProbesRenderingObjectsInfo& aObjects, Skybox* skybox = nullptr);
		void DrawDebugProbes(ER_ProbeType aType, int volumeIndex);
		void DrawDebugProbesVolumeGizmo(ER_ProbeType aType, int volumeIndex);
		void UpdateProbes(Game& game);
		int GetCellIndex(const XMFLOAT3& pos, ER_ProbeType aType, int volumeIndex);

		ER_LightProbe* GetGlobalDiffuseProbe() { return mGlobalDiffuseProbe; }
		const ER_LightProbe* GetDiffuseLightProbe(int index) const { return mDiffuseProbes[index]; }
		ER_GPUTexture* GetCulledDiffuseProbesTextureArray(int volumeIndex) const { return mDiffuseCubemapArrayRT[volumeIndex]; }
		ER_GPUBuffer* GetDiffuseProbesCellsIndicesBuffer(int volumeIndex) const { return mDiffuseProbesCellsIndicesGPUBuffer[volumeIndex]; }
		ER_GPUBuffer* GetDiffuseProbesTexArrayIndicesBuffer(int volumeIndex) const { return mDiffuseProbesTexArrayIndicesGPUBuffer[volumeIndex]; }
		ER_GPUBuffer* GetDiffuseProbesPositionsBuffer() const { return mDiffuseProbesPositionsGPUBuffer; }

		const ER_LightProbe* GetSpecularLightProbe(int index) const { return mSpecularProbes[index]; }
		ER_GPUTexture* GetCulledSpecularProbesTextureArray(int volumeIndex) const { return mSpecularCubemapArrayRT[volumeIndex]; }
		ER_GPUBuffer* GetSpecularProbesCellsIndicesBuffer(int volumeIndex) const { return mSpecularProbesCellsIndicesGPUBuffer[volumeIndex]; }
		ER_GPUBuffer* GetSpecularProbesTexArrayIndicesBuffer(int volumeIndex) const { return mSpecularProbesTexArrayIndicesGPUBuffer[volumeIndex]; }
		ER_GPUBuffer* GetSpecularProbesPositionsBuffer() const { return mSpecularProbesPositionsGPUBuffer; }

		const XMFLOAT3& GetProbesVolumeCascade(ER_ProbeType aType, int volumeIndex) { 
			if (aType == DIFFUSE_PROBE)
				return XMFLOAT3(DiffuseProbesVolumeCascadeSizes[volumeIndex],
					DiffuseProbesVolumeCascadeSizes[volumeIndex],
					DiffuseProbesVolumeCascadeSizes[volumeIndex]); 
			else
				return XMFLOAT3(SpecularProbesVolumeCascadeSizes[volumeIndex],
					SpecularProbesVolumeCascadeSizes[volumeIndex],
					SpecularProbesVolumeCascadeSizes[volumeIndex]);
		}
		const XMFLOAT4& GetProbesCellsCount(ER_ProbeType aType, int volumeIndex);
		float GetProbesIndexSkip(int volumeIndex);

		ID3D11ShaderResourceView* GetIntegrationMap() { return mIntegrationMapTextureSRV; }
		const XMFLOAT3& GetSceneProbesVolumeMin() { return mSceneProbesMinBounds; }
		const XMFLOAT3& GetSceneProbesVolumeMax() { return mSceneProbesMaxBounds; }
		bool IsEnabled() { return mEnabled; }
		bool mDebugDiscardCulledProbes = false;//used in DebugLightProbeMaterial
	private:
		void SetupDiffuseProbes(Game& game, Camera& camera, Scene* scene, DirectionalLight& light, ER_ShadowMapper& shadowMapper);
		void SetupSpecularProbes(Game& game, Camera& camera, Scene* scene, DirectionalLight& light, ER_ShadowMapper& shadowMapper);
		void AddProbeToCells(ER_LightProbe* aProbe, ER_ProbeType aType, const XMFLOAT3& minBounds, const XMFLOAT3& maxBounds);
		bool IsProbeInCell(ER_LightProbe* aProbe, ER_LightProbeCell& aCell, ER_AABB& aCellBounds);
		void UpdateProbesByType(Game& game, ER_ProbeType aType);
		
		QuadRenderer* mQuadRenderer = nullptr;
		Camera& mMainCamera;
		RenderableAABB* mDebugDiffuseProbeVolumeGizmo[NUM_PROBE_VOLUME_CASCADES] = { nullptr };
		RenderableAABB* mDebugSpecularProbeVolumeGizmo[NUM_PROBE_VOLUME_CASCADES] = { nullptr };

		ID3D11PixelShader* mConvolutionPS = nullptr;
		ID3D11SamplerState* mLinearSamplerState = nullptr;

		ID3D11ShaderResourceView* mIntegrationMapTextureSRV = nullptr; //TODO generate custom ones

		XMFLOAT3 mSceneProbesMinBounds;
		XMFLOAT3 mSceneProbesMaxBounds;

		int mMaxProbesInVolumeCount = 0;

		// Diffuse probes members
		XMFLOAT3 mCurrentDiffuseVolumesMaxBounds[NUM_PROBE_VOLUME_CASCADES];
		XMFLOAT3 mCurrentDiffuseVolumesMinBounds[NUM_PROBE_VOLUME_CASCADES];
		std::vector<ER_LightProbe*> mDiffuseProbes;
		ER_RenderingObject* mDiffuseProbeRenderingObject[NUM_PROBE_VOLUME_CASCADES] = { nullptr, nullptr };
		int* mDiffuseProbesTexArrayIndicesCPUBuffer[NUM_PROBE_VOLUME_CASCADES] = { nullptr };
		ER_GPUBuffer* mDiffuseProbesTexArrayIndicesGPUBuffer[NUM_PROBE_VOLUME_CASCADES] = { nullptr };
		ER_GPUBuffer* mDiffuseProbesCellsIndicesGPUBuffer[NUM_PROBE_VOLUME_CASCADES] = { nullptr };
		ER_GPUBuffer* mDiffuseProbesPositionsGPUBuffer = nullptr;
		ER_GPUTexture* mTempDiffuseCubemapFacesRT = nullptr;
		ER_GPUTexture* mTempDiffuseCubemapFacesConvolutedRT = nullptr;
		ER_GPUTexture* mDiffuseCubemapArrayRT[NUM_PROBE_VOLUME_CASCADES] = { nullptr };
		DepthTarget* mTempDiffuseCubemapDepthBuffers[CUBEMAP_FACES_COUNT] = { nullptr };
		std::vector<ER_LightProbeCell> mDiffuseProbesCells[NUM_PROBE_VOLUME_CASCADES];
		ER_AABB mDiffuseProbesCellBounds[NUM_PROBE_VOLUME_CASCADES];
		std::vector<int> mNonCulledDiffuseProbesIndices[NUM_PROBE_VOLUME_CASCADES];
		int mDiffuseProbesCountTotal = 0;
		int mDiffuseProbesCountX = 0;
		int mDiffuseProbesCountY = 0;
		int mDiffuseProbesCountZ = 0;
		int mDiffuseProbesCellsCountX[NUM_PROBE_VOLUME_CASCADES] = { 0 };
		int mDiffuseProbesCellsCountY[NUM_PROBE_VOLUME_CASCADES] = { 0 };
		int mDiffuseProbesCellsCountZ[NUM_PROBE_VOLUME_CASCADES] = { 0 };
		int mDiffuseProbesCellsCountTotal[NUM_PROBE_VOLUME_CASCADES] = { 0 };
		int mNonCulledDiffuseProbesCount[NUM_PROBE_VOLUME_CASCADES] = { 0 };
		bool mDiffuseProbesReady = false;
		ER_LightProbe* mGlobalDiffuseProbe = nullptr;
		bool mGlobalDiffuseProbeReady = false;

		// Specular probes members
		XMFLOAT3 mCurrentSpecularVolumesMaxBounds[NUM_PROBE_VOLUME_CASCADES];
		XMFLOAT3 mCurrentSpecularVolumesMinBounds[NUM_PROBE_VOLUME_CASCADES];
		std::vector<ER_LightProbe*> mSpecularProbes;
		ER_RenderingObject* mSpecularProbeRenderingObject[NUM_PROBE_VOLUME_CASCADES] = { nullptr, nullptr };
		int* mSpecularProbesTexArrayIndicesCPUBuffer[NUM_PROBE_VOLUME_CASCADES] = { nullptr };
		ER_GPUBuffer* mSpecularProbesTexArrayIndicesGPUBuffer[NUM_PROBE_VOLUME_CASCADES] = { nullptr };
		ER_GPUBuffer* mSpecularProbesCellsIndicesGPUBuffer[NUM_PROBE_VOLUME_CASCADES] = { nullptr };
		ER_GPUBuffer* mSpecularProbesPositionsGPUBuffer = nullptr;
		ER_GPUTexture* mTempSpecularCubemapFacesRT = nullptr;
		ER_GPUTexture* mTempSpecularCubemapFacesConvolutedRT = nullptr;
		DepthTarget* mTempSpecularCubemapDepthBuffers[CUBEMAP_FACES_COUNT] = { nullptr };
		ER_GPUTexture* mSpecularCubemapArrayRT[NUM_PROBE_VOLUME_CASCADES] = { nullptr };
		std::vector<ER_LightProbeCell> mSpecularProbesCells[NUM_PROBE_VOLUME_CASCADES];
		ER_AABB mSpecularProbesCellBounds[NUM_PROBE_VOLUME_CASCADES];
		std::vector<int> mNonCulledSpecularProbesIndices[NUM_PROBE_VOLUME_CASCADES];
		int mSpecularProbesCountTotal = 0;
		int mSpecularProbesCountX = 0;
		int mSpecularProbesCountY = 0;
		int mSpecularProbesCountZ = 0;
		int mSpecularProbesCellsCountX[NUM_PROBE_VOLUME_CASCADES]= { 0 };
		int mSpecularProbesCellsCountY[NUM_PROBE_VOLUME_CASCADES]= { 0 };
		int mSpecularProbesCellsCountZ[NUM_PROBE_VOLUME_CASCADES]= { 0 };
		int mSpecularProbesCellsCountTotal[NUM_PROBE_VOLUME_CASCADES] = { 0 };
		int mNonCulledSpecularProbesCount[NUM_PROBE_VOLUME_CASCADES] = { 0 };
		bool mSpecularProbesReady = false;

		std::wstring mLevelPath;
		bool mEnabled = true;
	};
}