#pragma once
#define CUBEMAP_FACES_COUNT 6
#define MAIN_CAMERA_PROBE_VOLUME_SIZE 45

#define DIFFUSE_PROBE_SIZE 32
#define DISTANCE_BETWEEN_DIFFUSE_PROBES 15

#define SPECULAR_PROBE_SIZE 128
#define DISTANCE_BETWEEN_SPECULAR_PROBES 30
#define SPECULAR_PROBE_MIP_COUNT 6

static const int MaxNonCulledDiffuseProbesCountPerAxis = MAIN_CAMERA_PROBE_VOLUME_SIZE * 2 / DISTANCE_BETWEEN_DIFFUSE_PROBES;
static const int MaxNonCulledDiffuseProbesCount = MaxNonCulledDiffuseProbesCountPerAxis * MaxNonCulledDiffuseProbesCountPerAxis * MaxNonCulledDiffuseProbesCountPerAxis;
static const int MaxNonCulledSpecularProbesCountPerAxis = MAIN_CAMERA_PROBE_VOLUME_SIZE * 2 / DISTANCE_BETWEEN_SPECULAR_PROBES;
static const int MaxNonCulledSpecularProbesCount = MaxNonCulledSpecularProbesCountPerAxis * MaxNonCulledSpecularProbesCountPerAxis * MaxNonCulledSpecularProbesCountPerAxis;

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

	enum ER_ProbeType
	{
		DIFFUSE_PROBE,
		SPECULAR_PROBE
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

		void CPUCullAgainstProbeBoundingVolume(const XMFLOAT3& aMin, const XMFLOAT3& aMax);
		bool IsCulled() { return mIsCulled; }
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
		bool mIsCulled = false; //i.e., we can cull-check against custom bounding box positioned at main camera
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
	private:
		void UpdateProbesByType(Game& game, ER_ProbeType aType, const XMFLOAT3& minBounds, const XMFLOAT3& maxBounds);
		QuadRenderer* mQuadRenderer = nullptr;
		Camera& mMainCamera;
		RenderableAABB* mDebugProbeVolumeGizmo;

		Rendering::RenderingObject* mDiffuseProbeRenderingObject; // deleted in scene
		Rendering::RenderingObject* mSpecularProbeRenderingObject; // deleted in scene

		std::vector<ER_LightProbe*> mDiffuseProbes;
		std::vector<ER_LightProbe*> mSpecularProbes;

		std::vector<int> mNonCulledDiffuseProbesIndices;
		std::vector<int> mNonCulledSpecularProbesIndices;

		int mNonCulledDiffuseProbesCount = 0;
		int mNonCulledSpecularProbesCount = 0;

		ER_GPUTexture* mDiffuseCubemapArrayRT;
		ER_GPUTexture* mSpecularCubemapArrayRT;

		ER_GPUTexture* mDiffuseCubemapFacesRT;
		ER_GPUTexture* mDiffuseCubemapFacesConvolutedRT;
		ER_GPUTexture* mSpecularCubemapFacesRT;
		ER_GPUTexture* mSpecularCubemapFacesConvolutedRT;
		DepthTarget* mDiffuseCubemapDepthBuffers[CUBEMAP_FACES_COUNT];
		DepthTarget* mSpecularCubemapDepthBuffers[CUBEMAP_FACES_COUNT];

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

		int mSpecularProbesCountTotal = 0;
		int mSpecularProbesCountX = 0;
		int mSpecularProbesCountY = 0;
		int mSpecularProbesCountZ = 0;
	};
}