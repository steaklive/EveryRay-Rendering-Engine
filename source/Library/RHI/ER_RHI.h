#pragma once
#include "Common.h"
#include "ER_GPUBuffer.h"
#include "ER_GPUTexture.h"

namespace Library
{
	enum ER_GRAPHICS_API
	{
		DX11,
		DX12
	};

	enum ER_RHI_DepthStencilState
	{
	};

	enum ER_RHI_BlendState
	{
	};

	enum ER_RHI_RasterizerState
	{
	};

	struct ER_RHI_Viewport
	{

	};

	class ER_RHI
	{
	public:
		ER_RHI();
		~ER_RHI();

		virtual bool Initialize(ER_GRAPHICS_API anAPI) = 0;

		virtual void SetRenderTargets(const std::vector<ER_GPUTexture*>& aRenderTargets, ER_GPUTexture* aDepthTarget = nullptr) = 0;
		virtual void SetDepthTarget(ER_GPUTexture* aDepthTarget) = 0;

		virtual void ClearRenderTarget(ER_GPUTexture* aRenderTarget, float colors[4]) = 0;
		virtual void ClearDepthTarget(ER_GPUTexture* aDepthTarget, float depth, UINT stencil = 0) = 0;
		virtual void ClearUAV(ER_GPUTexture* aRenderTarget, float colors[4]) = 0;
	
		virtual void CopyBuffer(ER_GPUBuffer* aDestBuffer, ER_GPUBuffer* aSrcBuffer) = 0;

		virtual void Draw() = 0;
		virtual void DrawInstanced() = 0;

		virtual void Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ) = 0;

		virtual void Present() = 0;

		virtual void SetDepthStencilState(ER_RHI_DepthStencilState aDS, UINT stencilRef) = 0;
		virtual ER_RHI_DepthStencilState GetCurrentDepthStencilState() = 0;

		virtual void SetBlendState(ER_RHI_BlendState aBS, const float BlendFactor[4], UINT SampleMask) = 0;
		virtual ER_RHI_BlendState GetCurrentBlendState() = 0;

		virtual void SetRasterizerState(ER_RHI_RasterizerState aRS) = 0;
		virtual ER_RHI_RasterizerState GetCurrentRasterizerState() = 0;

		virtual void SetViewport(ER_RHI_Viewport* aViewport) = 0;
		virtual ER_RHI_Viewport* GetCurrentViewport() = 0;

		virtual void SetVertexShaderCBs(const std::vector<ER_GPUBuffer*>& aCBs, UINT startSlot = 0) = 0;
		virtual void SetPixelShaderCBs(const std::vector<ER_GPUBuffer*>& aCBs, UINT startSlot = 0) = 0;
		virtual void SetComputeShaderCBs(const std::vector<ER_GPUBuffer*>& aCBs, UINT startSlot = 0) = 0;
		virtual void SetGeometryShaderCBs(const std::vector<ER_GPUBuffer*>& aCBs, UINT startSlot = 0) = 0;

		virtual void SetVertexShaderSRVs(const std::vector<ER_GPUTexture*>& aSRVs, UINT startSlot = 0) = 0;
		virtual void SetPixelShaderSRVs(const std::vector<ER_GPUTexture*>& aSRVs, UINT startSlot = 0) = 0;
		virtual void SetComputeShaderSRVs(const std::vector<ER_GPUTexture*>& aSRVs, UINT startSlot = 0) = 0;
		virtual void SetGeometryShaderSRVs(const std::vector<ER_GPUTexture*>& aSRVs, UINT startSlot = 0) = 0;

		virtual void SetVertexShaderUAVs(const std::vector<ER_GPUTexture*>& aUAVs, UINT startSlot = 0) = 0;
		virtual void SetPixelShaderUAVs(const std::vector<ER_GPUTexture*>& aUAVs, UINT startSlot = 0) = 0;
		virtual void SetComputeShaderUAVs(const std::vector<ER_GPUTexture*>& aUAVs, UINT startSlot = 0) = 0;
		virtual void SetGeometryShaderUAVs(const std::vector<ER_GPUTexture*>& aUAVs, UINT startSlot = 0) = 0;


		// TODO
		//virtual void SetVertexShaderSamplers(UINT startSlot, const std::vector<ER_RHI_Sampler*>& aSamplers) = 0;
		//virtual void SetPixelShaderSamplers(UINT startSlot, const std::vector<ER_RHI_Sampler*>& aSamplers) = 0;
		//virtual void SetComputeShaderSamplers(UINT startSlot, const std::vector<ER_RHI_Sampler*>& aSamplers) = 0;
		//virtual void SetGeometryShaderSamplers(UINT startSlot, const std::vector<ER_RHI_Sampler*>& aSamplers) = 0;

		// TODO
		//virtual void SetVertexShader(ER_RHI_Shader* aShader) = 0;
		//virtual void SetPixelShader(ER_RHI_Shader* aShader) = 0;
		//virtual void SetComputeShader(ER_RHI_Shader* aShader) = 0;
		//virtual void SetGeometryShader(ER_RHI_Shader* aShader) = 0;
		//virtual void SetTessellationShaders(ER_RHI_Shader* aShader) = 0;

		virtual void UnbindVertexResources() = 0;
		virtual void UnbindPixelResources() = 0;
		virtual void UnbindComputeResources() = 0;
		//virtual void UnbindGeometryResources() = 0;
		//virtual void UnbindTessellationResources() = 0;

		virtual void UpdateBuffer(ER_GPUBuffer* aBuffer, void* aData, int dataSize) = 0;

		ER_GRAPHICS_API GetAPI() { return mAPI; }
	private:
		ER_GRAPHICS_API mAPI;
	};
}