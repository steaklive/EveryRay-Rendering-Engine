#include "ER_RHI_DX11_GPUBuffer.h"
#include "..\..\ER_Utility.h"
#include "..\..\ER_CoreException.h"

namespace Library
{

	ER_RHI_DX11_GPUBuffer::ER_RHI_DX11_GPUBuffer()
	{

	}

	ER_RHI_DX11_GPUBuffer::~ER_RHI_DX11_GPUBuffer()
	{
		ReleaseObject(mBuffer);
		ReleaseObject(mBufferSRV);
		ReleaseObject(mBufferUAV);
	}

	void ER_RHI_DX11_GPUBuffer::CreateGPUBufferResource(ER_RHI* aRHI, void* aData, UINT objectsCount, UINT byteStride, bool isDynamic /*= false*/, ER_RHI_BIND_FLAG bindFlags /*= 0*/, UINT cpuAccessFlags /*= 0*/, UINT miscFlags /*= 0*/, ER_RHI_FORMAT format /*= ER_FORMAT_UNKNOWN*/)
	{
		assert(aRHI);
		ER_RHI_DX11* aRHIDX11 = static_cast<ER_RHI_DX11*>(aRHI);
		ID3D11Device* device = aRHIDX11->GetDevice();
		assert(device);

		D3D11_SUBRESOURCE_DATA init_data = { aData, 0, 0 };

		mRHIFormat = format;
		mFormat = aRHIDX11->GetFormat(format);
		mStride = byteStride;
		mByteSize = objectsCount * byteStride;

		D3D11_BUFFER_DESC buf_desc;
		buf_desc.ByteWidth = objectsCount * byteStride;
		buf_desc.Usage = isDynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
		buf_desc.BindFlags = bindFlags;
		buf_desc.CPUAccessFlags = cpuAccessFlags;
		buf_desc.MiscFlags = miscFlags;
		buf_desc.StructureByteStride = byteStride;
		if (FAILED(device->CreateBuffer(&buf_desc, aData != NULL ? &init_data : NULL, &mBuffer)))
			throw ER_CoreException("ER_RHI_DX11: Failed to create GPU buffer.");

		if (buf_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
			srv_desc.Format = mFormat;
			srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			srv_desc.Buffer.FirstElement = 0;
			srv_desc.Buffer.NumElements = objectsCount;
			if (FAILED(device->CreateShaderResourceView(mBuffer, &srv_desc, &mBufferSRV)))
				throw ER_CoreException("ER_RHI_DX11: Failed to create SRV of GPU buffer.");

		}

		if (buf_desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
		{
			// uav for positions
			D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
			uav_desc.Format = mFormat;
			uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			uav_desc.Buffer.FirstElement = 0;
			uav_desc.Buffer.NumElements = objectsCount;
			uav_desc.Buffer.Flags = 0;
			if (FAILED(device->CreateUnorderedAccessView(mBuffer, &uav_desc, &mBufferUAV)))
				throw ER_CoreException("ER_RHI_DX11: Failed to create UAV of GPU buffer.");
		}
	}

	void ER_RHI_DX11_GPUBuffer::Map(ER_RHI* aRHI, D3D11_MAP mapFlags, D3D11_MAPPED_SUBRESOURCE* outMappedResource)
	{
		assert(aRHI);
		ER_RHI_DX11* aRHIDX11 = static_cast<ER_RHI_DX11*>(aRHI);
		ID3D11DeviceContext1* context = aRHIDX11->GetContext();
		assert(context);

		if (FAILED(context->Map(mBuffer, 0, mapFlags, 0, outMappedResource)))
			throw ER_CoreException("ER_RHI_DX11: Failed to map GPU buffer.");
	}

	void ER_RHI_DX11_GPUBuffer::Unmap(ER_RHI* aRHI)
	{
		assert(aRHI);
		ER_RHI_DX11* aRHIDX11 = static_cast<ER_RHI_DX11*>(aRHI);
		ID3D11DeviceContext1* context = aRHIDX11->GetContext();
		assert(context);

		context->Unmap(mBuffer, 0);
	}

	void ER_RHI_DX11_GPUBuffer::Update(ER_RHI* aRHI, void* aData, int dataSize)
	{
		assert(aRHI);
		aRHI->UpdateBuffer(this, aData, dataSize);
	}

}