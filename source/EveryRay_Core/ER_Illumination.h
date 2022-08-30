#pragma once
#include "Common.h"
#include "ER_CoreComponent.h"
#include "ER_LightProbesManager.h"

#include "RHI/ER_RHI.h"

#define NUM_VOXEL_GI_CASCADES 2
#define NUM_VOXEL_GI_TEX_MIPS 6
#define VCT_GI_MAIN_PASS_DOWNSCALE 0.5

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

	namespace IlluminationCBufferData {
		struct ER_ALIGN16 VoxelizationDebugCB
		{
			XMMATRIX WorldVoxelCube;
			XMMATRIX ViewProjection;
		};
		struct ER_ALIGN16 VoxelConeTracingMainCB
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
		struct ER_ALIGN16 CompositeTotalIlluminationCB
		{
			XMFLOAT4 DebugVoxelAO;
		};
		struct ER_ALIGN16 UpsampleBlurCB
		{
			bool Upsample;
		};
		struct ER_ALIGN16 DeferredLightingCB
		{
			XMMATRIX ShadowMatrices[NUM_SHADOW_CASCADES];
			XMMATRIX ViewProj;
			XMFLOAT4 ShadowCascadeDistances;
			XMFLOAT4 ShadowTexelSize;
			XMFLOAT4 SunDirection;
			XMFLOAT4 SunColor;
			XMFLOAT4 CameraPosition;
			XMFLOAT4 CameraNearFarPlanes;
			float UseGlobalProbe;
			float SkipIndirectProbeLighting;
			float SSSTranslucency;
			float SSSWidth;
			float SSSDirectionLightMaxPlane;
			float SSSAvailable;
		};
		struct ER_ALIGN16 ForwardLightingCB
		{
			XMMATRIX ShadowMatrices[NUM_SHADOW_CASCADES];
			XMMATRIX ViewProjection;
			XMMATRIX World;
			XMFLOAT4 ShadowTexelSize;
			XMFLOAT4 ShadowCascadeDistances;
			XMFLOAT4 SunDirection;
			XMFLOAT4 SunColor;
			XMFLOAT4 CameraPosition;
			float UseGlobalProbe;
			float SkipIndirectProbeLighting;
		};
		struct ER_ALIGN16 LightProbesCB
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
		ER_Illumination(ER_Core& game, ER_Camera& camera, const ER_DirectionalLight& light, const ER_ShadowMapper& shadowMapper, const ER_Scene* scene);
		~ER_Illumination();

		void Initialize(const ER_Scene* scene);

		void DrawLocalIllumination(ER_GBuffer* gbuffer, ER_Skybox* skybox);
		void DrawGlobalIllumination(ER_GBuffer* gbuffer, const ER_CoreTime& gameTime);
		void CompositeTotalIllumination();

		void DrawDebugGizmos();

		void Update(const ER_CoreTime& gameTime, const ER_Scene* scene);
		void Config() { mShowDebug = !mShowDebug; }

		void SetShadowMap(ER_RHI_GPUTexture* tex) { mShadowMap = tex; }
		void SetFoliageSystemForGI(ER_FoliageManager* foliageSystem);
		void SetProbesManager(ER_LightProbesManager* manager) { mProbesManager = manager; }

		void PrepareForForwardLighting(ER_RenderingObject* aObj, int meshIndex);

		ER_RHI_GPUTexture* GetLocalIlluminationRT() const { return mLocalIlluminationRT; }
		ER_RHI_GPUTexture* GetFinalIlluminationRT() const { return mFinalIlluminationRT; }

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

		std::vector<ER_RHI_GPUTexture*> mVCTVoxelCascades3DRTs;
		ER_RHI_GPUTexture* mVCTVoxelizationDebugRT = nullptr;
		ER_RHI_GPUTexture* mVCTMainRT = nullptr;
		ER_RHI_GPUTexture* mVCTUpsampleAndBlurRT = nullptr;

		ER_RHI_GPUTexture* mLocalIlluminationRT = nullptr;
		ER_RHI_GPUTexture* mFinalIlluminationRT = nullptr;

		ER_RHI_GPUTexture* mDepthBuffer = nullptr;
		ER_RHI_GPUTexture* mShadowMap = nullptr;

		ER_RHI_GPUShader* mVCTVoxelizationDebugVS = nullptr;
		ER_RHI_GPUShader* mVCTVoxelizationDebugGS = nullptr;
		ER_RHI_GPUShader* mVCTVoxelizationDebugPS = nullptr;
		ER_RHI_GPUShader* mVCTMainCS = nullptr;
		ER_RHI_GPUShader* mUpsampleBlurCS = nullptr;
		ER_RHI_GPUShader* mCompositeIlluminationCS = nullptr;
		ER_RHI_GPUShader* mDeferredLightingCS = nullptr;
		ER_RHI_GPUShader* mForwardLightingVS = nullptr;
		ER_RHI_GPUShader* mForwardLightingVS_Instancing = nullptr;
		ER_RHI_GPUShader* mForwardLightingPS = nullptr;
		ER_RHI_GPUShader* mForwardLightingDiffuseProbesPS = nullptr;
		ER_RHI_GPUShader* mForwardLightingSpecularProbesPS = nullptr;
		
		ER_RHI_InputLayout* mForwardLightingRenderingObjectInputLayout = nullptr;
		ER_RHI_InputLayout* mForwardLightingRenderingObjectInputLayout_Instancing = nullptr;

		float mWorldVoxelScales[NUM_VOXEL_GI_CASCADES] = { 2.0f, 0.5f };
		XMFLOAT4 mVoxelCameraPositions[NUM_VOXEL_GI_CASCADES];
		ER_AABB mVoxelCascadesAABBs[NUM_VOXEL_GI_CASCADES];
		
		std::vector<ER_RenderableAABB*> mDebugVoxelZonesGizmos;

		//VCT GI
		float mVCTIndirectDiffuseStrength = 1.0f;
		float mVCTIndirectSpecularStrength = 1.0f;
		float mVCTMaxConeTraceDistance = 100.0f;
		float mVCTAoFalloff = 2.0f;
		float mVCTSamplingFactor = 0.5f;
		float mVCTVoxelSampleOffset = 0.0f;
		float mVCTGIPower = 1.0f;
		bool mIsVCTVoxelCameraPositionsUpdated = true;
		bool mShowVCTVoxelizationOnly = false;
		bool mShowVCTAmbientOcclusionOnly = false;
		bool mDrawVCTVoxelZonesGizmos = false;

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

		bool mEnabled = false;
		bool mShowDebug = false;

		RenderingObjectInfo mForwardPassObjects;
	};
}