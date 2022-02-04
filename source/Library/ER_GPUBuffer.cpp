#include "ER_GPUBuffer.h"
#include "GameException.h"

namespace Library
{
	ER_GPUBuffer::ER_GPUBuffer(ID3D11Device* device, void* aData, UINT objectsCount, UINT byteStride, D3D11_USAGE usage, UINT bindFlags, UINT cpuAccessFlags, UINT miscFlags, bool needUAV)
	{
		D3D11_SUBRESOURCE_DATA init_data = { aData, 0, 0 };
		mByteSize = objectsCount * byteStride;

		D3D11_BUFFER_DESC buf_desc;
		buf_desc.ByteWidth = objectsCount * byteStride;
		buf_desc.Usage = usage;
		buf_desc.BindFlags = bindFlags;
		buf_desc.CPUAccessFlags = cpuAccessFlags;
		buf_desc.MiscFlags = miscFlags;
		buf_desc.StructureByteStride = byteStride;
		if (FAILED(device->CreateBuffer(&buf_desc, aData != NULL ? &init_data : NULL, &mBuffer)))
			throw GameException("Failed to create GPU buffer.");

		if (needUAV)
		{
			// uav for positions
			D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
			uav_desc.Format = DXGI_FORMAT_UNKNOWN;
			uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			uav_desc.Buffer.FirstElement = 0;
			uav_desc.Buffer.NumElements = objectsCount;
			uav_desc.Buffer.Flags = 0;
			if (FAILED(device->CreateUnorderedAccessView(mBuffer, &uav_desc, &mBufferUAV)))
				throw GameException("Failed to create UAV of GPU buffer.");
		}
	}

	ER_GPUBuffer::~ER_GPUBuffer()
	{
		ReleaseObject(mBuffer);
		ReleaseObject(mBufferUAV);
	}

	void ER_GPUBuffer::Map(ID3D11DeviceContext* context, D3D11_MAP mapFlags, D3D11_MAPPED_SUBRESOURCE* outMappedResource)
	{
		if (FAILED(context->Map(mBuffer, 0, mapFlags, 0, outMappedResource)))
			throw GameException("Failed to map GPU buffer.");
	}

	void ER_GPUBuffer::Unmap(ID3D11DeviceContext* context)
	{
		context->Unmap(mBuffer, 0);
	}

	void ER_GPUBuffer::Update(ID3D11DeviceContext* context, void* aData, int dataSize, D3D11_MAP mapFlags)
	{
		assert(mByteSize == dataSize);

		D3D11_MAPPED_SUBRESOURCE mappedResource;
		ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
		Map(context, mapFlags, &mappedResource);
		memcpy(mappedResource.pData, aData, dataSize);
		Unmap(context);
	}

}