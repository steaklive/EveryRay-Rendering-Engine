#pragma once
#include "..\Common.h"
#include "..\ER_GPUBuffer.h"
#include "..\ER_GPUTexture.h"

#define ER_RHI_MAX_GRAPHICS_COMMAND_LISTS 4
#define ER_RHI_MAX_COMPUTE_COMMAND_LISTS 2
#define ER_RHI_MAX_BOUND_VERTEX_BUFFERS 2 //we only support 1 vertex buffer + 1 instance buffer

namespace Library
{
	static const int DefaultFrameRate = 60;

	enum ER_GRAPHICS_API
	{
		DX11,
		DX12
	};

	enum ER_RHI_SHADER_TYPE
	{
		ER_VERTEX,
		ER_GEOMETRY,
		ER_TESSELLATION,
		ER_PIXEL,
		ER_COMPUTE
	};

	enum ER_RHI_DEPTH_STENCIL_STATE
	{
		ER_DEPTH_DISABLED,
		ER_DEPTH_READ,
		ER_DEPTH_READ_WRITE,
		ER_DEPTH_STENCIL_READ,
		ER_DEPTH_STENCIL_READ_WRITE
	};

	enum ER_RHI_BLEND_STATE
	{
	};

	enum ER_RHI_SAMPLER_STATE
	{
		ER_BILINEAR_WRAP,
		ER_BILINEAR_CLAMP,
		ER_BILINEAR_BORDER,
		ER_BILINEAR_MIRROR,
		ER_TRILINEAR_WRAP,
		ER_TRILINEAR_CLAMP,
		ER_TRILINEAR_BORDER,
		ER_TRILINEAR_MIRROR,
		ER_ANISOTROPIC_WRAP,
		ER_ANISOTROPIC_CLAMP,
		ER_ANISOTROPIC_BORDER,
		ER_ANISOTROPIC_MIRROR,
		ER_SHADOW_SS /* this is dirty */
	};

	enum ER_RHI_RASTERIZER_STATE
	{
		ER_NO_CULLING,
		ER_BACK_CULLING,
		ER_FRONT_CULLING,
		ER_WIREFRAME,
		ER_SHADOW_RS/* this is dirty */
	};

	enum ER_RHI_RESOURCE_STATE
	{

	};

	enum ER_RHI_FORMAT
	{
		ER_FORMAT_UNKNOWN = 0,
		ER_FORMAT_R32G32B32A32_TYPELESS,
		ER_FORMAT_R32G32B32A32_FLOAT,
		ER_FORMAT_R32G32B32A32_UINT,
		ER_FORMAT_R32G32B32_TYPELESS,
		ER_FORMAT_R32G32B32_FLOAT,
		ER_FORMAT_R32G32B32_UINT,
		ER_FORMAT_R16G16B16A16_TYPELESS,
		ER_FORMAT_R16G16B16A16_FLOAT,
		ER_FORMAT_R16G16B16A16_UNORM,
		ER_FORMAT_R16G16B16A16_UINT,
		ER_FORMAT_R32G32_TYPELESS,
		ER_FORMAT_R32G32_FLOAT,
		ER_FORMAT_R32G32_UINT,
		ER_FORMAT_R10G10B10A2_TYPELESS,
		ER_FORMAT_R10G10B10A2_UNORM,
		ER_FORMAT_R10G10B10A2_UINT,
		ER_FORMAT_R11G11B10_FLOAT,
		ER_FORMAT_R8G8B8A8_TYPELESS,
		ER_FORMAT_R8G8B8A8_UNORM,
		ER_FORMAT_R8G8B8A8_UINT,
		ER_FORMAT_R16G16_TYPELESS,
		ER_FORMAT_R16G16_FLOAT,
		ER_FORMAT_R16G16_UNORM,
		ER_FORMAT_R16G16_UINT,
		ER_FORMAT_R32_TYPELESS,
		ER_FORMAT_D32_FLOAT,
		ER_FORMAT_R32_FLOAT,
		ER_FORMAT_R32_UINT,
		ER_FORMAT_D24_UNORM_S8_UINT,
		ER_FORMAT_R8G8_TYPELESS,
		ER_FORMAT_R8G8_UNORM,
		ER_FORMAT_R8G8_UINT,
		ER_FORMAT_R16_TYPELESS,
		ER_FORMAT_R16_FLOAT,
		ER_FORMAT_R16_UNORM,
		ER_FORMAT_R16_UINT,
		ER_FORMAT_R8_TYPELESS,
		ER_FORMAT_R8_UNORM,
		ER_FORMAT_R8_UINT
	};

	enum ER_RHI_PRIMITIVE_TYPE
	{
		ER_PRIMITIVE_TOPOLOGY_POINTLIST,
		ER_PRIMITIVE_TOPOLOGY_LINELIST,
		ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
		ER_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
		ER_PRIMITIVE_TOPOLOGY_CONTROL_POINT_PATCHLIST
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
		
		virtual void BeginGraphicsCommandList() = 0;
		virtual void EndGraphicsCommandList() = 0;

		virtual void BeginComputeCommandList() = 0;
		virtual void EndComputeCommandList() = 0;

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

		virtual ER_RHI_PRIMITIVE_TYPE GetCurrentTopologyType() = 0;

		virtual void PresentGraphics() = 0;
		virtual void PresentCompute() = 0;

		virtual void SetRenderTargets(const std::vector<ER_GPUTexture*>& aRenderTargets, ER_GPUTexture* aDepthTarget = nullptr, ER_GPUTexture * aUAV = nullptr) = 0;
		virtual void SetDepthTarget(ER_GPUTexture* aDepthTarget) = 0;

		virtual void SetDepthStencilState(ER_RHI_DEPTH_STENCIL_STATE aDS, UINT stencilRef) = 0;
		virtual ER_RHI_DEPTH_STENCIL_STATE GetCurrentDepthStencilState() = 0;

		virtual void SetBlendState(ER_RHI_BLEND_STATE aBS, const float BlendFactor[4], UINT SampleMask) = 0;
		virtual ER_RHI_BLEND_STATE GetCurrentBlendState() = 0;

		virtual void SetRasterizerState(ER_RHI_RASTERIZER_STATE aRS) = 0;
		virtual ER_RHI_RASTERIZER_STATE GetCurrentRasterizerState() = 0;

		virtual void SetViewport(ER_RHI_Viewport* aViewport) = 0;
		virtual ER_RHI_Viewport* GetCurrentViewport() = 0;

		virtual void SetShaderResources(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_GPUTexture*>& aSRVs, UINT startSlot = 0) = 0;
		virtual void SetUnorderedAccessResources(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_GPUTexture*>& aUAVs, UINT startSlot = 0) = 0;
		virtual void SetConstantBuffers(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_GPUBuffer*>& aCBs, UINT startSlot = 0) = 0;
		virtual void SetSamplers(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_RHI_SAMPLER_STATE>& aSamplers, UINT startSlot = 0) = 0;

		virtual void SetIndexBuffer(ER_GPUBuffer* aBuffer, UINT offset = 0) = 0;
		virtual void SetVertexBuffers(const std::vector<ER_GPUBuffer*>& aVertexBuffers) = 0;

		virtual void SetTopologyType(ER_RHI_PRIMITIVE_TYPE aType) = 0;

		//TODO virtual void SetShader(ER_RHI_Shader* aShader) = 0;
		virtual void UnbindResourcesFromShader(ER_RHI_SHADER_TYPE aShaderType) = 0;

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

		int mCurrentGraphicsCommandListIndex = 0;
		int mCurrentComputeCommandListIndex = 0;
	};
}