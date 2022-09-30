#pragma once
#include "ER_RHI_DX12.h"
#include "ER_RHI_DX12_GPUDescriptorHeapManager.h"

namespace EveryRay_Core
{
	class ER_RHI_DX12_GPUBuffer : public ER_RHI_GPUBuffer
	{
	public:
		ER_RHI_DX12_GPUBuffer();
		virtual ~ER_RHI_DX12_GPUBuffer();

		virtual void CreateGPUBufferResource(ER_RHI* aRHI, void* aData, UINT objectsCount, UINT byteStride, bool isDynamic = false, ER_RHI_BIND_FLAG bindFlags = ER_BIND_NONE, UINT cpuAccessFlags = 0, ER_RHI_RESOURCE_MISC_FLAG miscFlags = ER_RESOURCE_MISC_NONE, ER_RHI_FORMAT format = ER_FORMAT_UNKNOWN) override;
		virtual void* GetBuffer() override { return mBuffer.Get(); }
		virtual void* GetSRV() override { return nullptr; }
		virtual void* GetUAV() override { return nullptr; }
		virtual int GetSize() override { return mSize; }
		virtual UINT GetStride() override { return mStride; }
		virtual ER_RHI_FORMAT GetFormatRhi() override { return mRHIFormat; }
		virtual void* GetResource() { return mBuffer.Get(); }
		
		virtual ER_RHI_RESOURCE_STATE GetCurrentState() { return mResourceState; }
		virtual void SetCurrentState(ER_RHI_RESOURCE_STATE aState) { mResourceState = aState; }

		inline virtual bool IsBuffer() override { return true; }

		ER_RHI_DX12_DescriptorHandle& GetUAVDescriptorHandle() { return mBufferUAVHandle; }
		ER_RHI_DX12_DescriptorHandle& GetSRVDescriptorHandle() { return mBufferSRVHandle; }
		ER_RHI_DX12_DescriptorHandle& GetCBVDescriptorHandle() { return mBufferCBVHandle; }
		
		D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() { return mVertexBufferView; }
		D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() { return mIndexBufferView; }

		void Map(ER_RHI* aRHI, void** aOutData);
		void Unmap(ER_RHI* aRHI);
		void Update(ER_RHI* aRHI, void* aData, int dataSize);
		DXGI_FORMAT GetFormat() { return mFormat; }
	private:
		void UpdateSubresource(ER_RHI* aRHI, void* aData, int aSize, int cmdListIndex);
		ComPtr<ID3D12Resource> mBuffer;
		ComPtr<ID3D12Resource> mBufferUpload;

		ER_RHI_DX12_DescriptorHandle mBufferUAVHandle;
		ER_RHI_DX12_DescriptorHandle mBufferSRVHandle;
		ER_RHI_DX12_DescriptorHandle mBufferCBVHandle;

		DXGI_FORMAT mFormat;
		ER_RHI_FORMAT mRHIFormat;
		ER_RHI_RESOURCE_STATE mResourceState = ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_COMMON;

		UINT mStride;
		int mSize = 0;

		D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
		D3D12_INDEX_BUFFER_VIEW mIndexBufferView;

		D3D12_RESOURCE_FLAGS mResourceFlags = D3D12_RESOURCE_FLAG_NONE;
		D3D12_HEAP_TYPE mHeapType = D3D12_HEAP_TYPE_DEFAULT;

		ER_RHI_BIND_FLAG mBindFlags;
		unsigned char* mMappedData;
		bool mIsDynamic = false;
	};
}