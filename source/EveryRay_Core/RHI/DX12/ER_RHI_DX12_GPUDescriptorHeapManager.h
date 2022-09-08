#pragma once

#include "ER_RHI_DX12.h"

namespace EveryRay_Core
{
	class ER_RHI_DX12_DescriptorHandle
	{
	public:
		ER_RHI_DX12_DescriptorHandle()
		{
			mCPUHandle.ptr = NULL;
			mGPUHandle.ptr = NULL;
			mHeapIndex = 0;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE& GetCPUHandle() { return mCPUHandle; }
		D3D12_GPU_DESCRIPTOR_HANDLE& GetGPUHandle() { return mGPUHandle; }
		UINT GetHeapIndex() { return mHeapIndex; }

		void SetCPUHandle(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) { mCPUHandle = cpuHandle; }
		void SetGPUHandle(D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) { mGPUHandle = gpuHandle; }
		void SetHeapIndex(UINT heapIndex) { mHeapIndex = heapIndex; }

		bool IsValid() { return mCPUHandle.ptr != NULL; }
		bool IsReferencedByShader() { return mGPUHandle.ptr != NULL; }

	private:
		D3D12_CPU_DESCRIPTOR_HANDLE mCPUHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE mGPUHandle;
		UINT mHeapIndex;
	};

	class ER_RHI_DX12_DescriptorHeap
	{
	public:
		ER_RHI_DX12_DescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool isReferencedByShader = false);
		virtual ~ER_RHI_DX12_DescriptorHeap();

		ID3D12DescriptorHeap* GetHeap() { return mDescriptorHeap.Get(); }
		D3D12_DESCRIPTOR_HEAP_TYPE GetHeapType() { return mHeapType; }
		D3D12_CPU_DESCRIPTOR_HANDLE GetHeapCPUStart() { return mDescriptorHeapCPUStart; }
		D3D12_GPU_DESCRIPTOR_HANDLE GetHeapGPUStart() { return mDescriptorHeapGPUStart; }
		UINT GetMaxNoofDescriptors() { return mMaxNumDescriptors; }
		UINT GetDescriptorSize() { return mDescriptorSize; }

		void AddToHandle(ID3D12Device* device, ER_RHI_DX12_DescriptorHandle& destCPUHandle, ER_RHI_DX12_DescriptorHandle& sourceCPUHandle)
		{
			device->CopyDescriptorsSimple(1, destCPUHandle.GetCPUHandle(), sourceCPUHandle.GetCPUHandle(), mHeapType);
			destCPUHandle.GetCPUHandle().ptr += mDescriptorSize;
		}

		void AddToHandle(ID3D12Device* device, ER_RHI_DX12_DescriptorHandle& destCPUHandle, D3D12_CPU_DESCRIPTOR_HANDLE& sourceCPUHandle)
		{
			device->CopyDescriptorsSimple(1, destCPUHandle.GetCPUHandle(), sourceCPUHandle, mHeapType);
			destCPUHandle.GetCPUHandle().ptr += mDescriptorSize;
		}

	protected:
		ComPtr<ID3D12DescriptorHeap> mDescriptorHeap;
		D3D12_DESCRIPTOR_HEAP_TYPE mHeapType;
		D3D12_CPU_DESCRIPTOR_HANDLE mDescriptorHeapCPUStart;
		D3D12_GPU_DESCRIPTOR_HANDLE mDescriptorHeapGPUStart;
		UINT mMaxNumDescriptors;
		UINT mDescriptorSize;
		bool mIsReferencedByShader;
	};

	class ER_RHI_DX12_CPUDescriptorHeap : public ER_RHI_DX12_DescriptorHeap
	{
	public:
		ER_RHI_DX12_CPUDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors);
		~ER_RHI_DX12_CPUDescriptorHeap() final;

		ER_RHI_DX12_DescriptorHandle GetNewHandle();
		void FreeHandle(ER_RHI_DX12_DescriptorHandle& handle);

	private:
		std::vector<UINT> mFreeDescriptors;
		UINT mCurrentDescriptorIndex;
		UINT mActiveHandleCount;
	};

	class ER_RHI_DX12_GPUDescriptorHeap : public ER_RHI_DX12_DescriptorHeap
	{
	public:
		ER_RHI_DX12_GPUDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors);
		~ER_RHI_DX12_GPUDescriptorHeap() final {};

		void Reset();
		ER_RHI_DX12_DescriptorHandle GetHandleBlock(UINT count);

	private:
		UINT mCurrentDescriptorIndex;
	};

	class ER_RHI_DX12_GPUDescriptorHeapManager
	{
	public:
		ER_RHI_DX12_GPUDescriptorHeapManager(ID3D12Device* device);
		~ER_RHI_DX12_GPUDescriptorHeapManager();

		ER_RHI_DX12_DescriptorHandle CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE heapType);
		ER_RHI_DX12_DescriptorHandle CreateGPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT count);

		ER_RHI_DX12_GPUDescriptorHeap* GetGPUHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType)
		{
			return mGPUDescriptorHeaps[heapType];
		}

	private:
		ER_RHI_DX12_CPUDescriptorHeap* mCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
		ER_RHI_DX12_GPUDescriptorHeap* mGPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	};
}

