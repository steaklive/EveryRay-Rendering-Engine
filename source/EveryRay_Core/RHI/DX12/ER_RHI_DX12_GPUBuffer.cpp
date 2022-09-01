#include "ER_RHI_DX12_GPUBuffer.h"
#include "ER_RHI_DX12_GPUDescriptorHeapManager.h"
#include "..\..\ER_Utility.h"
#include "..\..\ER_CoreException.h"

namespace EveryRay_Core
{

	ER_RHI_DX12_GPUBuffer::ER_RHI_DX12_GPUBuffer()
	{

	}

	ER_RHI_DX12_GPUBuffer::~ER_RHI_DX12_GPUBuffer()
	{
		ReleaseObject(mBuffer);
		ReleaseObject(mBufferUpload);
	}

	void ER_RHI_DX12_GPUBuffer::CreateGPUBufferResource(ER_RHI* aRHI, void* aData, UINT objectsCount, UINT byteStride, bool isDynamic /*= false*/, ER_RHI_BIND_FLAG bindFlags /*= 0*/, UINT cpuAccessFlags /*= 0*/, ER_RHI_RESOURCE_MISC_FLAG miscFlags /*= 0*/, ER_RHI_FORMAT format /*= ER_FORMAT_UNKNOWN*/)
	{
		assert(aRHI);
		ER_RHI_DX12* aRHIDX12 = static_cast<ER_RHI_DX12*>(aRHI);
		ID3D12Device* device = aRHIDX12->GetDevice();
		assert(device);

		mRHIFormat = format;
		mFormat = aRHIDX11->GetFormat(format);
		mStride = byteStride;
		mSize = ER_ALIGN(objectsCount * byteStride, 256);

		D3D12_RESOURCE_DESC desc = {};
		desc.Alignment = 0;
		desc.DepthOrArraySize = 1;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Flags = mResourceFlags;
		desc.Format = mFormat;
		desc.Height = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Width = (UINT64)mSize;

		D3D12_HEAP_PROPERTIES heapProperties;
		heapProperties.Type = (bindFlags == ER_BIND_CONSTANT_BUFFER) ? D3D12_HEAP_TYPE_UPLOAD : mHeapType;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProperties.CreationNodeMask = 1;
		heapProperties.VisibleNodeMask = 1;

		if (FAILED(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, mResourceState, nullptr, &mBuffer)))
			throw ER_CoreException("ER_RHI_DX12: Failed to create committed resource of GPU buffer.");
		
		if (FAILED(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(mSize), 
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, &mBufferUpload)))
			throw ER_CoreException("ER_RHI_DX12: Failed to create committed resource of GPU buffer (upload).");


		D3D12_SUBRESOURCE_DATA data = {};
		data.pData = aData;
		data.RowPitch = mSize;
		data.SlicePitch = 0;

		UpdateSubresources(commandList, mBuffer.Get(), mBufferUpload.Get(), 0, 0, 1, &data);
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		D3D11_BUFFER_DESC buf_desc;
		buf_desc.ByteWidth = objectsCount * byteStride;
		if (isDynamic)
			buf_desc.Usage = D3D11_USAGE_DYNAMIC;
		else
		{
			if (cpuAccessFlags & D3D11_CPU_ACCESS_WRITE && cpuAccessFlags & D3D11_CPU_ACCESS_READ)
				buf_desc.Usage = D3D11_USAGE_STAGING;
			else
				buf_desc.Usage = D3D11_USAGE_DEFAULT;
		}
		buf_desc.BindFlags = bindFlags;
		buf_desc.CPUAccessFlags = isDynamic ? D3D11_CPU_ACCESS_WRITE : cpuAccessFlags;
		buf_desc.MiscFlags = miscFlags;
		buf_desc.StructureByteStride = byteStride;
		if (buf_desc.BindFlags & D3D11_BIND_CONSTANT_BUFFER)
			buf_desc.StructureByteStride = 0;
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