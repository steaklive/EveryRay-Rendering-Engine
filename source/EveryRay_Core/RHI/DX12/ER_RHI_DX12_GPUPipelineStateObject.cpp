#include "ER_RHI_DX12_GPUPipelineStateObject.h"
#include "ER_RHI_DX12_GPURootSignature.h"
#include "..\..\ER_Utility.h"
#include "..\..\ER_CoreException.h"

namespace EveryRay_Core
{

	ER_RHI_DX12_PSO::~ER_RHI_DX12_PSO()
	{
	}

	ER_RHI_DX12_GraphicsPSO::ER_RHI_DX12_GraphicsPSO(const std::string& aName) : ER_RHI_DX12_PSO(aName)
	{
		mName = aName;

		ZeroMemory(&mPSODesc, sizeof(mPSODesc));
		mPSODesc.NodeMask = 1;
		mPSODesc.SampleMask = 0xFFFFFFFFu;
		mPSODesc.SampleDesc.Count = 1;
		mPSODesc.InputLayout.NumElements = 0;
	}

	void ER_RHI_DX12_GraphicsPSO::SetBlendState(const D3D12_BLEND_DESC& BlendDesc)
	{
		mPSODesc.BlendState = BlendDesc;
	}

	void ER_RHI_DX12_GraphicsPSO::SetRasterizerState(const D3D12_RASTERIZER_DESC& RasterizerDesc)
	{
		mPSODesc.RasterizerState = RasterizerDesc;
	}

	void ER_RHI_DX12_GraphicsPSO::SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& DepthStencilDesc)
	{
		mPSODesc.DepthStencilState = DepthStencilDesc;
	}

	void ER_RHI_DX12_GraphicsPSO::SetSampleMask(UINT SampleMask)
	{
		mPSODesc.SampleMask = SampleMask;
	}

	void ER_RHI_DX12_GraphicsPSO::SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE TopologyType)
	{
		assert(TopologyType != D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED);
		mPSODesc.PrimitiveTopologyType = TopologyType;
	}

	void ER_RHI_DX12_GraphicsPSO::SetPrimitiveRestart(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBProps)
	{
		mPSODesc.IBStripCutValue = IBProps;
	}

	void ER_RHI_DX12_GraphicsPSO::Finalize(ID3D12Device* device)
	{
		mPSODesc.pRootSignature = mRootSignature->GetSignature();
		assert(mPSODesc.pRootSignature != nullptr);

		mPSODesc.InputLayout.pInputElementDescs = mInputLayouts.get();

		HRESULT hr;
		if (FAILED(hr = device->CreateGraphicsPipelineState(&mPSODesc, IID_PPV_ARGS(&mPSO))))
		{
			std::string message = "ER_RHI_DX12: Failed creating graphics PSO: ";
			message += mName;
			throw ER_CoreException(message.c_str());
		}
		else
		{
			std::string message = "[ER_Logger] ER_RHI_DX12: Finished creating graphics PSO: ";
			message += mName;
			message += '\n';
			ER_OUTPUT_LOG(ER_Utility::ToWideString(message).c_str());

			mPSO->SetName(ER_Utility::ToWideString(mName).c_str());
		}
	}

	void ER_RHI_DX12_GraphicsPSO::SetRenderTargetFormats(UINT NumRTVs, const DXGI_FORMAT* RTVFormats, DXGI_FORMAT DSVFormat)
	{
		assert(NumRTVs == 0 || RTVFormats != nullptr);

		for (UINT i = 0; i < NumRTVs; ++i)
		{
			mPSODesc.RTVFormats[i] = RTVFormats[i];
		}
		for (UINT i = NumRTVs; i < mPSODesc.NumRenderTargets; ++i)
		{
			mPSODesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
		}
		mPSODesc.NumRenderTargets = NumRTVs;
		if (DSVFormat != DXGI_FORMAT_UNKNOWN)
			mPSODesc.DSVFormat = DSVFormat;
		mPSODesc.SampleDesc.Count = 1;
		mPSODesc.SampleDesc.Quality = 0;
	}

	void ER_RHI_DX12_GraphicsPSO::SetInputLayout(ER_RHI* aRHI, UINT NumElements, const ER_RHI_INPUT_ELEMENT_DESC* pInputElementDescs)
	{
		ER_RHI_DX12* dx12 = static_cast<ER_RHI_DX12*>(aRHI);
		assert(dx12);

		mPSODesc.InputLayout.NumElements = NumElements;

		if (NumElements > 0)
		{
			D3D12_INPUT_ELEMENT_DESC* NewElements = (D3D12_INPUT_ELEMENT_DESC*)malloc(sizeof(D3D12_INPUT_ELEMENT_DESC) * NumElements);
			for (int i =0; i < static_cast<int>(NumElements); i++)
			{
				NewElements[i].SemanticName = pInputElementDescs[i].SemanticName;
				NewElements[i].SemanticIndex = pInputElementDescs[i].SemanticIndex;
				NewElements[i].Format = dx12->GetFormat(pInputElementDescs[i].Format);
				NewElements[i].InputSlot = pInputElementDescs[i].InputSlot;
				NewElements[i].AlignedByteOffset = pInputElementDescs[i].AlignedByteOffset;
				NewElements[i].InputSlotClass = pInputElementDescs[i].IsPerVertex ? D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA : D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
				NewElements[i].InstanceDataStepRate = pInputElementDescs[i].InstanceDataStepRate;
			}
			mInputLayouts.reset((const D3D12_INPUT_ELEMENT_DESC*)NewElements);
		}
		else
			mInputLayouts = nullptr;
	}

	ER_RHI_DX12_ComputePSO::ER_RHI_DX12_ComputePSO(const std::string& aName) : ER_RHI_DX12_PSO(aName)
	{
		mName = aName;

		ZeroMemory(&mPSODesc, sizeof(mPSODesc));
		mPSODesc.NodeMask = 1;
	}

	void ER_RHI_DX12_ComputePSO::Finalize(ID3D12Device* device)
	{
		mPSODesc.pRootSignature = mRootSignature->GetSignature();
		assert(mPSODesc.pRootSignature != nullptr);
		
		HRESULT hr;
		if (FAILED(hr = device->CreateComputePipelineState(&mPSODesc, IID_PPV_ARGS(&mPSO))))
		{
			std::string message = "ER_RHI_DX12: Failed creating compute PSO: ";
			message += mName;
			throw ER_CoreException(message.c_str());
		}
		else
		{
			std::string message = "[ER_Logger] ER_RHI_DX12: Finished creating compute PSO: ";
			message += mName;
			message += '\n';
			ER_OUTPUT_LOG(ER_Utility::ToWideString(message).c_str());
			
			mPSO->SetName(ER_Utility::ToWideString(mName).c_str());
		}
	}

}