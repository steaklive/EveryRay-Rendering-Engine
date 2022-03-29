#pragma once
#include "ER_IlluminationProbeManager.h"

namespace Library
{
	namespace LightProbeCBufferData {
		struct ProbeConvolutionCB
		{
			int FaceIndex;
			int MipIndex;
		};
	}

	class ER_LightProbe
	{
		using LightProbeRenderingObjectsInfo = std::map<std::string, ER_RenderingObject*>;
	public:
		ER_LightProbe(Game& game, DirectionalLight& light, ER_ShadowMapper& shadowMapper, int size, ER_ProbeType aType);
		~ER_LightProbe();

		void Compute(Game& game, const GameTime& gameTime, ER_GPUTexture* aTextureNonConvoluted, ER_GPUTexture* aTextureConvoluted, DepthTarget** aDepthBuffers,
			const std::wstring& levelPath, const LightProbeRenderingObjectsInfo& objectsToRender, QuadRenderer* quadRenderer, Skybox* skybox = nullptr);
		void UpdateProbe(const GameTime& gameTime);
		bool LoadProbeFromDisk(Game& game, const std::wstring& levelPath);
		ID3D11ShaderResourceView* GetCubemapSRV() const { return mCubemapTexture->GetSRV(); }
		ID3D11Texture2D* GetCubemapTexture2D() const { return mCubemapTexture->GetTexture2D(); }

		//TODO refactor
		void SetShaderInfoForConvolution(ID3D11PixelShader* ps, ID3D11SamplerState* ss)
		{
			mConvolutionPS = ps;
			mLinearSamplerState = ss;
		}

		void SetPosition(const XMFLOAT3& pos);
		const XMFLOAT3& GetPosition() { return mPosition; }
		void SetIndex(int index) { mIndex = index; }
		int GetIndex() { return mIndex; }

		void CPUCullAgainstProbeBoundingVolume(int volumeIndex, const XMFLOAT3& aMin, const XMFLOAT3& aMax);
		bool IsCulled(int volumeIndex) { return mIsCulled[volumeIndex]; }
		bool IsTaggedByVolume(int volumeIndex) { return mIsTaggedByVolume[volumeIndex]; }
		void SetIsTaggedByVolume(int volumeIndex) { mIsTaggedByVolume[volumeIndex] = true; }
		bool IsLoadedFromDisk() { return mIsProbeLoadedFromDisk; }
	private:
		void DrawGeometryToProbe(Game& game, const GameTime& gameTime, ER_GPUTexture* aTextureNonConvoluted, DepthTarget** aDepthBuffers, const LightProbeRenderingObjectsInfo& objectsToRender, Skybox* skybox);
		void ConvoluteProbe(Game& game, QuadRenderer* quadRenderer, ER_GPUTexture* aTextureNonConvoluted, ER_GPUTexture* aTextureConvoluted);
		void SaveProbeOnDisk(Game& game, const std::wstring& levelPath, ER_GPUTexture* aTextureConvoluted);
		std::wstring GetConstructedProbeName(const std::wstring& levelPath);

		ER_ProbeType mProbeType;

		DirectionalLight& mDirectionalLight;
		ER_ShadowMapper& mShadowMapper;

		LightProbeRenderingObjectsInfo mObjectsToRenderPerFace[CUBEMAP_FACES_COUNT];
		Camera* mCubemapCameras[CUBEMAP_FACES_COUNT];

		ER_GPUTexture* mCubemapTexture = nullptr;
		ConstantBuffer<LightProbeCBufferData::ProbeConvolutionCB> mConvolutionCB;

		ID3D11SamplerState* mLinearSamplerState = nullptr; //TODO remove
		ID3D11VertexShader* mConvolutionVS = nullptr;
		ID3D11PixelShader* mConvolutionPS = nullptr;
		ID3D11InputLayout* mInputLayout = nullptr; //TODO remove

		XMFLOAT3 mPosition;
		int mSize = 0;
		int mIndex = 0;
		bool mIsProbeLoadedFromDisk = false;
		bool mIsComputed = false;
		bool mIsCulled[NUM_PROBE_VOLUME_CASCADES] = { false, false };
		bool mIsTaggedByVolume[NUM_PROBE_VOLUME_CASCADES] = { false, false };
	};
}