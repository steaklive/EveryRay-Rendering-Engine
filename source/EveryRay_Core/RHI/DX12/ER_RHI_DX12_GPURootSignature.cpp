#include "ER_RHI_DX12_GPURootSignature.h"
#include "..\..\ER_Utility.h"
#include "..\..\ER_CoreException.h"

namespace EveryRay_Core
{
	ER_RHI_DX12_GPURootSignature::ER_RHI_DX12_GPURootSignature(UINT NumRootParams /*= 0*/, UINT NumStaticSamplers /*= 0*/)
		: mIsFinalized(false), mNumParameters(NumRootParams)
	{
		Reset(NumRootParams, NumStaticSamplers);
	}

	ER_RHI_DX12_GPURootSignature::~ER_RHI_DX12_GPURootSignature()
	{
	}

	void ER_RHI_DX12_GPURootSignature::InitStaticSampler(ER_RHI* rhi, UINT regIndex, const ER_RHI_SAMPLER_STATE& sampler, ER_RHI_SHADER_VISIBILITY visibility /*= ER_RHI_SHADER_VISIBILITY_ALL*/)
	{
		ER_RHI_DX12* dx12 = static_cast<ER_RHI_DX12*>(rhi);
		assert(dx12);

		assert(regIndex < mNumInitializedStaticSamplers);
		InitializeStaticSampler(regIndex, dx12->FindSamplerState(sampler), dx12->GetShaderVisibility(visibility));
	}

	void ER_RHI_DX12_GPURootSignature::InitDescriptorTable(ER_RHI* rhi, int index, const std::vector<ER_RHI_DESCRIPTOR_RANGE_TYPE>& ranges, const std::vector<UINT>& registerIndices,
		const std::vector<UINT>& descriptorCounters, ER_RHI_SHADER_VISIBILITY visibility)
	{
		assert(index < static_cast<int>(mNumParameters));

		ER_RHI_DX12* dx12 = static_cast<ER_RHI_DX12*>(rhi);
		assert(dx12);

		int rangesSize = static_cast<int>(ranges.size());
		assert(rangesSize == descriptorCounters.size() && rangesSize == registerIndices.size());

		ER_RHI_DX12_GPURootParameter& rootParam = mRootParameters.get()[index];
		rootParam.InitAsDescriptorTable(rangesSize, dx12->GetShaderVisibility(visibility));
		for (int i = 0; i < rangesSize; i++)
			rootParam.SetTableRange(i, dx12->GetDescriptorRangeType(ranges[i]), registerIndices[i], descriptorCounters[i]);
	}

	void ER_RHI_DX12_GPURootSignature::Reset(UINT NumRootParams, UINT NumStaticSamplers /*= 0*/)
	{
		if (NumRootParams > 0)
			mRootParameters.reset(new ER_RHI_DX12_GPURootParameter[NumRootParams]);
		else
			mRootParameters = nullptr;

		mNumParameters = NumRootParams;

		if (NumStaticSamplers > 0)
			mStaticSamplers.reset(new D3D12_STATIC_SAMPLER_DESC[NumStaticSamplers]);
		else
			mStaticSamplers = nullptr;
		mNumSamplers = NumStaticSamplers;
		mNumInitializedStaticSamplers = 0;
	}

	void ER_RHI_DX12_GPURootSignature::InitializeStaticSampler(UINT Register, const D3D12_SAMPLER_DESC& NonStaticSamplerDesc, D3D12_SHADER_VISIBILITY Visibility /*= D3D12_SHADER_VISIBILITY_ALL*/)
	{
		D3D12_STATIC_SAMPLER_DESC& StaticSamplerDesc = mStaticSamplers[mNumInitializedStaticSamplers++];

		StaticSamplerDesc.Filter = NonStaticSamplerDesc.Filter;
		StaticSamplerDesc.AddressU = NonStaticSamplerDesc.AddressU;
		StaticSamplerDesc.AddressV = NonStaticSamplerDesc.AddressV;
		StaticSamplerDesc.AddressW = NonStaticSamplerDesc.AddressW;
		StaticSamplerDesc.MipLODBias = NonStaticSamplerDesc.MipLODBias;
		StaticSamplerDesc.MaxAnisotropy = NonStaticSamplerDesc.MaxAnisotropy;
		StaticSamplerDesc.ComparisonFunc = NonStaticSamplerDesc.ComparisonFunc;
		StaticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
		StaticSamplerDesc.MinLOD = NonStaticSamplerDesc.MinLOD;
		StaticSamplerDesc.MaxLOD = NonStaticSamplerDesc.MaxLOD;
		StaticSamplerDesc.ShaderRegister = Register;
		StaticSamplerDesc.RegisterSpace = 0;
		StaticSamplerDesc.ShaderVisibility = Visibility;

		if (StaticSamplerDesc.AddressU == D3D12_TEXTURE_ADDRESS_MODE_BORDER ||
			StaticSamplerDesc.AddressV == D3D12_TEXTURE_ADDRESS_MODE_BORDER ||
			StaticSamplerDesc.AddressW == D3D12_TEXTURE_ADDRESS_MODE_BORDER)
		{
			assert(
				// Transparent Black
				NonStaticSamplerDesc.BorderColor[0] == 0.0f &&
				NonStaticSamplerDesc.BorderColor[1] == 0.0f &&
				NonStaticSamplerDesc.BorderColor[2] == 0.0f &&
				NonStaticSamplerDesc.BorderColor[3] == 0.0f ||
				// Opaque Black
				NonStaticSamplerDesc.BorderColor[0] == 0.0f &&
				NonStaticSamplerDesc.BorderColor[1] == 0.0f &&
				NonStaticSamplerDesc.BorderColor[2] == 0.0f &&
				NonStaticSamplerDesc.BorderColor[3] == 1.0f ||
				// Opaque White
				NonStaticSamplerDesc.BorderColor[0] == 1.0f &&
				NonStaticSamplerDesc.BorderColor[1] == 1.0f &&
				NonStaticSamplerDesc.BorderColor[2] == 1.0f &&
				NonStaticSamplerDesc.BorderColor[3] == 1.0f);
			//"Sampler border color does not match static sampler limitations");

			if (NonStaticSamplerDesc.BorderColor[3] == 1.0f)
			{
				if (NonStaticSamplerDesc.BorderColor[0] == 1.0f)
					StaticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
				else
					StaticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
			}
			else
				StaticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		}
	}

	void ER_RHI_DX12_GPURootSignature::Finalize(ID3D12Device* device, const std::string& name, D3D12_ROOT_SIGNATURE_FLAGS Flags /*= D3D12_ROOT_SIGNATURE_FLAG_NONE*/)
	{
		if (mIsFinalized)
			return;

		assert(mNumInitializedStaticSamplers == mNumSamplers);

		D3D12_ROOT_SIGNATURE_DESC RootDesc;
		RootDesc.NumParameters = mNumParameters;
		RootDesc.pParameters = (const D3D12_ROOT_PARAMETER*)mRootParameters.get();
		RootDesc.NumStaticSamplers = mNumSamplers;
		RootDesc.pStaticSamplers = (const D3D12_STATIC_SAMPLER_DESC*)mStaticSamplers.get();
		RootDesc.Flags = Flags;

		mDescriptorTableBitMap = 0;
		mSamplerTableBitMap = 0;

		for (UINT Param = 0; Param < mNumParameters; ++Param)
		{
			const D3D12_ROOT_PARAMETER& RootParam = RootDesc.pParameters[Param];
			mDescriptorTableSize[Param] = 0;

			if (RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
			{
				assert(RootParam.DescriptorTable.pDescriptorRanges != nullptr);

				// We keep track of sampler descriptor tables separately from CBV_SRV_UAV descriptor tables
				if (RootParam.DescriptorTable.pDescriptorRanges->RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
					mSamplerTableBitMap |= (1 << Param);
				else
					mDescriptorTableBitMap |= (1 << Param);

				for (UINT TableRange = 0; TableRange < RootParam.DescriptorTable.NumDescriptorRanges; ++TableRange)
					mDescriptorTableSize[Param] += RootParam.DescriptorTable.pDescriptorRanges[TableRange].NumDescriptors;
			}
		}

		ID3DBlob* pOutBlob = nullptr;
		ID3DBlob* pErrorBlob = nullptr;

		if (FAILED(D3D12SerializeRootSignature(&RootDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &pOutBlob, &pErrorBlob)))
		{
			std::string msg = "ER_RHI_DX12: Could not serialize a root signature: " + name;
			throw ER_CoreException(msg.c_str());
		}

		if (FAILED(device->CreateRootSignature(1, pOutBlob->GetBufferPointer(), pOutBlob->GetBufferSize(), IID_PPV_ARGS(&mSignature))))
		{
			std::string msg = "ER_RHI_DX12: Could not create a root signature: " + name;
			throw ER_CoreException(msg.c_str());
		}

		mSignature->SetName(ER_Utility::ToWideString(name).c_str());
		mIsFinalized = true;

		pOutBlob->Release();
		pErrorBlob->Release();
	}

	void ER_RHI_DX12_GPURootSignature::Finalize(ER_RHI* rhi, const std::string& name)
	{
		assert(rhi);
		Finalize(static_cast<ER_RHI_DX12*>(rhi)->GetDevice(), name);
	}

}

