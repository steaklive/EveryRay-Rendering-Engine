#pragma once
#include "Common.h"
#include "ER_CoreComponent.h"
#include "ER_LightProbesManager.h"

#include "RHI/ER_RHI.h"

#define NUM_VOXEL_GI_CASCADES 2
#define NUM_VOXEL_GI_TEX_MIPS 6

#define MAX_NUM_POINT_LIGHTS 64 // keep in sync with Lighting.hlsli; deprecate once tiled rendering is implemented

namespace EveryRay_Core
{
	class ER_CoreTime;
	class ER_DirectionalLight;
	class ER_Camera;
	class ER_Scene;
	class ER_GBuffer;
	class ER_ShadowMapper;
	class ER_FoliageManager;
	class ER_RenderableAABB;
	class ER_RenderingObject;
	class ER_Skybox;
	class ER_VolumetricFog;

	// TODO: At the moment our GIQuality config only affects VCT and it's resolution (off, 0.5, 0.75), we should also add:
	// - voxel resolution for vct per config (currently its hardcoded in voxelCascadesSizes)
	// - light probes settings (i.e., limit the amount of reflection probes per config)
	// - maybe something else...
	enum GIQuality
	{
		GI_LOW = 0,
		GI_MEDIUM,
		GI_HIGH
	};

	struct PointLightData
	{
		XMFLOAT4 PositionRadius;
		XMFLOAT4 ColorIntensity;
	};

	namespace IlluminationCBufferData {
		struct ER_ALIGN_GPU_BUFFER VoxelizationDebugCB
		{
			XMMATRIX WorldVoxelCube;
			XMMATRIX ViewProjection;
		};
		struct ER_ALIGN_GPU_BUFFER VoxelConeTracingMainCB
		{
			XMFLOAT4 VoxelCameraPositions[NUM_VOXEL_GI_CASCADES];
			XMFLOAT4 WorldVoxelScales[NUM_VOXEL_GI_CASCADES];
			XMFLOAT4 CameraPos;
			XMFLOAT2 UpsampleRatio;
			float IndirectDiffuseStrength;
			float IndirectSpecularStrength;
			float MaxConeTraceDistance;
			float AOFalloff;
			float SamplingFactor;
			float VoxelSampleOffset;
			float GIPower;
			XMFLOAT3 pad0;
		};
		struct ER_ALIGN_GPU_BUFFER CompositeTotalIlluminationCB
		{
			XMFLOAT4 DebugVoxelAO_Disable;
		};
		struct ER_ALIGN_GPU_BUFFER UpsampleBlurCB
		{
			bool Upsample;
		};
		struct ER_ALIGN_GPU_BUFFER DeferredLightingCB
		{
			XMMATRIX ShadowMatrices[NUM_SHADOW_CASCADES];
			XMMATRIX ViewProj;
			XMFLOAT4 ShadowCascadeDistances;
			XMFLOAT4 ShadowTexelSize;
			XMFLOAT4 SunDirection;
			XMFLOAT4 SunColor;
			XMFLOAT4 CameraPosition;
			XMFLOAT4 CameraNearFarPlanes;
			float SSSTranslucency;
			float SSSWidth;
			float SSSDirectionLightMaxPlane;
			float SSSAvailable;
			bool HasGlobalProbe;
		};
		struct ER_ALIGN_GPU_BUFFER ForwardLightingCB
		{
			XMMATRIX ShadowMatrices[NUM_SHADOW_CASCADES];
			XMMATRIX ViewProjection;
			XMFLOAT4 ShadowTexelSize;
			XMFLOAT4 ShadowCascadeDistances;
			XMFLOAT4 SunDirection;
			XMFLOAT4 SunColor;
			XMFLOAT4 CameraPosition;
		};
		struct ER_ALIGN_GPU_BUFFER LightProbesCB
		{
			XMFLOAT4 DiffuseProbesCellsCount; //x,y,z,total
			XMFLOAT4 SpecularProbesCellsCount; //x,y,z,total
			XMFLOAT4 SceneLightProbesBounds; //volume's extent of all scene's probes
			float DistanceBetweenDiffuseProbes;
			float DistanceBetweenSpecularProbes;
		};
	}

	class ER_Illumination : public ER_CoreComponent
	{
	public:
		ER_Illumination(ER_Core& game, ER_Camera& camera, const ER_DirectionalLight& light, const ER_ShadowMapper& shadowMapper, const ER_Scene* scene, GIQuality quality);
		~ER_Illumination();

		void Initialize(const ER_Scene* scene);

		void DrawLocalIllumination(ER_GBuffer* gbuffer, ER_Skybox* skybox);
		void DrawDynamicGlobalIllumination(ER_GBuffer* gbuffer, const ER_CoreTime& gameTime);
		void CompositeTotalIllumination();

		void DrawDebugGizmos(ER_RHI_GPUTexture* aRenderTarget, ER_RHI_GPUTexture* aDepth, ER_RHI_GPURootSignature* rs);
		void DrawDebugProbes(ER_RHI_GPUTexture* aRenderTarget, ER_RHI_GPUTexture* aDepth);

		void Update(const ER_CoreTime& gameTime, const ER_Scene* scene);
		void Config() { mShowDebug = !mShowDebug; }

		void SetShadowMap(ER_RHI_GPUTexture* tex) { mShadowMap = tex; }
		void SetFoliageSystemForGI(ER_FoliageManager* foliageSystem);
		void SetProbesManager(ER_LightProbesManager* manager) { mProbesManager = manager; }

		void PreparePipelineForForwardLighting(ER_RenderingObject* aObj);
		void PrepareResourcesForForwardLighting(ER_RenderingObject* aObj, int meshIndex, int lod);

		ER_RHI_GPUTexture* GetLocalIlluminationRT() const { return mLocalIlluminationRT; }
		ER_RHI_GPUTexture* GetFinalIlluminationRT() const { return mFinalIlluminationRT; }
		ER_RHI_GPUTexture* GetGBufferDepth() const;

		void SetSSS(bool val) { mIsSSS = val; }
		bool IsSSSBlurring() { return mIsSSS && !mIsSSSCulled; }
		float GetSSSWidth() { return mSSSWidth; }
		void SetSSSWidth(float val) { mSSSWidth = val; }
		float GetSSSStrength() { return mSSSStrength; }
		void SetSSSStrength(float val) { mSSSStrength = val; }
		float GetSSSTranslucency() { return mSSSTranslucency; }
		void SetSSSTranslucency(float val) { mSSSTranslucency = val; }
		float GetSSSDirLightPlaneScale() { return mSSSDirectionalLightPlaneScale; }
		void SetSSSDirLightPlaneScale(float val) { mSSSDirectionalLightPlaneScale = val; }

		bool IsDebugSkipIndirectLighting() { return mDebugSkipIndirectProbeLighting; }
	private:
		void DrawDeferredLighting(ER_GBuffer* gbuffer, ER_RHI_GPUTexture* aRenderTarget);
		void DrawForwardLighting(ER_GBuffer* gbuffer, ER_RHI_GPUTexture* aRenderTarget);

		void UpdateImGui();
		void UpdateVoxelCameraPosition();

		void CPUCullObjectsAgainstVoxelCascades(const ER_Scene* scene);

		ER_Camera& mCamera;
		const ER_DirectionalLight& mDirectionalLight;
		const ER_ShadowMapper& mShadowMapper;

		ER_LightProbesManager* mProbesManager = nullptr;
		ER_FoliageManager* mFoliageSystem = nullptr;
		ER_VolumetricFog* mVolumetricFog = nullptr;
		ER_GBuffer* mGbuffer = nullptr;

		using RenderingObjectInfo = std::map<std::string, ER_RenderingObject*>;
		RenderingObjectInfo mVoxelizationObjects[NUM_VOXEL_GI_CASCADES];

		ER_RHI_GPUConstantBuffer<IlluminationCBufferData::VoxelizationDebugCB> mVoxelizationDebugConstantBuffer;
		ER_RHI_GPUConstantBuffer<IlluminationCBufferData::VoxelConeTracingMainCB> mVoxelConeTracingMainConstantBuffer;
		ER_RHI_GPUConstantBuffer<IlluminationCBufferData::UpsampleBlurCB> mUpsampleBlurConstantBuffer;
		ER_RHI_GPUConstantBuffer<IlluminationCBufferData::CompositeTotalIlluminationCB> mCompositeTotalIlluminationConstantBuffer;
		ER_RHI_GPUConstantBuffer<IlluminationCBufferData::DeferredLightingCB> mDeferredLightingConstantBuffer;
		ER_RHI_GPUConstantBuffer<IlluminationCBufferData::ForwardLightingCB> mForwardLightingConstantBuffer;
		ER_RHI_GPUConstantBuffer<IlluminationCBufferData::LightProbesCB> mLightProbesConstantBuffer;

		ER_RHI_GPUTexture* mVCTVoxelCascades3DRTs[NUM_VOXEL_GI_CASCADES] = { nullptr, nullptr };
		ER_RHI_GPUTexture* mVCTVoxelizationDebugRT = nullptr;
		ER_RHI_GPUTexture* mVCTMainRT = nullptr;
		ER_RHI_GPUTexture* mVCTUpsampleAndBlurRT = nullptr;

		ER_RHI_GPUTexture* mLocalIlluminationRT = nullptr;
		ER_RHI_GPUTexture* mFinalIlluminationRT = nullptr;

		ER_RHI_GPUTexture* mDepthBuffer = nullptr;
		ER_RHI_GPUTexture* mShadowMap = nullptr;

		ER_RHI_GPUBuffer* mPointLightsBuffer = nullptr;

		ER_RHI_GPUShader* mVCTVoxelizationDebugVS = nullptr;
		ER_RHI_GPUShader* mVCTVoxelizationDebugGS = nullptr;
		ER_RHI_GPUShader* mVCTVoxelizationDebugPS = nullptr;
		std::string mVoxelizationDebugPSOName = "ER_RHI_GPUPipelineStateObject: VCT GI - Voxelization Pass Debug";
		ER_RHI_GPURootSignature* mVoxelizationRS = nullptr;
		ER_RHI_GPURootSignature* mVoxelizationDebugRS = nullptr;

		ER_RHI_GPUShader* mVCTMainCS = nullptr;
		std::string mVCTMainPSOName = "ER_RHI_GPUPipelineStateObject: VCT GI - Main Pass";
		ER_RHI_GPURootSignature* mVCTRS = nullptr;

		ER_RHI_GPUShader* mUpsampleBlurCS = nullptr;
		std::string mUpsampleBlurPSOName = "ER_RHI_GPUPipelineStateObject: Upsample and Blur Pass";
		ER_RHI_GPURootSignature* mUpsampleAndBlurRS = nullptr;

		ER_RHI_GPUShader* mCompositeIlluminationCS = nullptr;
		std::string mCompositeIlluminationPSOName = "ER_RHI_GPUPipelineStateObject: Composite Illumination Pass";
		ER_RHI_GPURootSignature* mCompositeIlluminationRS = nullptr;

		ER_RHI_GPUShader* mDeferredLightingCS = nullptr;
		std::string mDeferredLightingPSOName = "ER_RHI_GPUPipelineStateObject: Deferred Lighting Pass";
		ER_RHI_GPURootSignature* mDeferredLightingRS = nullptr;

		ER_RHI_GPUShader* mForwardLightingVS = nullptr;
		ER_RHI_GPUShader* mForwardLightingVS_Instancing = nullptr;
		ER_RHI_GPUShader* mForwardLightingPS = nullptr;
		ER_RHI_GPUShader* mForwardLightingPS_Transparent = nullptr;

		std::string mForwardLightingPSOName = "ER_RHI_GPUPipelineStateObject: Forward Lighting Pass";
		std::string mForwardLightingWireframePSOName = "ER_RHI_GPUPipelineStateObject: Forward Lighting Pass (Wireframe)";

		std::string mForwardLightingInstancingPSOName = "ER_RHI_GPUPipelineStateObject: Forward Lighting (Instancing) Pass";
		std::string mForwardLightingInstancingWireframePSOName = "ER_RHI_GPUPipelineStateObject: Forward Lighting (Wireframe)(Instancing) Pass";

		std::string mForwardLightingTransparentPSOName = "ER_RHI_GPUPipelineStateObject: Forward Lighting Pass (Transparent)";
		std::string mForwardLightingTransparentWireframePSOName = "ER_RHI_GPUPipelineStateObject: Forward Lighting Pass (Wireframe)(Transparent)";

		std::string mForwardLightingTransparentInstancingPSOName = "ER_RHI_GPUPipelineStateObject: Forward Lighting (Instancing) Pass (Transparent)";
		std::string mForwardLightingTransparentInstancingWireframePSOName = "ER_RHI_GPUPipelineStateObject: Forward Lighting (Wireframe)(Instancing) Pass (Transparent)";
		ER_RHI_GPURootSignature* mForwardLightingRS = nullptr;

		ER_RHI_GPUShader* mForwardLightingDiffuseProbesPS = nullptr;
		std::string mForwardLightingDiffuseProbesPSOName = "ER_RHI_GPUPipelineStateObject: Forward Lighting Diffuse Probes Pass";

		ER_RHI_GPUShader* mForwardLightingSpecularProbesPS = nullptr;
		std::string mForwardLightingSpecularProbesPSOName = "ER_RHI_GPUPipelineStateObject: Forward Lighting Specular Probes Pass";

		ER_RHI_InputLayout* mForwardLightingRenderingObjectInputLayout = nullptr;
		ER_RHI_InputLayout* mForwardLightingRenderingObjectInputLayout_Instancing = nullptr;

		ER_RHI_GPURootSignature* mDebugProbesRenderRS = nullptr;

		//VCT GI
		XMFLOAT4 mVoxelCameraPositions[NUM_VOXEL_GI_CASCADES];
		ER_AABB mLocalVoxelCascadesAABBs[NUM_VOXEL_GI_CASCADES]; // constant, must not change after initialization
		ER_AABB mWorldVoxelCascadesAABBs[NUM_VOXEL_GI_CASCADES]; // dynamic, changes with camera movement (not in every frame probably in order to save perf)
		ER_RenderableAABB* mDebugVoxelZonesGizmos[NUM_VOXEL_GI_CASCADES] = { nullptr, nullptr };
		float mWorldVoxelScales[NUM_VOXEL_GI_CASCADES] = { 2.0f, 0.5f };

		float mVCTIndirectDiffuseStrength = 0.2f;
		float mVCTIndirectSpecularStrength = 1.0f;
		float mVCTMaxConeTraceDistance = 100.0f;
		float mVCTAoFalloff = 15.0f;
		float mVCTSamplingFactor = 0.5f;
		float mVCTVoxelSampleOffset = 0.0f;
		float mVCTGIPower = 1.0f;
		float mVCTDownscaleFactor = 0.5f; // % from full-res RT
		bool mIsVCTVoxelCameraPositionsUpdated = true;
		bool mShowVCTVoxelizationOnly = false;
		bool mShowVCTAmbientOcclusionOnly = false;
		bool mDrawVCTVoxelZonesGizmos = false;
		bool mIsVCTEnabled = false;

		//light probes
		bool mDrawDiffuseProbes = false;
		bool mDrawSpecularProbes = false;
		bool mDebugSkipIndirectProbeLighting = false;

		//SSS
		bool mIsSSS = true; //global on/off flag
		bool mIsSSSCulled = false; //if there are any SSS objects on screen (false)
		float mSSSTranslucency = 0.993f;
		float mSSSWidth = 0.013f;
		float mSSSStrength = 1.35f;
		float mSSSDirectionalLightPlaneScale = 0.15f;

		bool mShowDebug = false;
		bool mDebugShadowCascades = false; // only works in deferred mode

		RenderingObjectInfo mForwardPassObjects;
		GIQuality mCurrentGIQuality;
	};
}