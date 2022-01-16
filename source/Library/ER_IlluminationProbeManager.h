#pragma once
#define CUBEMAP_FACES_COUNT 6
#define SPECULAR_PROBE_MIP_COUNT 6

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

		void ComputeOrLoad(Game& game, const GameTime& gameTime, const LightProbeRenderingObjectsInfo& objectsToRender, Skybox* skybox, const std::wstring& levelPath, bool forceRecompute = false);
		void UpdateProbe(const GameTime& gameTime);
		void SetPosition(const XMFLOAT3& position) { mPosition = position; }
		const XMFLOAT3& GetPosition() { return mPosition; }
		ID3D11ShaderResourceView* GetCubemapSRV() const { return mCubemapFacesConvolutedRT->getSRV(); }
	private:
		void Compute(Game& game, const GameTime& gameTime, const std::wstring& levelPath, const LightProbeRenderingObjectsInfo& objectsToRender, Skybox* skybox = nullptr);
		void DrawGeometryToProbe(Game& game, const GameTime& gameTime, const LightProbeRenderingObjectsInfo& objectsToRender, Skybox* skybox);
		void ConvoluteProbe(Game& game);
		
		void PrecullObjectsPerFace();
		void UpdateStandardLightingPBRMaterialVariables(Rendering::RenderingObject* obj, int meshIndex, int cubeFaceIndex);
		
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
		QuadRenderer* mQuadRenderer; //TODO maybe move to manager
		ConstantBuffer<LightProbeCBufferData::ProbeConvolutionCB> mConvolutionCB;

		ID3D11VertexShader* mConvolutionVS = nullptr;
		ID3D11PixelShader* mConvolutionPS = nullptr;
		ID3D11SamplerState* mLinearSamplerState = nullptr; //TODO remove
		ID3D11InputLayout* mInputLayout = nullptr; //TODO remove

		XMFLOAT3 mPosition;
		int mSize;
		bool mIsComputed = false;
		int mIndex;
	};

	class ER_IlluminationProbeManager
	{
	public:
		using ProbesRenderingObjectsInfo = std::map<std::string, Rendering::RenderingObject*>;
		ER_IlluminationProbeManager(Game& game, Camera& camera, Scene* scene, DirectionalLight& light, ShadowMapper& shadowMapper);
		~ER_IlluminationProbeManager();

		void SetLevelPath(const std::wstring& aPath) { mLevelPath = aPath; };
		void ComputeOrLoadProbes(Game& game, const GameTime& gameTime, ProbesRenderingObjectsInfo& aObjects, Skybox* skybox = nullptr);
		void DrawDebugProbes(Game& game, Scene* scene, ER_ProbeType aType);
		void UpdateDebugLightProbeMaterialVariables(Rendering::RenderingObject* obj, int meshIndex);
		const ER_LightProbe* GetDiffuseLightProbe(int index) const { return mDiffuseProbes[index]; }
		const ER_LightProbe* GetSpecularLightProbe(int index) const { return mSpecularProbes[index]; }
	private:
		Camera& mMainCamera;

		std::vector<ER_LightProbe*> mDiffuseProbes;
		std::vector<ER_LightProbe*> mSpecularProbes;
		std::wstring mLevelPath;
	};
}