#pragma once
#include "Common.h"

namespace Library
{
	class ER_GPUBuffer
	{
	public:
		ER_GPUBuffer(ID3D11Device* device, void* aData, UINT objectsCount, UINT byteStride, D3D11_USAGE usage = D3D11_USAGE_DEFAULT, UINT bindFlags = 0, UINT cpuAccessFlags = 0, UINT miscFlags = 0, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
		~ER_GPUBuffer();

		void Map(ID3D11DeviceContext* context, D3D11_MAP mapFlags, D3D11_MAPPED_SUBRESOURCE* outMappedResource);
		void Unmap(ID3D11DeviceContext* context);
		void Update(ID3D11DeviceContext* context, void* aData, int dataSize, D3D11_MAP mapFlags);
		ID3D11Buffer* GetBuffer() { return mBuffer; }
		ID3D11ShaderResourceView* GetBufferSRV() { return mBufferSRV; }
		ID3D11UnorderedAccessView* GetBufferUAV() { return mBufferUAV; }
		DXGI_FORMAT GetFormat() { return mFormat; }
		UINT GetStride() { return mStride; }
	private:
		ID3D11Buffer* mBuffer = nullptr;
		ID3D11UnorderedAccessView* mBufferUAV = nullptr;
		ID3D11ShaderResourceView* mBufferSRV = nullptr;

		DXGI_FORMAT mFormat;
		UINT mStride;
		int mByteSize = 0;
	};
}