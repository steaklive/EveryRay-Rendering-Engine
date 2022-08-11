#include "ER_RHI_DX12_GPUPipelineStateObject.h"
#include "..\..\ER_Utility.h"

namespace EveryRay_Core
{

	ER_RHI_DX12_PSO::~ER_RHI_DX12_PSO()
	{
		ReleaseObject(mPSO);
	}

	ER_RHI_DX12_GraphicsPSO::ER_RHI_DX12_GraphicsPSO(const std::string& aName)
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
		mPSODesc.pRootSignature = m_RootSignature->GetSignature();
		assert(mPSODesc.pRootSignature != nullptr);

		mPSODesc.InputLayout.pInputElementDescs = mInputLayouts;

		HRESULT hr;
		if (FAILED(hr = device->CreateGraphicsPipelineState(&mPSODesc, &mPSO)))
		{
			std::string message = "ER_RHI_DX12: Failed creating graphics PSO: ";
			message += mName;
			throw ER_CoreException(message.c_str());
		}
		else
		{
			std::string message = "ER_RHI_DX12: Finished creating graphics PSO: ";
			message += mName;
			message += '\n';
			ER_OUTPUT_LOG(ER_Utility::ToWideString(message).c_str());
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
		mPSODesc.DSVFormat = DSVFormat;
		mPSODesc.SampleDesc.Count = 1;
		mPSODesc.SampleDesc.Quality = 0;
	}

	void ER_RHI_DX12_GraphicsPSO::SetInputLayout(UINT NumElements, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs)
	{
		mPSODesc.InputLayout.NumElements = NumElements;

		if (NumElements > 0)
		{
			D3D12_INPUT_ELEMENT_DESC* NewElements = (D3D12_INPUT_ELEMENT_DESC*)malloc(sizeof(D3D12_INPUT_ELEMENT_DESC) * NumElements);
			memcpy(NewElements, pInputElementDescs, NumElements * sizeof(D3D12_INPUT_ELEMENT_DESC));
			mInputLayouts.reset((const D3D12_INPUT_ELEMENT_DESC*)NewElements);
		}
		else
			mInputLayouts = nullptr;
	}

	ER_RHI_DX12_ComputePSO::ER_RHI_DX12_ComputePSO(const std::string& aName)
	{
		mName = aName;

		ZeroMemory(&mPSODesc, sizeof(m_PSODesc));
		mPSODesc.NodeMask = 1;
	}

	void ER_RHI_DX12_ComputePSO::Finalize(ID3D12Device* device)
	{
		mPSODesc.pRootSignature = m_RootSignature->GetSignature();
		assert(mPSODesc.pRootSignature != nullptr);
		
		HRESULT hr;
		if (FAILED(hr = device->CreateComputePipelineState(&mPSODesc, &mPSO)))
		{
			std::string message = "ER_RHI_DX12: Failed creating compute PSO: ";
			message += mName;
			throw ER_CoreException(message.c_str());
		}
		else
		{
			std::string message = "ER_RHI_DX12: Finished creating compute PSO: ";
			message += mName;
			message += '\n';
			ER_OUTPUT_LOG(ER_Utility::ToWideString(message).c_str());
		}
	}

}