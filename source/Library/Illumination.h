#pragma once
#include "Common.h"
#include "ConstantBuffer.h"
#include "CustomRenderTarget.h"
#include "DepthTarget.h"
#include "RenderingObject.h"

#define VCT_SCENE_VOLUME_SIZE 256
#define VCT_MIPS 6
#define VCT_MAIN_RT_DOWNSCALE 0.5

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

	namespace IlluminationCBufferData {
		struct VoxelizationCB
		{
			XMMATRIX WorldVoxelCube;
			XMMATRIX ViewProjection;
			XMMATRIX ShadowMatrices[MAX_NUM_OF_CASCADES];
			XMFLOAT4 ShadowTexelSize;
			XMFLOAT4 ShadowCascadeDistances;
			XMFLOAT4 CameraPos;
			float WorldVoxelScale;
		};
		struct VoxelConeTracingCB
		{
			XMFLOAT4 CameraPos;
			XMFLOAT2 UpsampleRatio;
			float IndirectDiffuseStrength;
			float IndirectSpecularStrength;
			float MaxConeTraceDistance;
			float AOFalloff;
			float SamplingFactor;
			float VoxelSampleOffset;
			float GIPower;
		};
		struct UpsampleBlurCB
		{
			bool Upsample;
		};
	}

	class Illumination : public GameComponent
	{
	public:
		Illumination(Game& game, Camera& camera, DirectionalLight& light, ShadowMapper& shadowMapper, const Scene* scene);
		~Illumination();

		void Initialize(const Scene* scene);

		void Draw(const GameTime& gameTime, const Scene* scene, GBuffer* gbuffer);
		void Update(const GameTime& gameTime);
		void Config() { mShowDebug = !mShowDebug; }

		void SetShadowMapSRV(ID3D11ShaderResourceView* srv) { mShadowMapSRV = srv; }
		bool GetDebugVoxels() { return mVoxelizationDebugView; }

		void SetFoliageSystem(FoliageSystem* foliageSystem);

		ID3D11ShaderResourceView* GetGISRV() { 
			if (mVoxelizationDebugView)
				return mVCTVoxelizationDebugRT->getSRV();
			else
				return mVCTUpsampleAndBlurRT->getSRV();
		}

		ID3D11ShaderResourceView* GetIBLIrradianceDiffuseSRV() { return mIrradianceDiffuseTextureSRV; }
		ID3D11ShaderResourceView* GetIBLIrradianceSpecularSRV() { return mIrradianceSpecularTextureSRV; }
		ID3D11ShaderResourceView* GetIBLIntegrationSRV() { return mIrradianceDiffuseTextureSRV; }
	private:
		void UpdateVoxelizationGIMaterialVariables(Rendering::RenderingObject* obj, int meshIndex);
		void UpdateImGui();

		Camera& mCamera;
		DirectionalLight& mDirectionalLight;
		ShadowMapper& mShadowMapper;

		FoliageSystem* mFoliageSystem = nullptr;

		ConstantBuffer<IlluminationCBufferData::VoxelizationCB> mVoxelizationConstantBuffer;
		ConstantBuffer<IlluminationCBufferData::VoxelConeTracingCB> mVoxelConeTracingConstantBuffer;
		ConstantBuffer<IlluminationCBufferData::UpsampleBlurCB> mUpsampleBlurConstantBuffer;

		CustomRenderTarget* mVCTVoxelization3DRT = nullptr;
		CustomRenderTarget* mVCTVoxelizationDebugRT = nullptr;
		CustomRenderTarget* mVCTMainRT = nullptr;
		CustomRenderTarget* mVCTUpsampleAndBlurRT = nullptr;

		DepthTarget* mDepthBuffer = nullptr;

		ID3D11VertexShader* mVCTVoxelizationDebugVS = nullptr;
		ID3D11GeometryShader* mVCTVoxelizationDebugGS = nullptr;
		ID3D11PixelShader* mVCTVoxelizationDebugPS = nullptr;
		ID3D11ComputeShader* mVCTMainCS = nullptr;
		ID3D11ComputeShader* mUpsampleBlurCS = nullptr;

		ID3D11ShaderResourceView* mShadowMapSRV = nullptr;

		ID3D11DepthStencilState* mDepthStencilStateRW = nullptr;
		ID3D11SamplerState* mLinearSamplerState = nullptr;

		float mWorldVoxelScale = VCT_SCENE_VOLUME_SIZE * 0.5f;
		float mVCTIndirectDiffuseStrength = 1.0f;
		float mVCTIndirectSpecularStrength = 1.0f;
		float mVCTMaxConeTraceDistance = 100.0f;
		float mVCTAoFalloff = 2.0f;
		float mVCTSamplingFactor = 0.5f;
		float mVCTVoxelSampleOffset = 0.0f;
		float mVCTRTRatio = 0.5f; // from MAX_SCREEN_WIDTH/HEIGHT
		bool mVCTUseMainCompute = true;
		float mVCTGIPower = 1.0f;

		bool mVoxelizationDebugView = false;

		bool mEnabled = false;
		bool mShowDebug = false;

		ID3D11ShaderResourceView* mIrradianceDiffuseTextureSRV = nullptr;
		ID3D11ShaderResourceView* mIrradianceSpecularTextureSRV = nullptr;
		ID3D11ShaderResourceView* mIntegrationMapTextureSRV = nullptr;
		std::unique_ptr<IBLRadianceMap> mIBLRadianceMap;
	};
}