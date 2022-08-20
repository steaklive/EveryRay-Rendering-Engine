#pragma once

#include "ER_RHI_DX12.h"

namespace EveryRay_Core
{
	class ER_RHI_DX12_PSO
	{
	public:
		ER_RHI_DX12_PSO(const std::string& aName) : mRootSignature(nullptr), mName(aName) {}
		~ER_RHI_DX12_PSO();

		void SetRootSignature(const ER_RHI_DX12_GPURootSignature& rootSignature)
		{
			mRootSignature = &rootSignature;
		}

		const ER_RHI_DX12_GPURootSignature& GetRootSignature(void) const
		{
			assert(mRootSignature != nullptr);
			return *mRootSignature;
		}

		ID3D12PipelineState* GetPipelineStateObject() const { return mPSO; }

	protected:
		const ER_RHI_DX12_GPURootSignature* mRootSignature;
		ID3D12PipelineState* mPSO = nullptr;
		std::string mName;
	};

	class ER_RHI_DX12_GraphicsPSO : public ER_RHI_DX12_PSO
	{
	public:
		ER_RHI_DX12_GraphicsPSO(const std::string& aName);

		void SetBlendState(const D3D12_BLEND_DESC& BlendDesc);
		void SetRasterizerState(const D3D12_RASTERIZER_DESC& RasterizerDesc);
		void SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& DepthStencilDesc);
		void SetSampleMask(UINT SampleMask);
		void SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE TopologyType);
		void SetRenderTargetFormats(UINT NumRTVs, const DXGI_FORMAT* RTVFormats, DXGI_FORMAT DSVFormat = DXGI_FORMAT_UNKNOWN);
		void SetInputLayout(UINT NumElements, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs);
		void SetPrimitiveRestart(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBProps);

		void SetVertexShader(const void* Binary, size_t Size) { mPSODesc.VS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }
		void SetPixelShader(const void* Binary, size_t Size) { mPSODesc.PS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }
		void SetGeometryShader(const void* Binary, size_t Size) { mPSODesc.GS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }
		void SetHullShader(const void* Binary, size_t Size) { mPSODesc.HS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }
		void SetDomainShader(const void* Binary, size_t Size) { mPSODesc.DS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }

		void SetVertexShader(const D3D12_SHADER_BYTECODE& Binary) { mPSODesc.VS = Binary; }
		void SetPixelShader(const D3D12_SHADER_BYTECODE& Binary) { mPSODesc.PS = Binary; }
		void SetGeometryShader(const D3D12_SHADER_BYTECODE& Binary) { mPSODesc.GS = Binary; }
		void SetHullShader(const D3D12_SHADER_BYTECODE& Binary) { mPSODesc.HS = Binary; }
		void SetDomainShader(const D3D12_SHADER_BYTECODE& Binary) { mPSODesc.DS = Binary; }

		void Finalize(ID3D12Device* device);
	private:
		D3D12_GRAPHICS_PIPELINE_STATE_DESC mPSODesc;
		D3D12_INPUT_ELEMENT_DESC* mInputLayouts = nullptr;
	};

	class ER_RHI_DX12_ComputePSO : public ER_RHI_DX12_PSO
	{
	public:
		ER_RHI_DX12_ComputePSO(const std::string& aName);

		void SetComputeShader(const void* Binary, size_t Size) { mPSODesc.CS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }
		void SetComputeShader(const D3D12_SHADER_BYTECODE& Binary) { mPSODesc.CS = Binary; }

		void Finalize(ID3D12Device* device);

	private:
		D3D12_COMPUTE_PIPELINE_STATE_DESC mPSODesc;
	};
}