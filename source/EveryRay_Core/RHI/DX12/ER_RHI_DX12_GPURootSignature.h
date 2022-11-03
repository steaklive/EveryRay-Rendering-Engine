#pragma once
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//

#include "ER_RHI_DX12.h"

namespace EveryRay_Core
{
	class ER_RHI_DX12_GPURootParameter
	{
		friend class ER_RHI_DX12_GPURootSignature;
	public:
		ER_RHI_DX12_GPURootParameter()
		{
			mRootParameter.ParameterType = (D3D12_ROOT_PARAMETER_TYPE)0xFFFFFFFF;
		}

		~ER_RHI_DX12_GPURootParameter()
		{
			Clear();
		}

		void Clear()
		{
			if (mRootParameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
				delete[] mRootParameter.DescriptorTable.pDescriptorRanges;

			mRootParameter.ParameterType = (D3D12_ROOT_PARAMETER_TYPE)0xFFFFFFFF;
		}

		void InitAsConstants(UINT Register, UINT NumDwords, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
		{
			mRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			mRootParameter.ShaderVisibility = Visibility;
			mRootParameter.Constants.Num32BitValues = NumDwords;
			mRootParameter.Constants.ShaderRegister = Register;
			mRootParameter.Constants.RegisterSpace = 0;
		}

		void InitAsConstantBuffer(UINT Register, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
		{
			mRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			mRootParameter.ShaderVisibility = Visibility;
			mRootParameter.Descriptor.ShaderRegister = Register;
			mRootParameter.Descriptor.RegisterSpace = 0;
		}

		void InitAsBufferSRV(UINT Register, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
		{
			mRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
			mRootParameter.ShaderVisibility = Visibility;
			mRootParameter.Descriptor.ShaderRegister = Register;
			mRootParameter.Descriptor.RegisterSpace = 0;
		}

		void InitAsBufferUAV(UINT Register, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
		{
			mRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
			mRootParameter.ShaderVisibility = Visibility;
			mRootParameter.Descriptor.ShaderRegister = Register;
			mRootParameter.Descriptor.RegisterSpace = 0;
		}

		void InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE Type, UINT Register, UINT Count, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
		{
			InitAsDescriptorTable(1, Visibility);
			SetTableRange(0, Type, Register, Count);
		}

		void InitAsDescriptorTable(UINT RangeCount, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
		{
			mRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			mRootParameter.ShaderVisibility = Visibility;
			mRootParameter.DescriptorTable.NumDescriptorRanges = RangeCount;
			mRootParameter.DescriptorTable.pDescriptorRanges = new D3D12_DESCRIPTOR_RANGE[RangeCount];
		}

		void SetTableRange(UINT RangeIndex, D3D12_DESCRIPTOR_RANGE_TYPE Type, UINT Register, UINT Count, UINT Space = 0)
		{
			D3D12_DESCRIPTOR_RANGE* range = const_cast<D3D12_DESCRIPTOR_RANGE*>(mRootParameter.DescriptorTable.pDescriptorRanges + RangeIndex);
			range->RangeType = Type;
			range->NumDescriptors = Count;
			range->BaseShaderRegister = Register;
			range->RegisterSpace = Space;
			range->OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			//if (Type == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
			//	mSRVCount += Count;
			//else if (Type == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
			//	mUAVCount += Count;
			//else if (Type == D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
			//	mCBVCount += Count;
		}

		//void SetDescriptorHandle(ER_RHI_DX12_DescriptorHandle& handle) { mDescriptorHandle = handle; }
		//ER_RHI_DX12_DescriptorHandle& GetDescriptorHandle() const { return mDescriptorHandle; }

		const D3D12_ROOT_PARAMETER& operator() (void) const { return mRootParameter; }

		//UINT mSRVCount = 0;
		//UINT mCBVCount = 0;
		//UINT mUAVCount = 0;
	protected:
		D3D12_ROOT_PARAMETER mRootParameter;
	private:
		//ER_RHI_DX12_DescriptorHandle& mDescriptorHandle;
	};

	// Maximum 64 DWORDS divied up amongst all root parameters.
	// Root constants = 1 DWORD * NumConstants
	// Root descriptor (CBV, SRV, or UAV) = 2 DWORDs each
	// Descriptor table pointer = 1 DWORD
	// Static samplers = 0 DWORDS (compiled into shader)
	class ER_RHI_DX12_GPURootSignature : public ER_RHI_GPURootSignature
	{
	public:
		ER_RHI_DX12_GPURootSignature(UINT NumRootParams = 0, UINT NumStaticSamplers = 0);
		virtual ~ER_RHI_DX12_GPURootSignature();

		virtual void InitConstant(ER_RHI* rhi, UINT index, UINT regIndex, UINT numDWORDs, ER_RHI_SHADER_VISIBILITY visibility = ER_RHI_SHADER_VISIBILITY_ALL) override;
		virtual void InitStaticSampler(ER_RHI* rhi, UINT regIndex, const ER_RHI_SAMPLER_STATE& samplers, ER_RHI_SHADER_VISIBILITY visibility = ER_RHI_SHADER_VISIBILITY_ALL) override;
		virtual void InitDescriptorTable(ER_RHI* rhi, int index, const std::vector<ER_RHI_DESCRIPTOR_RANGE_TYPE>& ranges, const std::vector<UINT>& registerIndices,
			const std::vector<UINT>& descriptorCounters, ER_RHI_SHADER_VISIBILITY visibility = ER_RHI_SHADER_VISIBILITY_ALL) override;
		virtual void Finalize(ER_RHI* rhi, const std::string& name, bool needsInputAssembler = false) override;

		virtual int GetStaticSamplersCount() override { return mNumSamplers; }
		virtual int GetRootParameterCount() override { return mNumParameters; }
		//virtual int GetRootParameterCBVCount(int paramIndex) override { return mRootParameters.get()[paramIndex].mCBVCount; }
		//virtual int GetRootParameterSRVCount(int paramIndex) override { return mRootParameters.get()[paramIndex].mSRVCount; }
		//virtual int GetRootParameterUAVCount(int paramIndex) override { return mRootParameters.get()[paramIndex].mUAVCount; }

		ER_RHI_DX12_GPURootParameter& operator[] (size_t EntryIndex)
		{
			assert(EntryIndex < mNumParameters);
			return mRootParameters.get()[EntryIndex];
		}
		const ER_RHI_DX12_GPURootParameter& operator[] (size_t EntryIndex) const
		{
			assert(EntryIndex < mNumParameters);
			return mRootParameters.get()[EntryIndex];
		}

		void Reset(UINT NumRootParams, UINT NumStaticSamplers = 0);
		void InitializeStaticSampler(UINT Register, const D3D12_SAMPLER_DESC& NonStaticSamplerDesc, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL);
		void Finalize(ID3D12Device* device, const std::string& name, D3D12_ROOT_SIGNATURE_FLAGS Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE);

		ID3D12RootSignature* GetSignature() const { return mSignature.Get(); }
	protected:
		std::unique_ptr<ER_RHI_DX12_GPURootParameter[]> mRootParameters;
		std::unique_ptr<D3D12_STATIC_SAMPLER_DESC[]> mStaticSamplers;
		ComPtr<ID3D12RootSignature> mSignature;

		bool mIsFinalized;
		UINT mNumParameters;
		UINT mNumSamplers;
		UINT mNumInitializedStaticSamplers;
		uint32_t mDescriptorTableBitMap; // One bit is set for root parameters that are non-sampler descriptor tables
		uint32_t mSamplerTableBitMap; // One bit is set for root parameters that are sampler descriptor tables
		uint32_t mDescriptorTableSize[16]; // Non-sampler descriptor tables need to know their descriptor count
	};
}