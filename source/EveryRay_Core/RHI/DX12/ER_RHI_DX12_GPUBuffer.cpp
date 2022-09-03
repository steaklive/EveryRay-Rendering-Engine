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
		assert(aData && objectsCount > 0);
		assert(aRHI);
		ER_RHI_DX12* aRHIDX12 = static_cast<ER_RHI_DX12*>(aRHI);
		ID3D12Device* device = aRHIDX12->GetDevice();
		assert(device);

		ER_RHI_DX12_GPUDescriptorHeapManager* descriptorHeapManager = aRHIDX12->GetDescriptorHeapManager();
		assert(descriptorHeapManager);

		mRHIFormat = format;
		mFormat = aRHIDX12->GetFormat(format);
		mStride = byteStride;
		mSize = ER_ALIGN(objectsCount * byteStride, 256);
		if (bindFlags & ER_RHI_BIND_FLAG::ER_BIND_UNORDERED_ACCESS)
			mResourceFlags |= D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		if (isDynamic && !(bindFlags & ER_RHI_BIND_FLAG::ER_BIND_CONSTANT_BUFFER))
			mResourceState = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST;

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
		heapProperties.Type = mHeapType;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProperties.CreationNodeMask = 1;
		heapProperties.VisibleNodeMask = 1;

		if (FAILED(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, mResourceState, nullptr, &mBuffer)))
			throw ER_CoreException("ER_RHI_DX12: Failed to create committed resource of GPU buffer.");
		
		if (bindFlags & ER_RHI_BIND_FLAG::ER_BIND_CONSTANT_BUFFER)
		{
			if (FAILED(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(mSize),
				D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, &mBufferUpload)))
				throw ER_CoreException("ER_RHI_DX12: Failed to create committed resource of GPU buffer (upload).");

			mBufferCBVHandle = descriptorHeapManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			cbvDesc.BufferLocation = mBufferUpload->GetGPUVirtualAddress();
			cbvDesc.SizeInBytes = mSize;
			if (FAILED(device->CreateConstantBufferView(&cbvDesc, mBufferCBVHandle.GetCPUHandle())))
				throw ER_CoreException("ER_RHI_DX12: Failed to create constant buffer view of GPU buffer.");

			return;
		}

		if (isDynamic)
		{
			if (FAILED(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(mSize),
				D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, &mBufferUpload)))
				throw ER_CoreException("ER_RHI_DX12: Failed to create committed resource of GPU buffer (upload).");

			// store buffer in upload heap
			D3D12_SUBRESOURCE_DATA data = {};
			data.pData = reinterpret_cast<BYTE*>(aData);
			data.RowPitch = mSize;
			data.SlicePitch = data.RowPitch;

			// we are now creating a command with the command list to copy the data from
			// the upload heap to the default heap
			UpdateSubresources(commandList, mBuffer, mBufferUpload, 0, 0, 1, &data);
			if (bindFlags & ER_BIND_VERTEX_BUFFER)
				commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
			else if (bindFlags & ER_BIND_INDEX_BUFFER)
				commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));
			else
				commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON));
		}

		if (bindFlags & ER_RHI_BIND_FLAG::ER_BIND_SHADER_RESOURCE)
		{
			mBufferSRVHandle = descriptorHeapManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = mFormat;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Buffer.NumElements = objectsCount;
			srvDesc.Buffer.StructureByteStride = byteStride;
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			if (FAILED(device->CreateShaderResourceView(mBuffer, &srvDesc, mBufferSRVHandle.GetCPUHandle())))
				throw ER_CoreException("ER_RHI_DX12: Failed to create shader resource view of GPU buffer.");
		}

		if (bindFlags & ER_RHI_BIND_FLAG::ER_BIND_UNORDERED_ACCESS)
		{
			mBufferUAVHandle = descriptorHeapManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = mFormat;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uavDesc.Buffer.NumElements = objectsCount;
			uavDesc.Buffer.StructureByteStride = byteStride;
			uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
			if (FAILED(device->CreateUnorderedAccessView(mBuffer, nullptr, &uavDesc, mBufferUAVHandle.GetCPUHandle())))
				throw ER_CoreException("ER_RHI_DX12: Failed to create unordered access view of GPU buffer.");
		}

	}

	void ER_RHI_DX12_GPUBuffer::Map(ER_RHI* aRHI, void* aOutData)
	{
		assert(aRHI);
		ER_RHI_DX12* aRHIDX12 = static_cast<ER_RHI_DX12*>(aRHI);

		assert(mBufferUpload);

		D3D12_RANGE range(0, 0);
		if (FAILED(mBufferUpload->Map(0, &range, &aOutData))
			throw ER_CoreException("ER_RHI_DX12: Failed to map GPU buffer.");
	}

	void ER_RHI_DX12_GPUBuffer::Unmap(ER_RHI* aRHI)
	{
		assert(aRHI);
		ER_RHI_DX12* aRHIDX12 = static_cast<ER_RHI_DX12*>(aRHI);
		assert(mBufferUpload);

		mBufferUpload->Unmap(0, nullptr);
	}

	void ER_RHI_DX12_GPUBuffer::Update(ER_RHI* aRHI, void* aData, int dataSize)
	{
		assert(aRHI);
		aRHI->UpdateBuffer(this, aData, dataSize);
	}

}