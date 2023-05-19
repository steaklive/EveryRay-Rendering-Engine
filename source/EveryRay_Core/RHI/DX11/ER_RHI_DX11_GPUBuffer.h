#pragma once
#include "ER_RHI_DX11.h"

namespace EveryRay_Core
{
	class ER_RHI_DX11_GPUBuffer : public ER_RHI_GPUBuffer
	{
	public:
		ER_RHI_DX11_GPUBuffer(const std::string& aDebugName);
		virtual ~ER_RHI_DX11_GPUBuffer();

		virtual void CreateGPUBufferResource(ER_RHI* aRHI, void* aData, UINT objectsCount, UINT byteStride,
			bool isDynamic = false, ER_RHI_BIND_FLAG bindFlags = ER_BIND_NONE, UINT cpuAccessFlags = 0, 
			ER_RHI_RESOURCE_MISC_FLAG miscFlags = ER_RESOURCE_MISC_NONE, ER_RHI_FORMAT format = ER_FORMAT_UNKNOWN, bool canAppend = false) override;
		virtual void* GetBuffer() override { return mBuffer; }
		virtual void* GetSRV() override { return mBufferSRV; }
		virtual void* GetUAV() override { return mBufferUAV; }
		virtual int GetSize() override { return mByteSize; }
		virtual UINT GetStride() override { return mStride; }
		virtual ER_RHI_FORMAT GetFormatRhi() override { return mRHIFormat; }
		virtual void* GetResource() { return mBuffer; }

		virtual ER_RHI_RESOURCE_STATE GetCurrentState() { return ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_COMMON; }
		virtual void SetCurrentState(ER_RHI_RESOURCE_STATE aState) { }

		inline virtual bool IsBuffer() override { return true; }

		void Map(ER_RHI* aRHI, D3D11_MAP mapFlags, D3D11_MAPPED_SUBRESOURCE* outMappedResource);
		void Unmap(ER_RHI* aRHI);
		void Update(ER_RHI* aRHI, void* aData, int dataSize);
		DXGI_FORMAT GetFormat() { return mFormat; }
	private:
		ID3D11Buffer* mBuffer = nullptr;
		ID3D11UnorderedAccessView* mBufferUAV = nullptr;
		ID3D11ShaderResourceView* mBufferSRV = nullptr;

		DXGI_FORMAT mFormat;
		ER_RHI_FORMAT mRHIFormat;
		UINT mStride;
		int mByteSize = 0;
	};
}