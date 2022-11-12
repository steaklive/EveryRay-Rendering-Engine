#pragma once
#include "ER_RHI_DX12.h"
#include "ER_RHI_DX12_GPUDescriptorHeapManager.h"

namespace EveryRay_Core
{
	class ER_RHI_DX12_GPUTexture : public ER_RHI_GPUTexture
	{
	public:
		ER_RHI_DX12_GPUTexture(const std::wstring& aDebugName);
		virtual ~ER_RHI_DX12_GPUTexture();

		virtual void CreateGPUTextureResource(ER_RHI* aRHI, UINT width, UINT height, UINT samples, ER_RHI_FORMAT format, ER_RHI_BIND_FLAG bindFlags = ER_BIND_NONE,
			int mip = 1, int depth = -1, int arraySize = 1, bool isCubemap = false, int cubemapArraySize = -1) override;
		virtual void CreateGPUTextureResource(ER_RHI* aRHI, const std::string& aPath, bool isFullPath = false, bool is3D = false, bool skipFallback = false, bool* statusFlag = nullptr) override;
		virtual void CreateGPUTextureResource(ER_RHI* aRHI, const std::wstring& aPath, bool isFullPath = false, bool is3D = false, bool skipFallback = false, bool* statusFlag = nullptr) override;
		void CreateSimpleGPUTexture2DResource(ER_RHI* aRHI, UINT width, UINT height, DXGI_FORMAT format, ER_RHI_BIND_FLAG bindFlags = ER_BIND_NONE, int mip = 1);

		virtual void* GetRTV(void* aEmpty = nullptr) override { return nullptr; /* Not needed on DX12 */ }
		virtual void* GetRTV(int index) override { return nullptr; /* Not needed on DX12 */ }
		virtual void* GetDSV() override { return nullptr; /* Not needed on DX12 */ }
		virtual void* GetSRV() override { return nullptr; /* Not needed on DX12 */ }
		virtual void* GetUAV() override { return nullptr; /* Not needed on DX12 */ }
		virtual void* GetResource() { return mResource.Get(); }
		inline virtual bool IsBuffer() override { return false; }

		virtual ER_RHI_RESOURCE_STATE GetCurrentState() { return mCurrentResourceState; }
		virtual void SetCurrentState(ER_RHI_RESOURCE_STATE aState) { mCurrentResourceState = aState; }
		
		DXGI_FORMAT GetFormat() { return mFormat; }

		ER_RHI_DX12_DescriptorHandle& GetRTVHandle(int index = 0) { return mRTVHandles[index]; }
		ER_RHI_DX12_DescriptorHandle& GetUAVHandle(int index = 0) { return mUAVHandles[index]; }
		ER_RHI_DX12_DescriptorHandle& GetUAVHandleGPU(int index = 0) { return mUAVHandlesGPU[index]; }
		ER_RHI_DX12_DescriptorHandle& GetSRVHandle() { return mSRVHandle; }
		ER_RHI_DX12_DescriptorHandle& GetDSVHandle(bool readOnly = false) { if (readOnly) return mDSVReadOnlyHandle; else return mDSVHandle; }
 
		virtual UINT GetMips() override { return mMipLevels; }
		virtual UINT GetCalculatedMipCount() override;
		virtual UINT GetWidth() override { return mWidth; }
		virtual UINT GetHeight() override { return mHeight; }
		virtual UINT GetDepth() override { return mDepth; }

		bool IsLoadedFromFile() { return mIsLoadedFromFile; }
		const std::wstring& GetDebugName() { return mDebugName; }
	private:
		void LoadFallbackTexture(ER_RHI* aRHI);

		ER_RHI_DX12_DescriptorHandle mSRVHandle;
		ER_RHI_DX12_DescriptorHandle mDSVHandle;
		ER_RHI_DX12_DescriptorHandle mDSVReadOnlyHandle;
		std::vector<ER_RHI_DX12_DescriptorHandle> mRTVHandles;
		std::vector<ER_RHI_DX12_DescriptorHandle> mUAVHandles; //non-shader visible heap
		std::vector<ER_RHI_DX12_DescriptorHandle> mUAVHandlesGPU; //shader visible heap

		ER_RHI_RESOURCE_STATE mCurrentResourceState = ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_COMMON;
		
		ComPtr<ID3D12Resource> mResource;
		ComPtr<ID3D12Resource> mResourceUpload;
		DXGI_FORMAT mFormat;
		ER_RHI_FORMAT mRHIFormat;
		UINT mMipLevels = 0;
		UINT mBindFlags = 0;
		UINT mWidth = 0;
		UINT mHeight = 0;
		UINT mDepth = 0;
		UINT mArraySize = 0;
		bool mIsCubemap = false;
		bool mIsDepthStencil = false;
		bool mIsLoadedFromFile = false;

		std::wstring mDebugName;
	};
}
