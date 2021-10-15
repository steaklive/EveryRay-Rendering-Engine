#pragma once
#include "Common.h"
#include "ConstantBuffer.h"
#include "CustomRenderTarget.h"
#include "DepthTarget.h"
#include "RenderingObject.h"

#define VCT_SCENE_VOLUME_SIZE 256
#define VCT_MIPS 6

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

	namespace IlluminationCBufferData {
		struct VoxelizationCB
		{
			XMMATRIX WorldVoxelCube;
			XMMATRIX ViewProjection;
			XMMATRIX ShadowMatrices[MAX_NUM_OF_CASCADES];
			XMFLOAT4 ShadowTexelSize;
			XMFLOAT4 ShadowCascadeDistances;
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
				return mVCTMainRT->getSRV();
		}
	private:
		void UpdateVoxelizationGIMaterialVariables(Rendering::RenderingObject* obj, int meshIndex);
		void UpdateImGui();

		Camera& mCamera;
		DirectionalLight& mDirectionalLight;
		ShadowMapper& mShadowMapper;

		FoliageSystem* mFoliageSystem = nullptr;

		ConstantBuffer<IlluminationCBufferData::VoxelizationCB> mVoxelizationConstantBuffer;
		ConstantBuffer<IlluminationCBufferData::VoxelConeTracingCB> mVoxelConeTracingConstantBuffer;

		CustomRenderTarget* mVCTVoxelization3DRT = nullptr;
		CustomRenderTarget* mVCTVoxelizationDebugRT = nullptr;
		CustomRenderTarget* mVCTMainRT = nullptr;
		CustomRenderTarget* mVCTMainUpsampleAndBlurRT = nullptr;

		DepthTarget* mDepthBuffer = nullptr;

		ID3D11VertexShader* mVCTVoxelizationDebugVS = nullptr;
		ID3D11GeometryShader* mVCTVoxelizationDebugGS = nullptr;
		ID3D11PixelShader* mVCTVoxelizationDebugPS = nullptr;
		ID3D11ComputeShader* mVCTMainCS = nullptr;

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
		bool mVCTMainRTUseUpsampleAndBlur = true;
		float mVCTGIPower = 1.0f;

		bool mVoxelizationDebugView = false;

		bool mEnabled = false;
		bool mShowDebug = false;
	};
}