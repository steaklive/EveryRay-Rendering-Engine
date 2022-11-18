#pragma once
#include "RHI/ER_RHI.h"

namespace EveryRay_Core
{
	namespace LightProbeCBufferData {
		struct ER_ALIGN_GPU_BUFFER ProbeConvolutionCB
		{
			int FaceIndex;
			int MipIndex;
		};
	}
	class ER_Camera;
	class ER_DirectionalLight;
	class ER_ShadowMapper;
	class ER_Skybox;
	class ER_CoreTime;
	class ER_QuadRenderer;
	class ER_RenderingObject;
	class ER_Core;

	class ER_LightProbe
	{
		using LightProbeRenderingObjectsInfo = std::vector<std::pair<std::string, ER_RenderingObject*>>;
	public:
		ER_LightProbe(ER_Core& game, ER_DirectionalLight* light, ER_ShadowMapper* shadowMapper, int size, int aProbeType, int index);
		~ER_LightProbe();

		void Initialize(ER_Core& game, ER_DirectionalLight* light, ER_ShadowMapper* shadowMapper, int size, int aProbeType, int index);

		void Compute(ER_Core& game, ER_RHI_GPUTexture* aTextureNonConvoluted, ER_RHI_GPUTexture* aTextureConvoluted, ER_RHI_GPUTexture** aDepthBuffers,
			const std::wstring& levelPath, const LightProbeRenderingObjectsInfo& objectsToRender, ER_QuadRenderer* quadRenderer, ER_Skybox* skybox = nullptr);
		void UpdateProbe(const ER_CoreTime& gameTime);

		bool LoadProbeFromDisk(ER_Core& game, const std::wstring& levelPath);
		bool IsLoadedFromDisk() { return mIsProbeLoadedFromDisk; }
		
		ER_RHI_GPUTexture* GetCubemapTexture() const { return mCubemapTexture; }

		//TODO refactor
		void SetShaderInfoForConvolution(ER_RHI_GPUShader* ps)	{ mConvolutionPS = ps; }

		const std::vector<XMFLOAT3>& GetSphericalHarmonics() { return mSphericalHarmonicsRGB; }

		void SetPosition(const XMFLOAT3& pos);
		const XMFLOAT3& GetPosition() { return mPosition; }
		void SetIndex(int index) { mIndex = index; }
		int GetIndex() { return mIndex; }

		void CPUCullAgainstProbeBoundingVolume(const XMFLOAT3& aMin, const XMFLOAT3& aMax);
		bool IsCulled() { return mIsCulled; }
	private:
		void StoreSphericalHarmonicsFromCubemap(ER_Core& game, ER_RHI_GPUTexture* aTextureConvoluted);
		void SaveProbeOnDisk(ER_Core& game, const std::wstring& levelPath, ER_RHI_GPUTexture* aTextureConvoluted);
		void DrawGeometryToProbe(ER_Core& game, ER_RHI_GPUTexture* aTextureNonConvoluted, ER_RHI_GPUTexture** aDepthBuffers, const LightProbeRenderingObjectsInfo& objectsToRender, ER_Skybox* skybox);
		void ConvoluteProbe(ER_Core& game, ER_QuadRenderer* quadRenderer, ER_RHI_GPUTexture* aTextureNonConvoluted, ER_RHI_GPUTexture* aTextureConvoluted);
		std::wstring GetConstructedProbeName(const std::wstring& levelPath, bool inSphericalHarmonics = false);

		int mProbeType;

		ER_DirectionalLight* mDirectionalLight = nullptr;
		ER_ShadowMapper* mShadowMapper = nullptr;

		LightProbeRenderingObjectsInfo mObjectsToRenderPerFace[6];
		ER_Camera* mCubemapCameras[6];

		ER_RHI_GPUTexture* mCubemapTexture = nullptr; // for regular diffuse probe it should be null (because we use SH)
		ER_RHI_GPUConstantBuffer<LightProbeCBufferData::ProbeConvolutionCB> mConvolutionCB;

		ER_RHI_GPUShader* mConvolutionPS = nullptr; //just a pointer (deleted in the mananager) TODO remove

		std::vector<XMFLOAT3> mSphericalHarmonicsRGB;

		XMFLOAT3 mPosition;
		int mSize = 0;
		int mIndex = 0;
		bool mIsProbeLoadedFromDisk = false;
		bool mIsComputed = false;
		bool mIsCulled = false;
	};
}