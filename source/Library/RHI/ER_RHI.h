#pragma once
#include "..\Common.h"
#include "..\ER_GPUBuffer.h"
#include "..\ER_GPUTexture.h"

namespace Library
{
	static const int DefaultFrameRate = 60;

	enum ER_GRAPHICS_API
	{
		DX11,
		DX12
	};

	enum ER_RHI_DepthStencilState
	{
		DEPTH_DISABLED,
		DEPTH_READ,
		DEPTH_READ_WRITE,
		DEPTH_STENCIL_READ,
		DEPTH_STENCIL_READ_WRITE
	};

	enum ER_RHI_BlendState
	{
	};

	enum ER_RHI_SamplerState
	{
		BILINEAR_WRAP,
		BILINEAR_CLAMP,
		BILINEAR_BORDER,
		BILINEAR_MIRROR,
		TRILINEAR_WRAP,
		TRILINEAR_CLAMP,
		TRILINEAR_BORDER,
		TRILINEAR_MIRROR,
		ANISOTROPIC,
		SHADOW_SS /* this is dirty */
	};

	enum ER_RHI_RasterizerState
	{
		NO_CULLING,
		BACK_CULLING,
		FRONT_CULLING,
		WIREFRAME,
		SHADOW_RS/* this is dirty */
	};

	struct ER_RHI_Viewport
	{
		float TopLeftX;
		float TopLeftY;
		float Width;
		float Height;
		float MinDepth = 0.0f;
		float MaxDepth = 1.0f;
	};

	class ER_RHI
	{
	public:
		ER_RHI();
		virtual ~ER_RHI();

		virtual bool Initialize(UINT width, UINT height, bool isFullscreen) = 0;

		virtual void ClearMainRenderTarget(float colors[4]) = 0;
		virtual void ClearMainDepthStencilTarget(float depth, UINT stencil = 0) = 0;
		virtual void ClearRenderTarget(ER_GPUTexture* aRenderTarget, float colors[4]) = 0;
		virtual void ClearDepthStencilTarget(ER_GPUTexture* aDepthTarget, float depth, UINT stencil = 0) = 0;
		virtual void ClearUAV(ER_GPUTexture* aRenderTarget, float colors[4]) = 0;
	
		virtual void CopyBuffer(ER_GPUBuffer* aDestBuffer, ER_GPUBuffer* aSrcBuffer) = 0;

		virtual void Draw(UINT VertexCount) = 0;
		virtual void DrawIndexed(UINT IndexCount) = 0;
		virtual void DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation) = 0;
		virtual void DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation) = 0;
		//TODO DrawIndirect

		virtual void Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ) = 0;
		//TODO DispatchIndirect

		virtual void GenerateMips(ER_GPUTexture* aTexture) = 0; // not every API supports that!

		virtual void PresentGraphics() = 0;
		virtual void PresentCompute() = 0;

		virtual void SetRenderTargets(const std::vector<ER_GPUTexture*>& aRenderTargets, ER_GPUTexture* aDepthTarget = nullptr, ER_GPUTexture * aUAV = nullptr) = 0;
		virtual void SetDepthTarget(ER_GPUTexture* aDepthTarget) = 0;

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

		virtual void SetVertexShaderSRVs(const std::vector<ER_GPUTexture*>& aSRVs) = 0;
		virtual void SetPixelShaderSRVs(const std::vector<ER_GPUTexture*>& aSRVs, UINT startSlot = 0) = 0;
		virtual void SetComputeShaderSRVs(const std::vector<ER_GPUTexture*>& aSRVs, UINT startSlot = 0) = 0;
		virtual void SetGeometryShaderSRVs(const std::vector<ER_GPUTexture*>& aSRVs, UINT startSlot = 0) = 0;

		virtual void SetVertexShaderUAVs(const std::vector<ER_GPUTexture*>& aUAVs, UINT startSlot = 0) = 0;
		virtual void SetPixelShaderUAVs(const std::vector<ER_GPUTexture*>& aUAVs, UINT startSlot = 0) = 0;
		virtual void SetComputeShaderUAVs(const std::vector<ER_GPUTexture*>& aUAVs, UINT startSlot = 0) = 0;
		virtual void SetGeometryShaderUAVs(const std::vector<ER_GPUTexture*>& aUAVs, UINT startSlot = 0) = 0;

		virtual void SetVertexShaderSamplers(const std::vector<ER_RHI_SamplerState>& aSamplers, UINT startSlot = 0) = 0;
		virtual void SetPixelShaderSamplers(const std::vector<ER_RHI_SamplerState>& aSamplers, UINT startSlot = 0) = 0;
		virtual void SetComputeShaderSamplers(const std::vector<ER_RHI_SamplerState>& aSamplers, UINT startSlot = 0) = 0;
		virtual void SetGeometryShaderSamplers(const std::vector<ER_RHI_SamplerState>& aSamplers, UINT startSlot = 0) = 0;

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

		virtual void InitImGui() = 0;
		virtual void StartNewImGuiFrame() = 0;
		virtual void ShutdownImGui() = 0;

		virtual void SetWindowHandle(void* handle) { (HWND)mWindowHandle; }

		virtual ER_RHI_Viewport GetViewport() = 0;

		ER_GRAPHICS_API GetAPI() { return mAPI; }

	protected:
		HWND mWindowHandle;

		ER_GRAPHICS_API mAPI;
		bool mIsFullScreen = false;
	};
}