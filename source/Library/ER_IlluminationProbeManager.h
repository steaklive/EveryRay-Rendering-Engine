#pragma once
#define CUBEMAP_FACES_COUNT 6
#define MAIN_CAMERA_PROBE_VOLUME_SIZE 45

#define DIFFUSE_PROBE_SIZE 32
#define DISTANCE_BETWEEN_DIFFUSE_PROBES 15

#define SPECULAR_PROBE_MIP_COUNT 6

static const int MaxNonCulledProbesCountPerAxis = MAIN_CAMERA_PROBE_VOLUME_SIZE * 2 / DISTANCE_BETWEEN_DIFFUSE_PROBES;
static const int MaxNonCulledProbesCount = MaxNonCulledProbesCountPerAxis * MaxNonCulledProbesCountPerAxis * MaxNonCulledProbesCountPerAxis;

#include "Common.h"
#include "RenderingObject.h"
#include "CustomRenderTarget.h"
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
		ER_LightProbe(Game& game, DirectionalLight& light, ShadowMapper& shadowMapper, const XMFLOAT3& position, int size, ER_ProbeType aType, int index);
		~ER_LightProbe();

		void ComputeOrLoad(Game& game, const GameTime& gameTime, const LightProbeRenderingObjectsInfo& objectsToRender, QuadRenderer* quadRenderer, Skybox* skybox, const std::wstring& levelPath, bool forceRecompute = false);
		void UpdateProbe(const GameTime& gameTime);
		void SetPosition(const XMFLOAT3& position) { mPosition = position; }
		const XMFLOAT3& GetPosition() { return mPosition; }
		ID3D11ShaderResourceView* GetCubemapSRV() const { return mCubemapFacesConvolutedRT->getSRV(); }
		ID3D11Texture2D* GetCubemapTexture2D() const { return mCubemapFacesConvolutedRT->getTexture2D(); }
		
		//TODO refactor
		void SetShaderInfoForConvolution(ID3D11VertexShader* vs, ID3D11PixelShader* ps, ID3D11InputLayout* il)
		{
			mConvolutionVS = vs;
			mConvolutionPS = ps;
			mInputLayout = il;
		}

		void CPUCullAgainstProbeBoundingVolume(const XMFLOAT3& aMin, const XMFLOAT3& aMax);
		bool IsCulled() { return mIsCulled; };
	private:
		void Compute(Game& game, const GameTime& gameTime, const std::wstring& levelPath, const LightProbeRenderingObjectsInfo& objectsToRender, QuadRenderer* quadRenderer, Skybox* skybox = nullptr);
		void DrawGeometryToProbe(Game& game, const GameTime& gameTime, const LightProbeRenderingObjectsInfo& objectsToRender, Skybox* skybox);
		void ConvoluteProbe(Game& game, QuadRenderer* quadRenderer);
		
		void PrecullObjectsPerFace();
		void UpdateStandardLightingPBRnoIBLMaterialVariables(Rendering::RenderingObject* obj, int meshIndex, int cubeFaceIndex);
		
		void SaveProbeOnDisk(Game& game, const std::wstring& levelPath);
		bool LoadProbeFromDisk(Game& game, const std::wstring& levelPath);
		std::wstring GetConstructedProbeName(const std::wstring& levelPath);
		
		ER_ProbeType mProbeType;

		DirectionalLight& mDirectionalLight;
		ShadowMapper& mShadowMapper;

		LightProbeRenderingObjectsInfo mObjectsToRenderPerFace[CUBEMAP_FACES_COUNT];
		CustomRenderTarget* mCubemapFacesRT;
		CustomRenderTarget* mCubemapFacesConvolutedRT;
		DepthTarget* mDepthBuffers[CUBEMAP_FACES_COUNT];
		Camera* mCubemapCameras[CUBEMAP_FACES_COUNT];

		ConstantBuffer<LightProbeCBufferData::ProbeConvolutionCB> mConvolutionCB;

		ID3D11SamplerState* mLinearSamplerState = nullptr; //TODO remove
		ID3D11VertexShader* mConvolutionVS = nullptr;
		ID3D11PixelShader* mConvolutionPS = nullptr;
		ID3D11InputLayout* mInputLayout = nullptr; //TODO remove

		XMFLOAT3 mPosition;
		int mSize;
		int mIndex;
		bool mIsComputed = false;
		bool mIsCulled = false; //i.e., we can cull-check against custom bounding box positioned at main camera
	};

	class ER_IlluminationProbeManager
	{
	public:
		using ProbesRenderingObjectsInfo = std::map<std::string, Rendering::RenderingObject*>;
		ER_IlluminationProbeManager(Game& game, Camera& camera, Scene* scene, DirectionalLight& light, ShadowMapper& shadowMapper);
		~ER_IlluminationProbeManager();

		void SetLevelPath(const std::wstring& aPath) { mLevelPath = aPath; };
		void ComputeOrLoadProbes(Game& game, const GameTime& gameTime, ProbesRenderingObjectsInfo& aObjects, Skybox* skybox = nullptr);
		void DrawDebugProbes(ER_ProbeType aType);
		void DrawDebugProbesVolumeGizmo();
		void UpdateProbes(Game& game);
		void UpdateDebugLightProbeMaterialVariables(Rendering::RenderingObject* obj, int meshIndex);
		const ER_LightProbe* GetDiffuseLightProbe(int index) const { return mDiffuseProbes[index]; }
		const ER_LightProbe* GetSpecularLightProbe(int index) const { return mSpecularProbes[index]; }
	private:
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

		CustomRenderTarget* mDiffuseCubemapArrayRT;
		CustomRenderTarget* mSpecularCubemapArrayRT;

		bool mDiffuseProbesReady = false;
		bool mSpecularProbesReady = false;

		std::wstring mLevelPath;

		ID3D11VertexShader* mConvolutionVS = nullptr;
		ID3D11PixelShader* mConvolutionPS = nullptr;
		ID3D11InputLayout* mInputLayout = nullptr; //TODO remove

		int mDiffuseProbesCountTotal = 0;
		int mDiffuseProbesCountX = 0;
		int mDiffuseProbesCountY = 0;
		int mDiffuseProbesCountZ = 0;
	};
}