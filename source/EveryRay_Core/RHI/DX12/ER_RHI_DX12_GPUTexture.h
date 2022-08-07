#pragma once
#include "ER_RHI_DX11.h"

namespace EveryRay_Core
{
	class ER_RHI_DX11_GPUTexture : public ER_RHI_GPUTexture
	{
	public:
		ER_RHI_DX11_GPUTexture();
		virtual ~ER_RHI_DX11_GPUTexture();

		virtual void CreateGPUTextureResource(ER_RHI* aRHI, UINT width, UINT height, UINT samples, ER_RHI_FORMAT format, ER_RHI_BIND_FLAG bindFlags = ER_BIND_NONE,
			int mip = 1, int depth = -1, int arraySize = 1, bool isCubemap = false, int cubemapArraySize = -1) override;
		virtual void CreateGPUTextureResource(ER_RHI* aRHI, const std::string& aPath, bool isFullPath = false, bool is3D = false) override;
		virtual void CreateGPUTextureResource(ER_RHI* aRHI, const std::wstring& aPath, bool isFullPath = false, bool is3D = false) override;

		virtual void* GetRTV(void* aEmpty = nullptr) override { return mRTVs[0]; }
		virtual void* GetRTV(int index) override { return mRTVs[index]; }

		ID3D11RenderTargetView* GetRTV() { return mRTVs[0]; }
		ID3D11RenderTargetView** GetRTVs() { return mRTVs; }

		virtual void* GetDSV() override { return mDSV; }
		ID3D11DepthStencilView* GetDSV(bool readOnly = false) { if (readOnly) return mDSV_ReadOnly; else return mDSV; }

		virtual void* GetSRV() override { return mSRV; }
		void SetSRV(ID3D11ShaderResourceView* SRV) { mSRV = SRV; }

		ID3D11Texture2D* GetTexture2D() { return mTexture2D; }
		void SetTexture2D(ID3D11Texture2D* t2d) { mTexture2D = t2d; }

		ID3D11Texture3D* GetTexture3D() { return mTexture3D; }
		void SetTexture3D(ID3D11Texture3D* t3d) { mTexture3D = t3d; }

		virtual void* GetUAV() override { return mUAVs[0]; }
		ID3D11UnorderedAccessView** GetUAVs() { return mUAVs; }

		UINT GetMips() override { return mMipLevels; }
		UINT GetWidth() override { return mWidth; }
		UINT GetHeight() override { return mHeight; }
		UINT GetDepth() override { return mDepth; }

		bool IsLoadedFromFile() { return mIsLoadedFromFile; }

	private:
		void LoadFallbackTexture(ER_RHI* aRHI, ID3D11Resource** texture, ID3D11ShaderResourceView** textureView);

		ID3D11RenderTargetView** mRTVs = nullptr;
		ID3D11UnorderedAccessView** mUAVs = nullptr;
		ID3D11ShaderResourceView* mSRV = nullptr;
		ID3D11DepthStencilView* mDSV = nullptr;
		ID3D11DepthStencilView* mDSV_ReadOnly = nullptr;
		ID3D11Texture2D* mTexture2D = nullptr;
		ID3D11Texture3D* mTexture3D = nullptr;

		UINT mMipLevels = 0;
		UINT mBindFlags = 0;
		UINT mWidth = 0;
		UINT mHeight = 0;
		UINT mDepth = 0;
		UINT mArraySize = 0;
		bool mIsCubemap = false;
		bool mIsDepthStencil = false;
		bool mIsLoadedFromFile = false;
	};
}
