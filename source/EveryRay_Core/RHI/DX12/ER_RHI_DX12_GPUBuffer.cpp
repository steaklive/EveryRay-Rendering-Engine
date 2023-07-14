#include "ER_RHI_DX12_GPUBuffer.h"
#include "..\..\ER_Utility.h"
#include "..\..\ER_CoreException.h"

namespace EveryRay_Core
{
	ER_RHI_DX12_GPUBuffer::ER_RHI_DX12_GPUBuffer(const std::string& aDebugName)
		: mDebugName(aDebugName)
	{
	}

	ER_RHI_DX12_GPUBuffer::~ER_RHI_DX12_GPUBuffer()
	{
		mBuffer.Reset();

		for (int frameIndex = 0; frameIndex < DX12_MAX_BACK_BUFFER_COUNT; frameIndex++)
		{
			mBufferUpload[frameIndex].Reset();
			//if (mBufferUpload[frameIndex] && mIsDynamic)
			//	mBufferUpload[frameIndex]->Unmap(0, nullptr);
		}
	}

	void ER_RHI_DX12_GPUBuffer::CreateGPUBufferResource(ER_RHI* aRHI, void* aData, UINT objectsCount, UINT byteStride, bool isDynamic /*= false*/, ER_RHI_BIND_FLAG bindFlags /*= 0*/, UINT cpuAccessFlags /*= 0*/, ER_RHI_RESOURCE_MISC_FLAG miscFlags /*= 0*/, ER_RHI_FORMAT format /*= ER_FORMAT_UNKNOWN*/)
	{
		ER_RHI_DX12* aRHIDX12 = static_cast<ER_RHI_DX12*>(aRHI);
		ID3D12Device* device = aRHIDX12->GetDevice();
		assert(device);
		mIsDynamic = isDynamic;
		mBindFlags = bindFlags;
		if (bindFlags & ER_RHI_BIND_FLAG::ER_BIND_CONSTANT_BUFFER)
			assert(isDynamic);

		ER_RHI_DX12_GPUDescriptorHeapManager* descriptorHeapManager = aRHIDX12->GetDescriptorHeapManager();
		assert(descriptorHeapManager);

		mRHIFormat = format;
		mFormat = aRHIDX12->GetFormat(format);
		mStride = byteStride;
		mSize = objectsCount * byteStride;
		if ((bindFlags & ER_RHI_BIND_FLAG::ER_BIND_CONSTANT_BUFFER) || (miscFlags == ER_RHI_RESOURCE_MISC_FLAG::ER_RESOURCE_MISC_BUFFER_STRUCTURED))
			mSize = ER_BitmaskAlign(objectsCount * byteStride, ER_GPU_BUFFER_ALIGNMENT);
		if (bindFlags & ER_RHI_BIND_FLAG::ER_BIND_UNORDERED_ACCESS)
			mResourceFlags |= D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		D3D12_RESOURCE_DESC desc = {};
		desc.Alignment = 0;
		desc.DepthOrArraySize = 1;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Flags = mResourceFlags;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.Height = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Width = (UINT64)mSize;

		D3D12_HEAP_PROPERTIES heapProperties;
		heapProperties.Type = ((cpuAccessFlags & 0x10000L) && (cpuAccessFlags & 0x20000L)) ? D3D12_HEAP_TYPE_READBACK : mHeapType; // 0x10000L | 0x20000L is legacy from D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProperties.CreationNodeMask = 1;
		heapProperties.VisibleNodeMask = 1;

		if (FAILED(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, heapProperties.Type == D3D12_HEAP_TYPE_READBACK ? D3D12_RESOURCE_STATE_COPY_DEST : aRHIDX12->GetState(mResourceState), nullptr, IID_PPV_ARGS(&mBuffer))))
			throw ER_CoreException("ER_RHI_DX12: Failed to create committed resource of GPU buffer.");
		
		if (heapProperties.Type == D3D12_HEAP_TYPE_READBACK)
			return;

		desc.Flags &= ~D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		for (int frameIndex = 0; frameIndex < DX12_MAX_BACK_BUFFER_COUNT; frameIndex++)
		{
			if (FAILED(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mBufferUpload[frameIndex]))))
				throw ER_CoreException("ER_RHI_DX12: Failed to create committed resource of GPU buffer (upload).");

			if (mIsDynamic)
			{
				CD3DX12_RANGE readRange(0, 0);
				if (FAILED(mBufferUpload[frameIndex]->Map(0, &readRange, reinterpret_cast<void**>(&mMappedData[frameIndex]))))
					throw ER_CoreException("ER_RHI_DX12: Failed to map GPU buffer.");
				memcpy(mMappedData[frameIndex], aData, mSize);
			}

			if (bindFlags & ER_RHI_BIND_FLAG::ER_BIND_CONSTANT_BUFFER)
			{
				mBufferCBVHandle[frameIndex] = descriptorHeapManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, frameIndex);

				D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
				cbvDesc.BufferLocation = mBufferUpload[frameIndex]->GetGPUVirtualAddress();
				cbvDesc.SizeInBytes = mSize;
				device->CreateConstantBufferView(&cbvDesc, mBufferCBVHandle[frameIndex].GetCPUHandle());
			}
		}

		if (aData && !mIsDynamic)
			UpdateSubresource(aRHI, aData, mSize, aRHIDX12->GetCurrentGraphicsCommandListIndex());

		if (bindFlags & ER_BIND_VERTEX_BUFFER)
		{
			// Initialize the vertex buffer view.
			if (!mIsDynamic)
			{
				mVertexBufferViews[0].BufferLocation = mBuffer->GetGPUVirtualAddress();
				mVertexBufferViews[0].StrideInBytes = mStride;
				mVertexBufferViews[0].SizeInBytes = mSize;
			}
			else //this is instance buffer (most likely) or dynamic vertex buffer (less likely)
			{
				for (int frameIndex = 0; frameIndex < DX12_MAX_BACK_BUFFER_COUNT; frameIndex++)
				{
					mVertexBufferViews[frameIndex].BufferLocation = mBufferUpload[frameIndex]->GetGPUVirtualAddress();
					mVertexBufferViews[frameIndex].StrideInBytes = mStride;
					mVertexBufferViews[frameIndex].SizeInBytes = mSize;
				}
			}
		}

		if (bindFlags & ER_BIND_INDEX_BUFFER)
		{
			mIndexBufferView.BufferLocation = mBuffer->GetGPUVirtualAddress();
			mIndexBufferView.Format = mFormat;
			mIndexBufferView.SizeInBytes = mSize;
		}

		if (bindFlags & ER_BIND_SHADER_RESOURCE)
		{
			mBufferSRVHandle = descriptorHeapManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			if (miscFlags & ER_RHI_RESOURCE_MISC_FLAG::ER_RESOURCE_MISC_DRAWINDIRECT_ARGS)
				srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			else
				srvDesc.Format = mFormat;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Buffer.NumElements = objectsCount;
			srvDesc.Buffer.StructureByteStride = byteStride;
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			device->CreateShaderResourceView(mBuffer.Get(), &srvDesc, mBufferSRVHandle.GetCPUHandle());

		}

		if (bindFlags & ER_BIND_UNORDERED_ACCESS)
		{
			mBufferUAVHandle = descriptorHeapManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			if (miscFlags & ER_RHI_RESOURCE_MISC_FLAG::ER_RESOURCE_MISC_DRAWINDIRECT_ARGS)
				uavDesc.Format = DXGI_FORMAT_UNKNOWN;
			else
				uavDesc.Format = mFormat;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uavDesc.Buffer.NumElements = objectsCount;
			uavDesc.Buffer.StructureByteStride = byteStride;
			uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
			device->CreateUnorderedAccessView(mBuffer.Get(), nullptr, &uavDesc, mBufferUAVHandle.GetCPUHandle());
		}

		if (mBuffer)
			mBuffer->SetName(ER_Utility::ToWideString(mDebugName).c_str());

		for (int i = 0; i < DX12_MAX_BACK_BUFFER_COUNT; i++)
		{
			if (mBufferUpload[i])
				mBufferUpload[i]->SetName(ER_Utility::ToWideString(mDebugName + " Upload frame #" + std::to_string(i)).c_str());
		}

	}

	void ER_RHI_DX12_GPUBuffer::Map(ER_RHI* aRHI, void** aOutData)
	{
		assert(aRHI);
		ER_RHI_DX12* aRHIDX12 = static_cast<ER_RHI_DX12*>(aRHI);

		assert(mBufferUpload[ER_RHI_DX12::mBackBufferIndex] || mBuffer);
		HRESULT hr;
		CD3DX12_RANGE range(0, 0);
		if (mBufferUpload[ER_RHI_DX12::mBackBufferIndex])
		{
			if (FAILED(hr = mBufferUpload[ER_RHI_DX12::mBackBufferIndex]->Map(0, &range, aOutData)))
				throw ER_CoreException("ER_RHI_DX12: Failed to map GPU buffer.", hr);
		}
		else
		{
			if (FAILED(hr = mBuffer->Map(0, &range, aOutData)))
				throw ER_CoreException("ER_RHI_DX12: Failed to map GPU buffer.", hr);
		}
	}

	void ER_RHI_DX12_GPUBuffer::Unmap(ER_RHI* aRHI)
	{
		assert(aRHI);
		ER_RHI_DX12* aRHIDX12 = static_cast<ER_RHI_DX12*>(aRHI);
		assert(mBufferUpload[ER_RHI_DX12::mBackBufferIndex] || mBuffer);

		if (mBufferUpload[ER_RHI_DX12::mBackBufferIndex])
			mBufferUpload[ER_RHI_DX12::mBackBufferIndex]->Unmap(0, nullptr);
		else
			mBuffer->Unmap(0, nullptr);
	}

	void ER_RHI_DX12_GPUBuffer::UpdateSubresource(ER_RHI* aRHI, void* aData, int dataSize, int cmdListIndex)
	{
		assert(mSize >= dataSize);
		assert(cmdListIndex != -1);
		assert(aRHI);

		ER_RHI_DX12* aRHIDX12 = static_cast<ER_RHI_DX12*>(aRHI);

		D3D12_SUBRESOURCE_DATA data = {};
		data.pData = aData;
		data.RowPitch = dataSize;
		data.SlicePitch = 0;

		aRHIDX12->TransitionResources({ static_cast<ER_RHI_GPUResource*>(this) }, ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_COPY_DEST, cmdListIndex);
		UpdateSubresources(aRHIDX12->GetGraphicsCommandList(cmdListIndex), mBuffer.Get(), mBufferUpload[ER_RHI_DX12::mBackBufferIndex].Get(), 0, 0, 1, &data);

		if (mBindFlags & ER_BIND_CONSTANT_BUFFER || mBindFlags & ER_BIND_VERTEX_BUFFER)
			aRHIDX12->TransitionResources({ static_cast<ER_RHI_GPUResource*>(this) }, ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, cmdListIndex);
		else if (mBindFlags & ER_BIND_INDEX_BUFFER)
			aRHIDX12->TransitionResources({ static_cast<ER_RHI_GPUResource*>(this) }, ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_INDEX_BUFFER, cmdListIndex);
		else
			aRHIDX12->TransitionResources({ static_cast<ER_RHI_GPUResource*>(this) }, ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_GENERIC_READ, cmdListIndex);
	}

	void ER_RHI_DX12_GPUBuffer::Update(ER_RHI* aRHI, void* aData, int dataSize, bool updateForAllBackBuffers)
	{
		assert(mSize >= dataSize);
		assert(mIsDynamic);
		assert(aRHI);
		ER_RHI_DX12* aRHIDX12 = static_cast<ER_RHI_DX12*>(aRHI);
		ID3D12Device* device = aRHIDX12->GetDevice();

		if (mIsDynamic)
		{
			if (updateForAllBackBuffers)
			{
				for (int i = 0; i < DX12_MAX_BACK_BUFFER_COUNT; i++)
				{
					memcpy(mMappedData[i], aData, dataSize);
				}
			}
			else
				memcpy(mMappedData[ER_RHI_DX12::mBackBufferIndex], aData, dataSize);
		}
		//else
		//	UpdateSubresource(aRHI, aData, dataSize, aRHIDX12->GetCurrentGraphicsCommandListIndex());
	}

}