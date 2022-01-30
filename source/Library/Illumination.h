#pragma once
#include "Common.h"
#include "ConstantBuffer.h"
#include "ER_GPUTexture.h"
#include "DepthTarget.h"
#include "RenderingObject.h"

#define NUM_VOXEL_GI_CASCADES 2
#define NUM_VOXEL_GI_TEX_MIPS 6
#define VCT_GI_MAIN_PASS_DOWNSCALE 0.5

namespace Library
{
	class FullScreenRenderTarget;
	class DirectionalLight;
	class GameComponent;
	class GameTime;
	class Camera;
	class Scene;
	class GBuffer;
	class ShadowMapper;
	class FoliageSystem;
	class IBLRadianceMap;
	class RenderableAABB;
	class RenderingObject;
	class ER_IlluminationProbeManager;

	namespace IlluminationCBufferData {
		struct VoxelizationDebugCB
		{
			XMMATRIX WorldVoxelCube;
			XMMATRIX ViewProjection;
		};
		struct VoxelConeTracingCB
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
		struct UpsampleBlurCB
		{
			bool Upsample;
		};
		struct DeferredLightingCB
		{
			XMMATRIX ShadowMatrices[NUM_SHADOW_CASCADES];
			XMFLOAT4 ShadowCascadeDistances;
			XMFLOAT4 ShadowTexelSize;
			XMFLOAT4 SunDirection;
			XMFLOAT4 SunColor;
			XMFLOAT4 CameraPosition;
		};
	}

	class Illumination : public GameComponent
	{
	public:
		Illumination(Game& game, Camera& camera, const DirectionalLight& light, const ShadowMapper& shadowMapper, const Scene* scene);
		~Illumination();

		void Initialize(const Scene* scene);

		void DrawLocalIllumination(GBuffer* gbuffer, ER_GPUTexture* aRenderTarget, bool isEditorMode = false, bool clearInitTarget = false);
		void DrawGlobalIllumination(GBuffer* gbuffer, const GameTime& gameTime);

		void Update(const GameTime& gameTime, const Scene* scene);
		void Config() { mShowDebug = !mShowDebug; }

		void SetShadowMapSRV(ID3D11ShaderResourceView* srv) { mShadowMapSRV = srv; }
		void SetFoliageSystemForGI(FoliageSystem* foliageSystem);

		void SetProbesManager(ER_IlluminationProbeManager* manager) { mProbesManager = manager; }

		bool GetDebugVoxels() { return mDrawVoxelization; }
		bool GetDebugAO() { return mDrawAmbientOcclusionOnly; }

		ID3D11ShaderResourceView* GetGlobaIlluminationSRV() const {
			if (mDrawVoxelization)
				return mVCTVoxelizationDebugRT->GetSRV();
			else
				return mVCTUpsampleAndBlurRT->GetSRV();
		}

		float GetDirectionalLightIntensity() const { return mDirectionalLightIntensity; }
	private:
		void DrawDeferredLighting(GBuffer* gbuffer, ER_GPUTexture* aRenderTarget, bool clearTarget = false);
		void DrawForwardLighting(GBuffer* gbuffer, ER_GPUTexture* aRenderTarget);
		void DrawDebugGizmos();

		void UpdateVoxelizationGIMaterialVariables(Rendering::RenderingObject* obj, int meshIndex, int voxelCascadeIndex);
		void UpdateForwardLightingMaterial(Rendering::RenderingObject* obj, int meshIndex);
	
		void UpdateImGui();
		void UpdateVoxelCameraPosition();

		void CPUCullObjectsAgainstVoxelCascades(const Scene* scene);

		Camera& mCamera;
		const DirectionalLight& mDirectionalLight;
		const ShadowMapper& mShadowMapper;

		ER_IlluminationProbeManager* mProbesManager = nullptr;
		FoliageSystem* mFoliageSystem = nullptr;

		using RenderingObjectInfo = std::map<std::string, Rendering::RenderingObject*>;
		RenderingObjectInfo mVoxelizationObjects[NUM_VOXEL_GI_CASCADES];

		ConstantBuffer<IlluminationCBufferData::VoxelizationDebugCB> mVoxelizationDebugConstantBuffer;
		ConstantBuffer<IlluminationCBufferData::VoxelConeTracingCB> mVoxelConeTracingConstantBuffer;
		ConstantBuffer<IlluminationCBufferData::UpsampleBlurCB> mUpsampleBlurConstantBuffer;
		ConstantBuffer<IlluminationCBufferData::DeferredLightingCB> mDeferredLightingConstantBuffer;

		std::vector<ER_GPUTexture*> mVCTVoxelCascades3DRTs;
		ER_GPUTexture* mVCTVoxelizationDebugRT = nullptr;
		ER_GPUTexture* mVCTMainRT = nullptr;
		ER_GPUTexture* mVCTUpsampleAndBlurRT = nullptr;

		GBuffer* mGbuffer = nullptr;

		DepthTarget* mDepthBuffer = nullptr;

		ID3D11VertexShader* mVCTVoxelizationDebugVS = nullptr;
		ID3D11GeometryShader* mVCTVoxelizationDebugGS = nullptr;
		ID3D11PixelShader* mVCTVoxelizationDebugPS = nullptr;
		ID3D11ComputeShader* mVCTMainCS = nullptr;
		ID3D11ComputeShader* mUpsampleBlurCS = nullptr;
		ID3D11ComputeShader* mDeferredLightingCS = nullptr;

		ID3D11ShaderResourceView* mShadowMapSRV = nullptr;

		ID3D11DepthStencilState* mDepthStencilStateRW = nullptr;
		ID3D11SamplerState* mLinearSamplerState = nullptr;
		ID3D11SamplerState* mShadowSamplerState = nullptr;

		float mWorldVoxelScales[NUM_VOXEL_GI_CASCADES] = { 2.0f, 0.5f };
		XMFLOAT4 mVoxelCameraPositions[NUM_VOXEL_GI_CASCADES];
		ER_AABB mVoxelCascadesAABBs[NUM_VOXEL_GI_CASCADES];
		
		std::vector<RenderableAABB*> mDebugVoxelZonesGizmos;

		float mVCTIndirectDiffuseStrength = 1.0f;
		float mVCTIndirectSpecularStrength = 1.0f;
		float mVCTMaxConeTraceDistance = 100.0f;
		float mVCTAoFalloff = 2.0f;
		float mVCTSamplingFactor = 0.5f;
		float mVCTVoxelSampleOffset = 0.0f;
		float mVCTGIPower = 1.0f;
		bool mVoxelCameraPositionsUpdated = true;

		bool mDrawVoxelization = false;
		bool mDrawVoxelZonesGizmos = false;
		bool mDrawAmbientOcclusionOnly = false;
		bool mDrawDiffuseProbes = false;
		bool mDrawSpecularProbes = false;
		bool mDrawProbesVolumeGizmo = false;

		bool mEnabled = false;
		bool mShowDebug = false;

		float mDirectionalLightIntensity = 10.0f;

		RenderingObjectInfo mForwardPassObjects;
	};
}