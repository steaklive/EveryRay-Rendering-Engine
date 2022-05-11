#pragma once
#define CUBEMAP_FACES_COUNT 6

#define DIFFUSE_PROBE_SIZE 32 //cubemap dimension
#define DISTANCE_BETWEEN_DIFFUSE_PROBES 15

#define SPECULAR_PROBE_MIP_COUNT 6
#define SPECULAR_PROBE_SIZE 128 //cubemap dimension
#define DISTANCE_BETWEEN_SPECULAR_PROBES 30

#define MAX_PROBES_IN_VOLUME_PER_AXIS 6 // == cbrt(2048 / CUBEMAP_FACES_COUNT), 2048 - tex. array limit (DX11)
#define PROBE_COUNT_PER_CELL 8 // 3D cube cell of probes in each vertex

#define SPHERICAL_HARMONICS_ORDER 2
#define SPHERICAL_HARMONICS_COEF_COUNT (SPHERICAL_HARMONICS_ORDER + 1) * (SPHERICAL_HARMONICS_ORDER + 1)

#define DIFFUSE_PROBES_VOLUME_SIZE MAX_PROBES_IN_VOLUME_PER_AXIS * DISTANCE_BETWEEN_DIFFUSE_PROBES / 2
#define SPECULAR_PROBES_VOLUME_SIZE MAX_PROBES_IN_VOLUME_PER_AXIS * DISTANCE_BETWEEN_SPECULAR_PROBES / 2

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
	class ER_Skybox;
	class GameTime;
	class ER_QuadRenderer;
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

	class ER_LightProbesManager
	{
	public:
		using ProbesRenderingObjectsInfo = std::map<std::string, ER_RenderingObject*>;
		ER_LightProbesManager(Game& game, Camera& camera, Scene* scene, DirectionalLight& light, ER_ShadowMapper& shadowMapper);
		~ER_LightProbesManager();

		bool AreProbesReady() { return mDiffuseProbesReady && mSpecularProbesReady; }
		void SetLevelPath(const std::wstring& aPath) { mLevelPath = aPath; };
		void ComputeOrLoadLocalProbes(Game& game, ProbesRenderingObjectsInfo& aObjects, ER_Skybox* skybox = nullptr);
		void ComputeOrLoadGlobalProbes(Game& game, ProbesRenderingObjectsInfo& aObjects, ER_Skybox* skybox);
		void DrawDebugProbes(ER_ProbeType aType);
		void DrawDebugProbesVolumeGizmo(ER_ProbeType aType);
		void UpdateProbes(Game& game);
		int GetCellIndex(const XMFLOAT3& pos, ER_ProbeType aType);

		ER_LightProbe* GetGlobalDiffuseProbe() { return mGlobalDiffuseProbe; }
		const ER_LightProbe* GetDiffuseLightProbe(int index) const { return mDiffuseProbes[index]; }
		ER_GPUBuffer* GetDiffuseProbesCellsIndicesBuffer() const { return mDiffuseProbesCellsIndicesGPUBuffer; }
		ER_GPUBuffer* GetDiffuseProbesPositionsBuffer() const { return mDiffuseProbesPositionsGPUBuffer; }
		ER_GPUBuffer* GetDiffuseProbesSphericalHarmonicsCoefficientsBuffer() const { return mDiffuseProbesSphericalHarmonicsGPUBuffer; }

		ER_LightProbe* GetGlobalSpecularProbe() { return mGlobalSpecularProbe; }
		const ER_LightProbe* GetSpecularLightProbe(int index) const { return mSpecularProbes[index]; }
		ER_GPUTexture* GetCulledSpecularProbesTextureArray() const { return mSpecularCubemapArrayRT; }
		ER_GPUBuffer* GetSpecularProbesCellsIndicesBuffer() const { return mSpecularProbesCellsIndicesGPUBuffer; }
		ER_GPUBuffer* GetSpecularProbesTexArrayIndicesBuffer() const { return mSpecularProbesTexArrayIndicesGPUBuffer; }
		ER_GPUBuffer* GetSpecularProbesPositionsBuffer() const { return mSpecularProbesPositionsGPUBuffer; }

		const XMFLOAT3& GetProbesVolumeCascade(ER_ProbeType aType) { 
			if (aType == DIFFUSE_PROBE)
				return XMFLOAT3(DIFFUSE_PROBES_VOLUME_SIZE,	DIFFUSE_PROBES_VOLUME_SIZE,	DIFFUSE_PROBES_VOLUME_SIZE);
			else
				return XMFLOAT3(SPECULAR_PROBES_VOLUME_SIZE, SPECULAR_PROBES_VOLUME_SIZE, SPECULAR_PROBES_VOLUME_SIZE);
		}
		const XMFLOAT4& GetProbesCellsCount(ER_ProbeType aType);

		ID3D11ShaderResourceView* GetIntegrationMap() { return mIntegrationMapTextureSRV; }

		const XMFLOAT3& GetSceneProbesVolumeMin() { return mSceneProbesMinBounds; }
		const XMFLOAT3& GetSceneProbesVolumeMax() { return mSceneProbesMaxBounds; }

		bool IsEnabled() { return mEnabled; }
		bool AreGlobalProbesReady() { return mGlobalDiffuseProbeReady && mGlobalSpecularProbeReady; }

		bool mDebugDiscardCulledProbes = false;//used in DebugLightProbeMaterial
	private:
		void SetupGlobalDiffuseProbe(Game& game, Camera& camera, Scene* scene, DirectionalLight& light, ER_ShadowMapper& shadowMapper);
		void SetupGlobalSpecularProbe(Game& game, Camera& camera, Scene* scene, DirectionalLight& light, ER_ShadowMapper& shadowMapper);
		void SetupDiffuseProbes(Game& game, Camera& camera, Scene* scene, DirectionalLight& light, ER_ShadowMapper& shadowMapper);
		void SetupSpecularProbes(Game& game, Camera& camera, Scene* scene, DirectionalLight& light, ER_ShadowMapper& shadowMapper);
		void AddProbeToCells(ER_LightProbe* aProbe, ER_ProbeType aType, const XMFLOAT3& minBounds, const XMFLOAT3& maxBounds);
		bool IsProbeInCell(ER_LightProbe* aProbe, ER_LightProbeCell& aCell, ER_AABB& aCellBounds);
		void UpdateProbesByType(Game& game, ER_ProbeType aType);
		
		ER_QuadRenderer* mQuadRenderer = nullptr;
		Camera& mMainCamera;
		RenderableAABB* mDebugDiffuseProbeVolumeGizmo = nullptr;
		RenderableAABB* mDebugSpecularProbeVolumeGizmo = nullptr;

		ID3D11PixelShader* mConvolutionPS = nullptr;

		ID3D11ShaderResourceView* mIntegrationMapTextureSRV = nullptr; //TODO generate custom ones

		XMFLOAT3 mSceneProbesMinBounds;
		XMFLOAT3 mSceneProbesMaxBounds;

		int mMaxProbesInVolumeCount = 0;

		// Diffuse probes members
		std::vector<ER_LightProbe*> mDiffuseProbes;
		ER_RenderingObject* mDiffuseProbeRenderingObject = nullptr;
		
		ER_GPUBuffer* mDiffuseProbesCellsIndicesGPUBuffer = nullptr;
		ER_GPUBuffer* mDiffuseProbesPositionsGPUBuffer = nullptr;
		ER_GPUBuffer* mDiffuseProbesSphericalHarmonicsGPUBuffer = nullptr;

		ER_GPUTexture* mTempDiffuseCubemapFacesRT = nullptr;
		ER_GPUTexture* mTempDiffuseCubemapFacesConvolutedRT = nullptr;
		DepthTarget* mTempDiffuseCubemapDepthBuffers[CUBEMAP_FACES_COUNT] = { nullptr };
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

		// Specular probes members
		std::vector<ER_LightProbe*> mSpecularProbes;
		ER_RenderingObject* mSpecularProbeRenderingObject = nullptr;
		int* mSpecularProbesTexArrayIndicesCPUBuffer = nullptr;
		ER_GPUBuffer* mSpecularProbesTexArrayIndicesGPUBuffer = nullptr;
		ER_GPUBuffer* mSpecularProbesCellsIndicesGPUBuffer = nullptr;
		ER_GPUBuffer* mSpecularProbesPositionsGPUBuffer = nullptr;

		ER_GPUTexture* mTempSpecularCubemapFacesRT = nullptr;
		ER_GPUTexture* mTempSpecularCubemapFacesConvolutedRT = nullptr;
		DepthTarget* mTempSpecularCubemapDepthBuffers[CUBEMAP_FACES_COUNT] = { nullptr };
		ER_GPUTexture* mSpecularCubemapArrayRT = nullptr;
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

		std::wstring mLevelPath;
		bool mEnabled = true;
	};
}