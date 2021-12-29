#pragma once
#include "Common.h"
#include "ConstantBuffer.h"
#include "CustomRenderTarget.h"
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
		Illumination(Game& game, Camera& camera, DirectionalLight& light, ShadowMapper& shadowMapper, const Scene* scene);
		~Illumination();

		void Initialize(const Scene* scene);

		void DrawLocalIllumination(GBuffer* gbuffer, CustomRenderTarget* aRenderTarget, bool isEditorMode = false);
		void DrawGlobalIllumination(GBuffer* gbuffer, const GameTime& gameTime);

		void Update(const GameTime& gameTime, const Scene* scene);
		void Config() { mShowDebug = !mShowDebug; }

		void SetShadowMapSRV(ID3D11ShaderResourceView* srv) { mShadowMapSRV = srv; }
		void SetFoliageSystemForGI(FoliageSystem* foliageSystem);

		bool GetDebugVoxels() { return mDrawVoxelization; }
		bool GetDebugAO() { return mDrawAmbientOcclusionOnly; }

		ID3D11ShaderResourceView* GetGlobaIlluminationSRV() {
			if (mDrawVoxelization)
				return mVCTVoxelizationDebugRT->getSRV();
			else
				return mVCTUpsampleAndBlurRT->getSRV();
		}

		ID3D11ShaderResourceView* GetIBLIrradianceDiffuseSRV() { return mIrradianceDiffuseTextureSRV; }
		ID3D11ShaderResourceView* GetIBLIrradianceSpecularSRV() { return mIrradianceSpecularTextureSRV; }
		ID3D11ShaderResourceView* GetIBLIntegrationSRV() { return mIntegrationMapTextureSRV; }

		float GetDirectionalLightIntensity() { return mDirectionalLightIntensity; }
	private:
		void DrawDeferredLighting(GBuffer* gbuffer, CustomRenderTarget* aRenderTarget);
		void DrawForwardLighting();
		void DrawDebugGizmos();

		void UpdateVoxelizationGIMaterialVariables(Rendering::RenderingObject* obj, int meshIndex, int voxelCascadeIndex);
		void UpdateImGui();
		void UpdateVoxelCameraPosition();

		void CPUCullObjectsAgainstVoxelCascades(const Scene* scene);

		Camera& mCamera;
		DirectionalLight& mDirectionalLight;
		ShadowMapper& mShadowMapper;

		FoliageSystem* mFoliageSystem = nullptr;

		using RenderingObjectInfo = std::map<std::string, Rendering::RenderingObject*>;
		RenderingObjectInfo mVoxelizationObjects[NUM_VOXEL_GI_CASCADES];

		ConstantBuffer<IlluminationCBufferData::VoxelizationDebugCB> mVoxelizationDebugConstantBuffer;
		ConstantBuffer<IlluminationCBufferData::VoxelConeTracingCB> mVoxelConeTracingConstantBuffer;
		ConstantBuffer<IlluminationCBufferData::UpsampleBlurCB> mUpsampleBlurConstantBuffer;
		ConstantBuffer<IlluminationCBufferData::DeferredLightingCB> mDeferredLightingConstantBuffer;

		std::vector<CustomRenderTarget*> mVCTVoxelCascades3DRTs;
		CustomRenderTarget* mVCTVoxelizationDebugRT = nullptr;
		CustomRenderTarget* mVCTMainRT = nullptr;
		CustomRenderTarget* mVCTUpsampleAndBlurRT = nullptr;

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

		bool mDrawVoxelization = false;
		bool mDrawVoxelZonesGizmos = false;
		bool mDrawAmbientOcclusionOnly = false;

		bool mEnabled = false;
		bool mShowDebug = false;

		float mDirectionalLightIntensity = 5.0f;

		ID3D11ShaderResourceView* mIrradianceDiffuseTextureSRV = nullptr;
		ID3D11ShaderResourceView* mIrradianceSpecularTextureSRV = nullptr;
		ID3D11ShaderResourceView* mIntegrationMapTextureSRV = nullptr;
		std::unique_ptr<IBLRadianceMap> mIBLRadianceMap;

		RenderingObjectInfo mForwardPassObjects;
	};
}