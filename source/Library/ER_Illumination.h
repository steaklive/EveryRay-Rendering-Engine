#pragma once
#include "Common.h"
#include "GameComponent.h"
#include "ConstantBuffer.h"
#include "ER_GPUTexture.h"
#include "DepthTarget.h"
#include "ER_LightProbesManager.h"

#define NUM_VOXEL_GI_CASCADES 2
#define NUM_VOXEL_GI_TEX_MIPS 6
#define VCT_GI_MAIN_PASS_DOWNSCALE 0.5

namespace Library
{
	class GameTime;
	class DirectionalLight;
	class ER_Camera;
	class ER_Scene;
	class ER_GBuffer;
	class ER_ShadowMapper;
	class ER_FoliageManager;
	class IBLRadianceMap;
	class ER_RenderableAABB;
	class ER_RenderingObject;
	class ER_GPUBuffer;
	class ER_Skybox;
	class ER_VolumetricFog;

	namespace IlluminationCBufferData {
		struct VoxelizationDebugCB
		{
			XMMATRIX WorldVoxelCube;
			XMMATRIX ViewProjection;
		};
		struct VoxelConeTracingMainCB
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
		struct CompositeTotalIlluminationCB
		{
			XMFLOAT4 DebugVoxelAO;
		};
		struct UpsampleBlurCB
		{
			bool Upsample;
		};
		struct DeferredLightingCB
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
		struct ForwardLightingCB
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
		struct LightProbesCB
		{
			XMFLOAT4 DiffuseProbesCellsCount; //x,y,z,total
			XMFLOAT4 SpecularProbesCellsCount; //x,y,z,total
			XMFLOAT4 SceneLightProbesBounds; //volume's extent of all scene's probes
			float DistanceBetweenDiffuseProbes;
			float DistanceBetweenSpecularProbes;
		};
	}

	class ER_Illumination : public GameComponent
	{
	public:
		ER_Illumination(Game& game, ER_Camera& camera, const DirectionalLight& light, const ER_ShadowMapper& shadowMapper, const ER_Scene* scene);
		~ER_Illumination();

		void Initialize(const ER_Scene* scene);

		void DrawLocalIllumination(ER_GBuffer* gbuffer, ER_Skybox* skybox);
		void DrawGlobalIllumination(ER_GBuffer* gbuffer, const GameTime& gameTime);
		void CompositeTotalIllumination();

		void DrawDebugGizmos();

		void Update(const GameTime& gameTime, const ER_Scene* scene);
		void Config() { mShowDebug = !mShowDebug; }

		void SetShadowMapSRV(ID3D11ShaderResourceView* srv) { mShadowMapSRV = srv; }
		void SetFoliageSystemForGI(ER_FoliageManager* foliageSystem);
		void SetProbesManager(ER_LightProbesManager* manager) { mProbesManager = manager; }

		void PrepareForForwardLighting(ER_RenderingObject* aObj, int meshIndex);

		ER_GPUTexture* GetLocalIlluminationRT() const { return mLocalIlluminationRT; }
		ER_GPUTexture* GetFinalIlluminationRT() const { return mFinalIlluminationRT; }

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
		void DrawDeferredLighting(ER_GBuffer* gbuffer, ER_GPUTexture* aRenderTarget);
		void DrawForwardLighting(ER_GBuffer* gbuffer, ER_GPUTexture* aRenderTarget);

		void UpdateImGui();
		void UpdateVoxelCameraPosition();

		void CPUCullObjectsAgainstVoxelCascades(const ER_Scene* scene);

		ER_Camera& mCamera;
		const DirectionalLight& mDirectionalLight;
		const ER_ShadowMapper& mShadowMapper;

		ER_LightProbesManager* mProbesManager = nullptr;
		ER_FoliageManager* mFoliageSystem = nullptr;
		ER_VolumetricFog* mVolumetricFog = nullptr;

		using RenderingObjectInfo = std::map<std::string, ER_RenderingObject*>;
		RenderingObjectInfo mVoxelizationObjects[NUM_VOXEL_GI_CASCADES];

		ConstantBuffer<IlluminationCBufferData::VoxelizationDebugCB> mVoxelizationDebugConstantBuffer;
		ConstantBuffer<IlluminationCBufferData::VoxelConeTracingMainCB> mVoxelConeTracingMainConstantBuffer;
		ConstantBuffer<IlluminationCBufferData::UpsampleBlurCB> mUpsampleBlurConstantBuffer;
		ConstantBuffer<IlluminationCBufferData::CompositeTotalIlluminationCB> mCompositeTotalIlluminationConstantBuffer;
		ConstantBuffer<IlluminationCBufferData::DeferredLightingCB> mDeferredLightingConstantBuffer;
		ConstantBuffer<IlluminationCBufferData::ForwardLightingCB> mForwardLightingConstantBuffer;
		ConstantBuffer<IlluminationCBufferData::LightProbesCB> mLightProbesConstantBuffer;

		std::vector<ER_GPUTexture*> mVCTVoxelCascades3DRTs;
		ER_GPUTexture* mVCTVoxelizationDebugRT = nullptr;
		ER_GPUTexture* mVCTMainRT = nullptr;
		ER_GPUTexture* mVCTUpsampleAndBlurRT = nullptr;

		ER_GPUTexture* mLocalIlluminationRT = nullptr;
		ER_GPUTexture* mFinalIlluminationRT = nullptr;

		ER_GBuffer* mGbuffer = nullptr;

		DepthTarget* mDepthBuffer = nullptr;

		ID3D11VertexShader* mVCTVoxelizationDebugVS = nullptr;
		ID3D11GeometryShader* mVCTVoxelizationDebugGS = nullptr;
		ID3D11PixelShader* mVCTVoxelizationDebugPS = nullptr;
		ID3D11ComputeShader* mVCTMainCS = nullptr;
		ID3D11ComputeShader* mUpsampleBlurCS = nullptr;
		ID3D11ComputeShader* mCompositeIlluminationCS = nullptr;
		ID3D11ComputeShader* mDeferredLightingCS = nullptr;
		ID3D11VertexShader* mForwardLightingVS = nullptr;
		ID3D11VertexShader* mForwardLightingVS_Instancing = nullptr;
		ID3D11PixelShader* mForwardLightingPS = nullptr;
		ID3D11PixelShader* mForwardLightingDiffuseProbesPS = nullptr;
		ID3D11PixelShader* mForwardLightingSpecularProbesPS = nullptr;

		ID3D11ShaderResourceView* mShadowMapSRV = nullptr;

		ID3D11DepthStencilState* mDepthStencilStateRW = nullptr;
		
		ID3D11InputLayout* mForwardLightingRenderingObjectInputLayout = nullptr;
		ID3D11InputLayout* mForwardLightingRenderingObjectInputLayout_Instancing = nullptr;

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