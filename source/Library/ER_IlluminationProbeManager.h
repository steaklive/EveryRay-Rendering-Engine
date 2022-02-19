#pragma once
#define CUBEMAP_FACES_COUNT 6

#define DIFFUSE_PROBE_SIZE 32 //cubemap dimension
#define DISTANCE_BETWEEN_DIFFUSE_PROBES 15

#define SPECULAR_PROBE_SIZE 128 //cubemap dimension
#define DISTANCE_BETWEEN_SPECULAR_PROBES 30
#define SPECULAR_PROBE_MIP_COUNT 6

#define PROBE_COUNT_PER_CELL 8 // 3D cube cell of probes in each vertex

#define NUM_PROBE_VOLUME_CASCADES 2

static const int ProbesVolumeCascadeSizes[NUM_PROBE_VOLUME_CASCADES] = { 45, 225 };

#include "Common.h"
#include "RenderingObject.h"
#include "ER_GPUTexture.h"
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
		using ProbesRenderingObjectsInfo = std::map<std::string, Rendering::RenderingObject*>;
		ER_IlluminationProbeManager(Game& game, Camera& camera, Scene* scene, DirectionalLight& light, ShadowMapper& shadowMapper);
		~ER_IlluminationProbeManager();

		bool AreProbesReady() { return mDiffuseProbesReady && mSpecularProbesReady; }
		void SetLevelPath(const std::wstring& aPath) { mLevelPath = aPath; };
		void ComputeOrLoadProbes(Game& game, const GameTime& gameTime, ProbesRenderingObjectsInfo& aObjects, Skybox* skybox = nullptr);
		void DrawDebugProbes(ER_ProbeType aType, int volumeIndex);
		void DrawDebugProbesVolumeGizmo(int volumeIndex);
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

		const XMFLOAT3& GetProbesVolumeCascade(int volumeIndex) { 
			return XMFLOAT3(ProbesVolumeCascadeSizes[volumeIndex],
				ProbesVolumeCascadeSizes[volumeIndex],
				ProbesVolumeCascadeSizes[volumeIndex]); }
		const XMFLOAT4& GetProbesCellsCount(ER_ProbeType aType, int volumeIndex);
		float GetProbesIndexSkip(ER_ProbeType aType, int volumeIndex);

		ID3D11ShaderResourceView* GetIntegrationMap() { return mIntegrationMapTextureSRV; }
		const XMFLOAT3& GetSceneProbesVolumeMin() { return mSceneProbesMinBounds; }
		const XMFLOAT3& GetSceneProbesVolumeMax() { return mSceneProbesMaxBounds; }
		bool mDebugDiscardCulledProbes = false;//used in DebugLightProbeMaterial
	private:
		void SetupDiffuseProbes(Game& game, Camera& camera, Scene* scene, DirectionalLight& light, ShadowMapper& shadowMapper);
		void SetupSpecularProbes(Game& game, Camera& camera, Scene* scene, DirectionalLight& light, ShadowMapper& shadowMapper);
		void AddProbeToCells(ER_LightProbe* aProbe, ER_ProbeType aType, const XMFLOAT3& minBounds, const XMFLOAT3& maxBounds);
		bool IsProbeInCell(ER_LightProbe* aProbe, ER_LightProbeCell& aCell, ER_AABB& aCellBounds);
		void UpdateProbesByType(Game& game, ER_ProbeType aType);
		void UpdateDebugLightProbeMaterialVariables(Rendering::RenderingObject* obj, int meshIndex, ER_ProbeType aType, int volumeIndex);
		
		QuadRenderer* mQuadRenderer = nullptr;
		Camera& mMainCamera;
		RenderableAABB* mDebugProbeVolumeGizmo[NUM_PROBE_VOLUME_CASCADES] = { nullptr };

		ID3D11VertexShader* mConvolutionVS = nullptr;
		ID3D11PixelShader* mConvolutionPS = nullptr;
		ID3D11InputLayout* mInputLayout = nullptr; //TODO remove
		ID3D11SamplerState* mLinearSamplerState = nullptr;

		ID3D11ShaderResourceView* mIntegrationMapTextureSRV = nullptr; //TODO generate custom ones

		XMFLOAT3 mSceneProbesMinBounds;
		XMFLOAT3 mSceneProbesMaxBounds;
		XMFLOAT3 mCurrentVolumesMaxBounds[NUM_PROBE_VOLUME_CASCADES];
		XMFLOAT3 mCurrentVolumesMinBounds[NUM_PROBE_VOLUME_CASCADES];

		// Diffuse probes members
		std::vector<ER_LightProbe*> mDiffuseProbes;
		Rendering::RenderingObject* mDiffuseProbeRenderingObject[NUM_PROBE_VOLUME_CASCADES] = { nullptr, nullptr };
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
		const int mDiffuseProbeIndexSkip[NUM_PROBE_VOLUME_CASCADES] = { 1, 5 };
		int mDiffuseProbesCountTotal = 0;
		int mDiffuseProbesCountX = 0;
		int mDiffuseProbesCountY = 0;
		int mDiffuseProbesCountZ = 0;
		int mDiffuseProbesCellsCountX[NUM_PROBE_VOLUME_CASCADES] = { 0 };
		int mDiffuseProbesCellsCountY[NUM_PROBE_VOLUME_CASCADES] = { 0 };
		int mDiffuseProbesCellsCountZ[NUM_PROBE_VOLUME_CASCADES] = { 0 };
		int mDiffuseProbesCellsCountTotal[NUM_PROBE_VOLUME_CASCADES] = { 0 };
		int mNonCulledDiffuseProbesCount[NUM_PROBE_VOLUME_CASCADES] = { 0 };
		int mMaxNonCulledDiffuseProbesCountPerAxis = 0;
		int mMaxNonCulledDiffuseProbesCount = 0;
		bool mDiffuseProbesReady = false;
		ER_LightProbe* mGlobalDiffuseProbe = nullptr;
		bool mGlobalDiffuseProbeReady = false;

		// Specular probes members
		std::vector<ER_LightProbe*> mSpecularProbes;
		Rendering::RenderingObject* mSpecularProbeRenderingObject[NUM_PROBE_VOLUME_CASCADES] = { nullptr, nullptr };
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
		const int mSpecularProbeIndexSkip[NUM_PROBE_VOLUME_CASCADES] = { 1, 5 };
		int mSpecularProbesCountTotal = 0;
		int mSpecularProbesCountX = 0;
		int mSpecularProbesCountY = 0;
		int mSpecularProbesCountZ = 0;
		int mSpecularProbesCellsCountX[NUM_PROBE_VOLUME_CASCADES]= { 0 };
		int mSpecularProbesCellsCountY[NUM_PROBE_VOLUME_CASCADES]= { 0 };
		int mSpecularProbesCellsCountZ[NUM_PROBE_VOLUME_CASCADES]= { 0 };
		int mSpecularProbesCellsCountTotal[NUM_PROBE_VOLUME_CASCADES] = { 0 };
		int mNonCulledSpecularProbesCount[NUM_PROBE_VOLUME_CASCADES] = { 0 };
		int mMaxNonCulledSpecularProbesCountPerAxis = 0;
		int mMaxNonCulledSpecularProbesCount = 0;
		bool mSpecularProbesReady = false;

		std::wstring mLevelPath;
		bool mEnabled = true;
	};
}