#pragma once
#define CUBEMAP_FACES_COUNT 6

#define DIFFUSE_PROBE_SIZE 32 //cubemap dimension
#define DISTANCE_BETWEEN_DIFFUSE_PROBES 15

#define SPECULAR_PROBE_SIZE 128 //cubemap dimension
#define DISTANCE_BETWEEN_SPECULAR_PROBES 30
#define SPECULAR_PROBE_MIP_COUNT 6

#define PROBE_COUNT_PER_CELL 8 // 3D cube cell of probes in each vertex

#define NUM_PROBE_VOLUME_CASCADES 2

static const int ProbesVolumeCascadeSizes[NUM_PROBE_VOLUME_CASCADES] = { 45, 135 };

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

	enum ER_ProbeType
	{
		DIFFUSE_PROBE = 0,
		SPECULAR_PROBE = 1,

		PROBE_TYPES_COUNT = 2
	};

	namespace LightProbeCBufferData {
		struct ProbeConvolutionCB
		{
			int FaceIndex;
			int MipIndex;
		};
	}

	class ER_LightProbe
	{
		using LightProbeRenderingObjectsInfo = std::map<std::string, Rendering::RenderingObject*>;
	public:
		ER_LightProbe(Game& game, DirectionalLight& light, ShadowMapper& shadowMapper, int size, ER_ProbeType aType);
		~ER_LightProbe();

		void Compute(Game& game, const GameTime& gameTime, ER_GPUTexture* aTextureNonConvoluted, ER_GPUTexture* aTextureConvoluted, DepthTarget** aDepthBuffers,
			const std::wstring& levelPath, const LightProbeRenderingObjectsInfo& objectsToRender, QuadRenderer* quadRenderer, Skybox* skybox = nullptr);
		void UpdateProbe(const GameTime& gameTime);
		bool LoadProbeFromDisk(Game& game, const std::wstring& levelPath);
		ID3D11ShaderResourceView* GetCubemapSRV() const { return mCubemapTexture->GetSRV(); }
		ID3D11Texture2D* GetCubemapTexture2D() const { return mCubemapTexture->GetTexture2D(); }
		
		//TODO refactor
		void SetShaderInfoForConvolution(ID3D11VertexShader* vs, ID3D11PixelShader* ps, ID3D11InputLayout* il, ID3D11SamplerState* ss)
		{
			mConvolutionVS = vs;
			mConvolutionPS = ps;
			mInputLayout = il;
			mLinearSamplerState = ss;
		}

		void SetPosition(const XMFLOAT3& pos);
		const XMFLOAT3& GetPosition() { return mPosition; }
		void SetIndex(int index) { mIndex = index; }
		int GetIndex() { return mIndex; }

		void CPUCullAgainstProbeBoundingVolume(int volumeIndex, const XMFLOAT3& aMin, const XMFLOAT3& aMax);
		bool IsCulled(int volumeIndex) { return mIsCulled[volumeIndex]; }
		bool IsTaggedByVolume(int volumeIndex) { return mIsTaggedByVolume[volumeIndex]; }
		void SetIsTaggedByVolume(int volumeIndex) { mIsTaggedByVolume[volumeIndex] = true; }
		bool IsLoadedFromDisk() { return mIsProbeLoadedFromDisk; }
	private:
		void DrawGeometryToProbe(Game& game, const GameTime& gameTime, ER_GPUTexture* aTextureNonConvoluted, DepthTarget** aDepthBuffers, const LightProbeRenderingObjectsInfo& objectsToRender, Skybox* skybox);
		void ConvoluteProbe(Game& game, QuadRenderer* quadRenderer, ER_GPUTexture* aTextureNonConvoluted, ER_GPUTexture* aTextureConvoluted);
		
		void PrecullObjectsPerFace();
		void UpdateStandardLightingPBRProbeMaterialVariables(Rendering::RenderingObject* obj, int meshIndex, int cubeFaceIndex);
		
		void SaveProbeOnDisk(Game& game, const std::wstring& levelPath, ER_GPUTexture* aTextureConvoluted);
		std::wstring GetConstructedProbeName(const std::wstring& levelPath);
		
		ER_ProbeType mProbeType;

		DirectionalLight& mDirectionalLight;
		ShadowMapper& mShadowMapper;

		LightProbeRenderingObjectsInfo mObjectsToRenderPerFace[CUBEMAP_FACES_COUNT];
		Camera* mCubemapCameras[CUBEMAP_FACES_COUNT];

		ER_GPUTexture* mCubemapTexture = nullptr;
		ConstantBuffer<LightProbeCBufferData::ProbeConvolutionCB> mConvolutionCB;

		ID3D11SamplerState* mLinearSamplerState = nullptr; //TODO remove
		ID3D11VertexShader* mConvolutionVS = nullptr;
		ID3D11PixelShader* mConvolutionPS = nullptr;
		ID3D11InputLayout* mInputLayout = nullptr; //TODO remove

		XMFLOAT3 mPosition;
		int mSize;
		int mIndex;
		bool mIsProbeLoadedFromDisk = false;
		bool mIsComputed = false;
		bool mIsCulled[NUM_PROBE_VOLUME_CASCADES] = { false, false };
		bool mIsTaggedByVolume[NUM_PROBE_VOLUME_CASCADES] = { false, false };
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
		void DrawDebugProbes(ER_ProbeType aType);
		void DrawDebugProbesVolumeGizmo();
		void UpdateProbes(Game& game);
		void UpdateDebugLightProbeMaterialVariables(Rendering::RenderingObject* obj, int meshIndex, ER_ProbeType aType);
		const ER_LightProbe* GetDiffuseLightProbe(int index) const { return mDiffuseProbes[index]; }
		const ER_LightProbe* GetSpecularLightProbe(int index) const { return mSpecularProbes[index]; }
		ID3D11ShaderResourceView* GetIntegrationMap() { return mIntegrationMapTextureSRV; }
		int GetCellIndex(const XMFLOAT3& pos, ER_ProbeType aType);

		ER_GPUTexture* GetCulledDiffuseProbesTextureArray(int volumeIndex) const { return mDiffuseCubemapArrayRT[volumeIndex]; }
		ER_GPUBuffer* GetDiffuseProbesCellsIndicesBuffer() const { return mDiffuseProbesCellsIndicesGPUBuffer; }
		ER_GPUBuffer* GetDiffuseProbesTexArrayIndicesBuffer(int volumeIndex) const { return mDiffuseProbesTexArrayIndicesGPUBuffer[volumeIndex]; }
		ER_GPUBuffer* GetDiffuseProbesPositionsBuffer() const { return mDiffuseProbesPositionsGPUBuffer; }

		const XMFLOAT3& GetProbesVolumeMin() { return mSceneProbesMinBounds; }
		const XMFLOAT3& GetProbesVolumeMax() { return mSceneProbesMaxBounds; }
		const XMFLOAT4& GetProbesCellsCount(ER_ProbeType aType);

	private:
		void AddProbeToCells(ER_LightProbe* aProbe, ER_ProbeType aType, const XMFLOAT3& minBounds, const XMFLOAT3& maxBounds);
		bool IsProbeInCell(ER_LightProbe* aProbe, ER_LightProbeCell& aCell, ER_AABB& aCellBounds);
		void UpdateProbesByType(Game& game, ER_ProbeType aType);
		QuadRenderer* mQuadRenderer = nullptr;
		Camera& mMainCamera;
		RenderableAABB* mDebugProbeVolumeGizmo[NUM_PROBE_VOLUME_CASCADES];

		Rendering::RenderingObject* mDiffuseProbeRenderingObject = nullptr;
		Rendering::RenderingObject* mSpecularProbeRenderingObject = nullptr;

		std::vector<ER_LightProbe*> mDiffuseProbes;
		std::vector<ER_LightProbe*> mSpecularProbes;

		std::vector<ER_LightProbeCell> mDiffuseProbesCells;
		ER_AABB mDiffuseProbesCellBounds;

		std::vector<int> mNonCulledDiffuseProbesIndices[NUM_PROBE_VOLUME_CASCADES];
		std::vector<int> mNonCulledSpecularProbesIndices[NUM_PROBE_VOLUME_CASCADES];

		int mNonCulledDiffuseProbesCount[NUM_PROBE_VOLUME_CASCADES];
		int mNonCulledSpecularProbesCount[NUM_PROBE_VOLUME_CASCADES];

		ER_GPUTexture* mDiffuseCubemapArrayRT[NUM_PROBE_VOLUME_CASCADES];
		ER_GPUTexture* mSpecularCubemapArrayRT[NUM_PROBE_VOLUME_CASCADES];
		ER_GPUTexture* mDiffuseCubemapFacesRT;
		ER_GPUTexture* mDiffuseCubemapFacesConvolutedRT;
		ER_GPUTexture* mSpecularCubemapFacesRT;
		ER_GPUTexture* mSpecularCubemapFacesConvolutedRT;
		DepthTarget* mDiffuseCubemapDepthBuffers[CUBEMAP_FACES_COUNT];
		DepthTarget* mSpecularCubemapDepthBuffers[CUBEMAP_FACES_COUNT];

		int* mDiffuseProbesTexArrayIndicesCPUBuffer[NUM_PROBE_VOLUME_CASCADES];
		ER_GPUBuffer* mDiffuseProbesTexArrayIndicesGPUBuffer[NUM_PROBE_VOLUME_CASCADES];
		ER_GPUBuffer* mDiffuseProbesCellsIndicesGPUBuffer;
		ER_GPUBuffer* mDiffuseProbesPositionsGPUBuffer;

		bool mDiffuseProbesReady = false;
		bool mSpecularProbesReady = false;

		std::wstring mLevelPath;

		ID3D11VertexShader* mConvolutionVS = nullptr;
		ID3D11PixelShader* mConvolutionPS = nullptr;
		ID3D11InputLayout* mInputLayout = nullptr; //TODO remove
		ID3D11SamplerState* mLinearSamplerState = nullptr;

		ID3D11ShaderResourceView* mIntegrationMapTextureSRV; //TEMP

		int mDiffuseProbesCountTotal = 0;
		int mDiffuseProbesCountX = 0;
		int mDiffuseProbesCountY = 0;
		int mDiffuseProbesCountZ = 0;
		int mDiffuseProbesCellsCountX = 0;
		int mDiffuseProbesCellsCountY = 0;
		int mDiffuseProbesCellsCountZ = 0;
		int mDiffuseProbesCellsCountTotal = 0;

		int mSpecularProbesCountTotal = 0;
		int mSpecularProbesCountX = 0;
		int mSpecularProbesCountY = 0;
		int mSpecularProbesCountZ = 0;

		bool mEnabled = true;

		XMFLOAT3 mSceneProbesMinBounds;
		XMFLOAT3 mSceneProbesMaxBounds;

		XMFLOAT3 mCurrentVolumesMaxBounds[NUM_PROBE_VOLUME_CASCADES];
		XMFLOAT3 mCurrentVolumesMinBounds[NUM_PROBE_VOLUME_CASCADES];

		int MaxNonCulledDiffuseProbesCountPerAxis;
		int MaxNonCulledDiffuseProbesCount;

		int MaxNonCulledSpecularProbesCountPerAxis;
		int MaxNonCulledSpecularProbesCount;
	};
}