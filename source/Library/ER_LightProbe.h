#pragma once
#include "ER_LightProbesManager.h"

namespace Library
{
	namespace LightProbeCBufferData {
		struct ProbeConvolutionCB
		{
			int FaceIndex;
			int MipIndex;
		};
	}
	class ER_Camera;

	class ER_LightProbe
	{
		using LightProbeRenderingObjectsInfo = std::map<std::string, ER_RenderingObject*>;
	public:
		ER_LightProbe(Game& game, DirectionalLight& light, ER_ShadowMapper& shadowMapper, int size, ER_ProbeType aType);
		~ER_LightProbe();

		void Compute(Game& game, ER_GPUTexture* aTextureNonConvoluted, ER_GPUTexture* aTextureConvoluted, ER_GPUTexture** aDepthBuffers,
			const std::wstring& levelPath, const LightProbeRenderingObjectsInfo& objectsToRender, ER_QuadRenderer* quadRenderer, ER_Skybox* skybox = nullptr);
		void UpdateProbe(const ER_CoreTime& gameTime);
		bool LoadProbeFromDisk(Game& game, const std::wstring& levelPath);
		ID3D11ShaderResourceView* GetCubemapSRV() const { return mCubemapTexture->GetSRV(); }
		ID3D11Texture2D* GetCubemapTexture2D() const { return mCubemapTexture->GetTexture2D(); }

		//TODO refactor
		void SetShaderInfoForConvolution(ID3D11PixelShader* ps)	{ mConvolutionPS = ps; }

		const std::vector<XMFLOAT3>& GetSphericalHarmonics() { return mSphericalHarmonicsRGB; }

		void SetPosition(const XMFLOAT3& pos);
		const XMFLOAT3& GetPosition() { return mPosition; }
		void SetIndex(int index) { mIndex = index; }
		int GetIndex() { return mIndex; }

		void CPUCullAgainstProbeBoundingVolume(const XMFLOAT3& aMin, const XMFLOAT3& aMax);
		bool IsCulled() { return mIsCulled; }
		bool IsLoadedFromDisk() { return mIsProbeLoadedFromDisk; }
	private:
		void StoreSphericalHarmonicsFromCubemap(Game& game, ER_GPUTexture* aTextureConvoluted);
		void DrawGeometryToProbe(Game& game, ER_GPUTexture* aTextureNonConvoluted, ER_GPUTexture** aDepthBuffers, const LightProbeRenderingObjectsInfo& objectsToRender, ER_Skybox* skybox);
		void ConvoluteProbe(Game& game, ER_QuadRenderer* quadRenderer, ER_GPUTexture* aTextureNonConvoluted, ER_GPUTexture* aTextureConvoluted);
		void SaveProbeOnDisk(Game& game, const std::wstring& levelPath, ER_GPUTexture* aTextureConvoluted);
		std::wstring GetConstructedProbeName(const std::wstring& levelPath, bool inSphericalHarmonics = false);

		ER_ProbeType mProbeType;

		DirectionalLight& mDirectionalLight;
		ER_ShadowMapper& mShadowMapper;

		LightProbeRenderingObjectsInfo mObjectsToRenderPerFace[CUBEMAP_FACES_COUNT];
		ER_Camera* mCubemapCameras[CUBEMAP_FACES_COUNT];

		ER_GPUTexture* mCubemapTexture = nullptr; // for regular diffuse probe it should be null (because we use SH)
		ConstantBuffer<LightProbeCBufferData::ProbeConvolutionCB> mConvolutionCB;

		ID3D11PixelShader* mConvolutionPS = nullptr;

		std::vector<XMFLOAT3> mSphericalHarmonicsRGB;

		XMFLOAT3 mPosition;
		int mSize = 0;
		int mIndex = 0;
		bool mIsProbeLoadedFromDisk = false;
		bool mIsComputed = false;
		bool mIsCulled = false;
	};
}