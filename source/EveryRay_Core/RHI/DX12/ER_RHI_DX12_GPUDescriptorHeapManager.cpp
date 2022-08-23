#include "ER_RHI_DX12_GPUDescriptorHeapManager.h"
#include "..\..\ER_CoreException.h"

namespace EveryRay_Core
{
	ER_RHI_DX12_DescriptorHeap::ER_RHI_DX12_DescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool isReferencedByShader)
		: mHeapType(heapType)
		, mMaxNumDescriptors(numDescriptors)
		, mIsReferencedByShader(isReferencedByShader)
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
		heapDesc.NumDescriptors = mMaxNumDescriptors;
		heapDesc.Type = mHeapType;
		heapDesc.Flags = mIsReferencedByShader ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		heapDesc.NodeMask = 0;

		if (FAILED(device->CreateDescriptorHeap(&heapDesc, &mDescriptorHeap)))
			throw ER_CoreException("ER_RHI_DX12: Could not create descriptor heap");

		mDescriptorHeapCPUStart = mDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

		if (mIsReferencedByShader)
			mDescriptorHeapGPUStart = mDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

		mDescriptorSize = device->GetDescriptorHandleIncrementSize(mHeapType);
	}

	ER_RHI_DX12_DescriptorHeap::~ER_RHI_DX12_DescriptorHeap()
	{
		ReleaseObject(mDescriptorHeap);
	}

	ER_RHI_DX12_CPUDescriptorHeap::ER_RHI_DX12_CPUDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors)
		: ER_RHI_DX12_DescriptorHeap(device, heapType, numDescriptors, false)
	{
		mCurrentDescriptorIndex = 0;
		mActiveHandleCount = 0;
	}

	ER_RHI_DX12_CPUDescriptorHeap::~ER_RHI_DX12_CPUDescriptorHeap()
	{
		mFreeDescriptors.clear();
	}

	ER_RHI_DX12_DescriptorHandle ER_RHI_DX12_CPUDescriptorHeap::GetNewHandle()
	{
		UINT newHandleID = 0;

		if (mCurrentDescriptorIndex < mMaxNumDescriptors)
		{
			newHandleID = mCurrentDescriptorIndex;
			mCurrentDescriptorIndex++;
		}
		else if (mFreeDescriptors.size() > 0)
		{
			newHandleID = mFreeDescriptors.back();
			mFreeDescriptors.pop_back();
		}
		else
			throw ER_CoreException("ER_RHI_DX12: Ran out of dynamic descriptor heap handles, need to increase heap size");

		ER_RHI_DX12_DescriptorHandle newHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = mDescriptorHeapCPUStart;
		cpuHandle.ptr += newHandleID * mDescriptorSize;
		newHandle.SetCPUHandle(cpuHandle);
		newHandle.SetHeapIndex(newHandleID);
		mActiveHandleCount++;

		return newHandle;
	}

	void ER_RHI_DX12_CPUDescriptorHeap::FreeHandle(ER_RHI_DX12_DescriptorHandle& handle)
	{
		mFreeDescriptors.push_back(handle.GetHeapIndex());

		if (mActiveHandleCount == 0)
			throw ER_CoreException("ER_RHI_DX12: Freeing heap handles when there should be none left");

		mActiveHandleCount--;
	}

	ER_RHI_DX12_GPUDescriptorHeap::ER_RHI_DX12_GPUDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors)
		: ER_RHI_DX12_DescriptorHeap(device, heapType, numDescriptors, true)
	{
		mCurrentDescriptorIndex = 0;
	}

	ER_RHI_DX12_DescriptorHandle ER_RHI_DX12_GPUDescriptorHeap::GetHandleBlock(UINT count)
	{
		UINT newHandleID = 0;
		UINT blockEnd = mCurrentDescriptorIndex + count;

		if (blockEnd < mMaxNumDescriptors)
		{
			newHandleID = mCurrentDescriptorIndex;
			mCurrentDescriptorIndex = blockEnd;
		}
		else
			throw ER_CoreException("ER_RHI_DX12: Ran out of GPU descriptor heap handles, need to increase heap size");

		ER_RHI_DX12_DescriptorHandle newHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = mDescriptorHeapCPUStart;
		cpuHandle.ptr += newHandleID * mDescriptorSize;
		newHandle.SetCPUHandle(cpuHandle);

		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = mDescriptorHeapGPUStart;
		gpuHandle.ptr += newHandleID * mDescriptorSize;
		newHandle.SetGPUHandle(gpuHandle);

		newHandle.SetHeapIndex(newHandleID);

		return newHandle;
	}

	void ER_RHI_DX12_GPUDescriptorHeap::Reset()
	{
		mCurrentDescriptorIndex = 0;
	}

	ER_RHI_DX12_GPUDescriptorHeapManager::ER_RHI_DX12_GPUDescriptorHeapManager(ID3D12Device* device)
	{
		ZeroMemory(mCPUDescriptorHeaps, sizeof(mCPUDescriptorHeaps));

		static const int MaxNoofSRVDescriptors = 4 * 4096;

		mCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = new ER_RHI_DX12_CPUDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MaxNoofSRVDescriptors);
		mCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV] = new ER_RHI_DX12_CPUDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 128);
		mCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV] = new ER_RHI_DX12_CPUDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 128);
		mCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = new ER_RHI_DX12_CPUDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 16);
		
		ZeroMemory(mGPUDescriptorHeaps, sizeof(mGPUDescriptorHeaps));
		mGPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = new ER_RHI_DX12_GPUDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MaxNoofSRVDescriptors);
		mGPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = new ER_RHI_DX12_GPUDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 16);
	}

	ER_RHI_DX12_GPUDescriptorHeapManager::~ER_RHI_DX12_GPUDescriptorHeapManager()
	{
		for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++)
		{
			DeleteObject(mCPUDescriptorHeaps[i]);
			if (i < 2)
				DeleteObject(mGPUDescriptorHeaps[i]);
		}
	}

	ER_RHI_DX12_DescriptorHandle ER_RHI_DX12_GPUDescriptorHeapManager::CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE heapType)
	{
		return mCPUDescriptorHeaps[heapType]->GetNewHandle();
	}

	ER_RHI_DX12_DescriptorHandle ER_RHI_DX12_GPUDescriptorHeapManager::CreateGPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT count)
	{
		return mGPUDescriptorHeaps[heapType]->GetHandleBlock(count);
	}
}
