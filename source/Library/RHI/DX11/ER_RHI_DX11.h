#pragma once
#include "..\ER_RHI.h"

#include <d3d11_1.h>
#include <D3DCompiler.h>

#include "imgui_impl_dx11.h"

namespace Library
{
	class ER_RHI_DX11: public ER_RHI
	{
	public:
		ER_RHI_DX11();
		virtual ~ER_RHI_DX11();

		virtual bool Initialize(UINT width, UINT height, bool isFullscreen) override;

		virtual void ClearMainRenderTarget(float colors[4]) override;
		virtual void ClearMainDepthStencilTarget(float depth, UINT stencil = 0) override;
		virtual void ClearRenderTarget(ER_GPUTexture* aRenderTarget, float colors[4]) override;
		virtual void ClearDepthStencilTarget(ER_GPUTexture* aDepthTarget, float depth, UINT stencil = 0) override;
		virtual void ClearUAV(ER_GPUTexture* aRenderTarget, float colors[4]) override;

		virtual void CopyBuffer(ER_GPUBuffer* aDestBuffer, ER_GPUBuffer* aSrcBuffer) override;
		
		virtual void Draw(UINT VertexCount) override;
		virtual void DrawIndexed(UINT IndexCount) override;
		virtual void DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation) override;
		virtual void DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation) override;
		//TODO DrawIndirect

		virtual void Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ) override;
		//TODO DispatchIndirect

		virtual void GenerateMips(ER_GPUTexture* aTexture) override;

		virtual void PresentGraphics() override;
		virtual void PresentCompute() override; //TODO

		virtual void SetRenderTargets(const std::vector<ER_GPUTexture*>& aRenderTargets, ER_GPUTexture* aDepthTarget = nullptr, ER_GPUTexture* aUAV = nullptr) override;
		virtual void SetDepthTarget(ER_GPUTexture* aDepthTarget) override;

		virtual void SetDepthStencilState(ER_RHI_DepthStencilState aDS, UINT stencilRef) override;
		virtual ER_RHI_DepthStencilState GetCurrentDepthStencilState() override;

		virtual void SetBlendState(ER_RHI_BlendState aBS, const float BlendFactor[4], UINT SampleMask) override;
		virtual ER_RHI_BlendState GetCurrentBlendState() override;

		virtual void SetRasterizerState(ER_RHI_RasterizerState aRS) override;
		virtual ER_RHI_RasterizerState GetCurrentRasterizerState() override;

		virtual void SetViewport(ER_RHI_Viewport* aViewport) override;
		virtual ER_RHI_Viewport* GetCurrentViewport() override;

		virtual void SetVertexShaderCBs(const std::vector<ER_GPUBuffer*>& aCBs, UINT startSlot = 0) override;
		virtual void SetPixelShaderCBs(const std::vector<ER_GPUBuffer*>& aCBs, UINT startSlot = 0) override;
		virtual void SetComputeShaderCBs(const std::vector<ER_GPUBuffer*>& aCBs, UINT startSlot = 0) override;
		virtual void SetGeometryShaderCBs(const std::vector<ER_GPUBuffer*>& aCBs, UINT startSlot = 0) override;

		virtual void SetVertexShaderSRVs(const std::vector<ER_GPUTexture*>& aSRVs) override;
		virtual void SetPixelShaderSRVs(const std::vector<ER_GPUTexture*>& aSRVs, UINT startSlot = 0) override;
		virtual void SetComputeShaderSRVs(const std::vector<ER_GPUTexture*>& aSRVs, UINT startSlot = 0) override;
		virtual void SetGeometryShaderSRVs(const std::vector<ER_GPUTexture*>& aSRVs, UINT startSlot = 0) override;

		virtual void SetVertexShaderUAVs(const std::vector<ER_GPUTexture*>& aUAVs, UINT startSlot = 0) override;
		virtual void SetPixelShaderUAVs(const std::vector<ER_GPUTexture*>& aUAVs, UINT startSlot = 0) override;
		virtual void SetComputeShaderUAVs(const std::vector<ER_GPUTexture*>& aUAVs, UINT startSlot = 0) override;
		virtual void SetGeometryShaderUAVs(const std::vector<ER_GPUTexture*>& aUAVs, UINT startSlot  = 0) override;

		virtual void SetVertexShaderSamplers(const std::vector<ER_RHI_SamplerState>& aSamplers, UINT startSlot = 0) override;
		virtual void SetPixelShaderSamplers(const std::vector<ER_RHI_SamplerState>& aSamplers, UINT startSlot = 0) override;
		virtual void SetComputeShaderSamplers(const std::vector<ER_RHI_SamplerState>& aSamplers, UINT startSlot = 0) override;
		virtual void SetGeometryShaderSamplers(const std::vector<ER_RHI_SamplerState>& aSamplers, UINT startSlot = 0) override;

		// TODO
		//virtual void SetVertexShader(ER_RHI_Shader* aShader) override;
		//virtual void SetPixelShader(ER_RHI_Shader* aShader) override;
		//virtual void SetComputeShader(ER_RHI_Shader* aShader) override;
		//virtual void SetGeometryShader(ER_RHI_Shader* aShader) override;
		//virtual void SetTessellationShaders(ER_RHI_Shader* aShader) override;

		virtual void UnbindVertexResources() override;
		virtual void UnbindPixelResources() override;
		virtual void UnbindComputeResources() override;
		//virtual void UnbindGeometryResources() override;
		//virtual void UnbindTessellationResources() override;

		virtual void UpdateBuffer(ER_GPUBuffer* aBuffer, void* aData, int dataSize) override;
		
		virtual void InitImGui() override;
		virtual void StartNewImGuiFrame() override;
		virtual void ShutdownImGui() override;

		virtual ER_RHI_Viewport GetViewport() override { return mMainViewport; }

		ER_GRAPHICS_API GetAPI() { return mAPI; }
	private:
		D3D_FEATURE_LEVEL mFeatureLevel = D3D_FEATURE_LEVEL_11_1;
		ID3D11Device1* mDirect3DDevice = nullptr;
		ID3D11DeviceContext1* mDirect3DDeviceContext = nullptr;
		IDXGISwapChain1* mSwapChain = nullptr;

		ID3D11Texture2D* mDepthStencilBuffer = nullptr;
		D3D11_TEXTURE2D_DESC mBackBufferDesc;
		ID3D11RenderTargetView* mMainRenderTargetView = nullptr;
		ID3D11DepthStencilView* mMainDepthStencilView = nullptr;

		ER_RHI_Viewport mMainViewport;
	};
}