#pragma once
#define CUBEMAP_FACES_COUNT 6

#define DIFFUSE_PROBE_SIZE 32 //cubemap dimension

#define SPECULAR_PROBE_MIP_COUNT 6
#define SPECULAR_PROBE_SIZE 128 //cubemap dimension

#define MAX_CUBEMAPS_IN_VOLUME_PER_AXIS 6 // == cbrt(2048 / CUBEMAP_FACES_COUNT), 2048 - tex. array limit (DX11)
#define PROBE_COUNT_PER_CELL_3D 8
#define PROBE_COUNT_PER_CELL_2D 4

#define SPHERICAL_HARMONICS_ORDER 2
#define SPHERICAL_HARMONICS_COEF_COUNT (SPHERICAL_HARMONICS_ORDER + 1) * (SPHERICAL_HARMONICS_ORDER + 1)

#include "Common.h"
#include "ER_RenderingObject.h"
#include "ER_LightProbe.h"
#include "RHI/ER_RHI.h"

namespace EveryRay_Core
{
	class ER_Camera;
	class ER_ShadowMapper;
	class ER_DirectionalLight;
	class ER_Skybox;
	class ER_CoreTime;
	class ER_QuadRenderer;
	class ER_Scene;
	class ER_RenderableAABB;

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

	class ER_LightProbesManager
	{
	public:
		using ProbesRenderingObjectsInfo = std::vector<std::pair<std::string, ER_RenderingObject*>>;
		ER_LightProbesManager(ER_Core& game, ER_Camera& camera, ER_Scene* scene, ER_DirectionalLight* light, ER_ShadowMapper* shadowMapper);
		~ER_LightProbesManager();

		bool AreProbesReady() { return mDiffuseProbesReady && mSpecularProbesReady; }
		void SetLevelPath(const std::wstring& aPath) { mLevelPath = aPath; };
		void ComputeOrLoadLocalProbes(ER_Core& game, ProbesRenderingObjectsInfo& aObjects);
		void ComputeOrLoadGlobalProbes(ER_Core& game, ProbesRenderingObjectsInfo& aObjects);
		void DrawDebugProbes(ER_RHI* rhi, ER_RHI_GPUTexture* aRenderTarget, ER_RHI_GPUTexture* aDepth, ER_ProbeType aType, ER_RHI_GPURootSignature* rs);
		void UpdateProbes(ER_Core& game);
		int GetCellIndex(const XMFLOAT3& pos, ER_ProbeType aType);

		ER_LightProbe* GetGlobalDiffuseProbe() const { return mGlobalDiffuseProbe; }
		const ER_LightProbe& GetDiffuseLightProbe(int index) const { return mDiffuseProbes[index]; }
		ER_RHI_GPUBuffer* GetDiffuseProbesCellsIndicesBuffer() const { return mDiffuseProbesCellsIndicesGPUBuffer; }
		ER_RHI_GPUBuffer* GetDiffuseProbesPositionsBuffer() const { return mDiffuseProbesPositionsGPUBuffer; }
		ER_RHI_GPUBuffer* GetDiffuseProbesSphericalHarmonicsCoefficientsBuffer() const { return mDiffuseProbesSphericalHarmonicsGPUBuffer; }
		float GetDistanceBetweenDiffuseProbes() { return mDistanceBetweenDiffuseProbes; }

		ER_LightProbe* GetGlobalSpecularProbe() const { return mGlobalSpecularProbe; }
		const ER_LightProbe& GetSpecularLightProbe(int index) const { return mSpecularProbes[index]; }
		ER_RHI_GPUTexture* GetCulledSpecularProbesTextureArray() const { return mSpecularCubemapArrayRT; }
		ER_RHI_GPUBuffer* GetSpecularProbesCellsIndicesBuffer() const { return mSpecularProbesCellsIndicesGPUBuffer; }
		ER_RHI_GPUBuffer* GetSpecularProbesTexArrayIndicesBuffer() const { return mSpecularProbesTexArrayIndicesGPUBuffer; }
		ER_RHI_GPUBuffer* GetSpecularProbesPositionsBuffer() const { return mSpecularProbesPositionsGPUBuffer; }
		float GetDistanceBetweenSpecularProbes() { return mDistanceBetweenSpecularProbes; }

		ER_RHI_GPUTexture* GetIntegrationMap() { return mIntegrationMapTextureSRV; }
		
		XMFLOAT4 GetProbesCellsCount(ER_ProbeType aType);
		const XMFLOAT3& GetSceneProbesVolumeMin() { return mSceneProbesMinBounds; }
		const XMFLOAT3& GetSceneProbesVolumeMax() { return mSceneProbesMaxBounds; }

		bool Is2DCellGrid() { return mIsPlacedOnTerrain; } // by default we use 3D cell grid, but terrain scenes is a special case for now and use 2D

		bool IsEnabled() { return mEnabled; }
		bool AreGlobalProbesReady() { return mGlobalDiffuseProbeReady && mGlobalSpecularProbeReady; }

		bool mDebugDiscardCulledProbes = false;//used in DebugLightProbeMaterial
	private:
		void SetupGlobalDiffuseProbe(ER_Core& game, ER_Camera& camera, ER_Scene* scene, ER_DirectionalLight* light, ER_ShadowMapper* shadowMapper);
		void SetupGlobalSpecularProbe(ER_Core& game, ER_Camera& camera, ER_Scene* scene, ER_DirectionalLight* light, ER_ShadowMapper* shadowMapper);
		void SetupDiffuseProbes(ER_Core& game, ER_Camera& camera, ER_Scene* scene, ER_DirectionalLight* light, ER_ShadowMapper* shadowMapper);
		void SetupSpecularProbes(ER_Core& game, ER_Camera& camera, ER_Scene* scene, ER_DirectionalLight* light, ER_ShadowMapper* shadowMapper);
		void AddProbeToCells(ER_LightProbe& aProbe, ER_ProbeType aType, const XMFLOAT3& minBounds, const XMFLOAT3& maxBounds);
		bool IsProbeInCell(ER_LightProbe& aProbe, ER_LightProbeCell& aCell, ER_AABB& aCellBounds);
		void UpdateProbesByType(ER_Core& game, ER_ProbeType aType);
		void PlaceProbesOnTerrain(ER_Core& game, ER_RHI_GPUBuffer* outputBuffer, ER_RHI_GPUBuffer* inputBuffer, XMFLOAT4* positions, int positionsCount, float customDampDelta = FLT_MAX);

		ER_QuadRenderer* mQuadRenderer = nullptr;
		ER_Camera& mMainCamera;

		ER_RHI_GPUShader* mConvolutionPS = nullptr;

		ER_RHI_GPUTexture* mIntegrationMapTextureSRV = nullptr; //TODO generate custom ones

		XMFLOAT3 mSceneProbesMinBounds;
		XMFLOAT3 mSceneProbesMaxBounds;

		// Diffuse probes members
		std::vector<ER_LightProbe> mDiffuseProbes;
		ER_RenderingObject* mDiffuseProbeRenderingObject = nullptr;
		std::string mDiffuseDebugLightProbePassPSOName = "ER_RHI_GPUPipelineStateObject: Light Probes Manager - Diffuse Debug Probe Pass";
		ER_RHI_GPUBuffer* mDiffuseProbesCellsIndicesGPUBuffer = nullptr;
		ER_RHI_GPUBuffer* mDiffuseProbesPositionsGPUBuffer = nullptr;
		ER_RHI_GPUBuffer* mDiffuseInputPositionsOnTerrainBuffer = nullptr;
		ER_RHI_GPUBuffer* mDiffuseOutputPositionsOnTerrainBuffer = nullptr;
		ER_RHI_GPUBuffer* mDiffuseProbesSphericalHarmonicsGPUBuffer = nullptr;
		ER_RHI_GPUTexture* mTempDiffuseCubemapFacesRT = nullptr;
		ER_RHI_GPUTexture* mTempDiffuseCubemapFacesConvolutedRT = nullptr;
		ER_RHI_GPUTexture* mTempDiffuseCubemapDepthBuffers[CUBEMAP_FACES_COUNT] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
		std::vector<ER_LightProbeCell> mDiffuseProbesCells;
		ER_AABB mDiffuseProbesCellBounds;
		int mDiffuseProbesCountTotal = 0;
		int mDiffuseProbesCountX = 0;
		int mDiffuseProbesCountY = 0;
		int mDiffuseProbesCountZ = 0;
		int mDiffuseProbesCellsCountX = 0;
		int mDiffuseProbesCellsCountY = 0;
		int mDiffuseProbesCellsCountZ = 0;
		int mDiffuseProbesCellsCountTotal = 0;
		bool mDiffuseProbesReady = false;
		ER_LightProbe* mGlobalDiffuseProbe = nullptr;
		bool mGlobalDiffuseProbeReady = false;
		float mDistanceBetweenDiffuseProbes = 0.0f;

		// Specular probes members
		std::vector<ER_LightProbe> mSpecularProbes;
		ER_RenderingObject* mSpecularProbeRenderingObject = nullptr;
		std::string mSpecularDebugLightProbePassPSOName = "ER_RHI_GPUPipelineStateObject: Light Probes Manager - Specular Debug Probe Pass";
		int* mSpecularProbesTexArrayIndicesCPUBuffer = nullptr;
		ER_RHI_GPUBuffer* mSpecularProbesTexArrayIndicesGPUBuffer = nullptr;
		ER_RHI_GPUBuffer* mSpecularProbesCellsIndicesGPUBuffer = nullptr;
		ER_RHI_GPUBuffer* mSpecularProbesPositionsGPUBuffer = nullptr;
		ER_RHI_GPUBuffer* mSpecularInputPositionsOnTerrainBuffer = nullptr;
		ER_RHI_GPUBuffer* mSpecularOutputPositionsOnTerrainBuffer = nullptr;
		ER_RHI_GPUTexture* mTempSpecularCubemapFacesRT = nullptr;
		ER_RHI_GPUTexture* mTempSpecularCubemapFacesConvolutedRT = nullptr;
		ER_RHI_GPUTexture* mTempSpecularCubemapDepthBuffers[CUBEMAP_FACES_COUNT] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
		ER_RHI_GPUTexture* mSpecularCubemapArrayRT = nullptr;
		std::vector<ER_LightProbeCell> mSpecularProbesCells;
		ER_AABB mSpecularProbesCellBounds;
		std::vector<int> mNonCulledSpecularProbesIndices;
		int mSpecularProbesCountTotal = 0;
		int mSpecularProbesCountX = 0;
		int mSpecularProbesCountY = 0;
		int mSpecularProbesCountZ = 0;
		int mSpecularProbesCellsCountX = 0;
		int mSpecularProbesCellsCountY = 0;
		int mSpecularProbesCellsCountZ = 0;
		int mSpecularProbesCellsCountTotal = 0;
		int mNonCulledSpecularProbesCount = 0;
		bool mSpecularProbesReady = false;
		ER_LightProbe* mGlobalSpecularProbe = nullptr;
		bool mGlobalSpecularProbeReady = false;
		float mDistanceBetweenSpecularProbes = 0.0f;
		float mSpecularProbesVolumeSize = 0.0f;
		int mMaxSpecularProbesInVolumeCount = 0;

		bool mIsPlacedOnTerrain = false;

		// height offset of the bottom layer of probes (negative values - offset above terrain)
		float mTerrainPlacementHeightDeltaDiffuseProbes = 0.0f;
		float mTerrainPlacementHeightDeltaSpecularProbes = 0.0f;

		// by default 3D, but terrain can change that to 2D
		int mCurrentProbeCountPerCell = PROBE_COUNT_PER_CELL_3D;
		bool mIs2DCellsGrid = false;

		std::wstring mLevelPath;
		bool mEnabled = true;
	};
}