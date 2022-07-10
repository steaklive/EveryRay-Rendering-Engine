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
		
		virtual void BeginGraphicsCommandList() override {}; //not supported on DX11
		virtual void EndGraphicsCommandList() override {}; //not supported on DX11

		virtual void BeginComputeCommandList() override {}; //not supported on DX11
		virtual void EndComputeCommandList() override {}; //not supported on DX11

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

		virtual void SetDepthStencilState(ER_RHI_DEPTH_STENCIL_STATE aDS, UINT stencilRef) override;
		virtual ER_RHI_DEPTH_STENCIL_STATE GetCurrentDepthStencilState() override;

		virtual void SetBlendState(ER_RHI_BLEND_STATE aBS, const float BlendFactor[4], UINT SampleMask) override;
		virtual ER_RHI_BLEND_STATE GetCurrentBlendState() override; //TODO

		virtual void SetRasterizerState(ER_RHI_RASTERIZER_STATE aRS) override;
		virtual ER_RHI_RASTERIZER_STATE GetCurrentRasterizerState() override;

		virtual void SetViewport(ER_RHI_Viewport* aViewport) override;
		virtual ER_RHI_Viewport* GetCurrentViewport() override;

		virtual void SetShaderResources(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_GPUTexture*>& aSRVs, UINT startSlot = 0) override;
		virtual void SetUnorderedAccessResources(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_GPUTexture*>& aUAVs, UINT startSlot = 0) override;
		virtual void SetConstantBuffers(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_GPUBuffer*>& aCBs, UINT startSlot = 0) override;

		//TODO virtual void SetShader(ER_RHI_Shader* aShader) = 0;
		virtual void SetSamplers(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_RHI_SAMPLER_STATE>& aSamplers, UINT startSlot = 0) override;
		
		virtual void SetIndexBuffer(ER_GPUBuffer* aBuffer, UINT offset = 0) override;
		virtual void SetVertexBuffers(const std::vector<ER_GPUBuffer*>& aVertexBuffers) override;
		
		virtual void SetTopologyType(ER_RHI_PRIMITIVE_TYPE aType) override;
		virtual ER_RHI_PRIMITIVE_TYPE GetCurrentTopologyType() override;

		virtual void UnbindResourcesFromShader(ER_RHI_SHADER_TYPE aShaderType) override;

		virtual void UpdateBuffer(ER_GPUBuffer* aBuffer, void* aData, int dataSize) override;
		
		virtual void InitImGui() override;
		virtual void StartNewImGuiFrame() override;
		virtual void ShutdownImGui() override;

		virtual ER_RHI_Viewport GetViewport() override { return mMainViewport; }

		ER_GRAPHICS_API GetAPI() { return mAPI; }
	protected:
		virtual void ExecuteCommandLists() = 0;
	private:
		DXGI_FORMAT GetFormat(ER_RHI_FORMAT aFormat);
		D3D11_PRIMITIVE_TOPOLOGY GetTopologyType(ER_RHI_PRIMITIVE_TYPE aType);
		ER_RHI_PRIMITIVE_TYPE GetTopologyType(D3D11_PRIMITIVE_TOPOLOGY aType);

		void CreateSamplerStates();
		void CreateBlendStates();
		void CreateRasterizerStates();
		void CreateDepthStencilStates();

		D3D_FEATURE_LEVEL mFeatureLevel = D3D_FEATURE_LEVEL_11_1;
		ID3D11Device1* mDirect3DDevice = nullptr;
		ID3D11DeviceContext1* mDirect3DDeviceContext = nullptr;
		IDXGISwapChain1* mSwapChain = nullptr;

		ID3D11Texture2D* mDepthStencilBuffer = nullptr;
		D3D11_TEXTURE2D_DESC mBackBufferDesc;
		ID3D11RenderTargetView* mMainRenderTargetView = nullptr;
		ID3D11DepthStencilView* mMainDepthStencilView = nullptr;

		ID3D11SamplerState* BilinearWrapSS = nullptr;
		ID3D11SamplerState* BilinearMirrorSS = nullptr;
		ID3D11SamplerState* BilinearClampSS = nullptr;
		ID3D11SamplerState* BilinearBorderSS = nullptr;
		ID3D11SamplerState* TrilinearWrapSS = nullptr;
		ID3D11SamplerState* TrilinearMirrorSS = nullptr;
		ID3D11SamplerState* TrilinearClampSS = nullptr;
		ID3D11SamplerState* TrilinearBorderSS = nullptr;
		ID3D11SamplerState* AnisotropicWrapSS = nullptr;
		ID3D11SamplerState* AnisotropicMirrorSS = nullptr;
		ID3D11SamplerState* AnisotropicClampSS = nullptr;
		ID3D11SamplerState* AnisotropicBorderSS = nullptr;
		ID3D11SamplerState* ShadowSS = nullptr;
		std::map<ER_RHI_SAMPLER_STATE, ID3D11SamplerState*> mSamplerStates;

		ID3D11BlendState* mNoBlendState = nullptr;
		ID3D11BlendState* mAlphaToCoverageState = nullptr;
		std::map<ER_RHI_BLEND_STATE, ID3D11BlendState*> mBlendStates;

		ER_RHI_Viewport mMainViewport;
	};
}