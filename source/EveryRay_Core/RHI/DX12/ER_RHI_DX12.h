#pragma once
#include "..\ER_RHI.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dx12.h>
#include <dxc/dxcapi.h>
#if defined (_DEBUG) || (DEBUG)
#include <dxgidebug.h>
#endif
#include <d3dcompiler.h>

#include "imgui_impl_dx12.h"

namespace EveryRay_Core
{
	class ER_RHI_DX12_InputLayout : public ER_RHI_InputLayout
	{
	public:
		ER_RHI_DX12_InputLayout(ER_RHI_INPUT_ELEMENT_DESC* inputElementDescriptions, UINT inputElementDescriptionCount)
			: ER_RHI_InputLayout(inputElementDescriptions, inputElementDescriptionCount) { }
	};

	enum ER_RHI_DX12_PSO_STATE
	{
		UNSET = 0,
		GRAPHICS,
		COMPUTE
	};

	class ER_RHI_DX12_GraphicsPSO;
	class ER_RHI_DX12_ComputePSO;

	class ER_RHI_DX12: public ER_RHI
	{
	public:
		ER_RHI_DX12();
		virtual ~ER_RHI_DX12();

		virtual bool Initialize(HWND windowHandle, UINT width, UINT height, bool isFullscreen) override;
		
		virtual void BeginGraphicsCommandList() override;
		virtual void EndGraphicsCommandList() override;

		virtual void BeginComputeCommandList() override {}; //TODO
		virtual void EndComputeCommandList() override {}; //TODO

		virtual void ClearMainRenderTarget(float colors[4]) override;
		virtual void ClearMainDepthStencilTarget(float depth, UINT stencil = 0) override;
		virtual void ClearRenderTarget(ER_RHI_GPUTexture* aRenderTarget, float colors[4], int rtvArrayIndex = -1) override;
		virtual void ClearDepthStencilTarget(ER_RHI_GPUTexture* aDepthTarget, float depth, UINT stencil = 0) override;
		virtual void ClearUAV(ER_RHI_GPUResource* aRenderTarget, float colors[4]) override;
		virtual void CreateInputLayout(ER_RHI_InputLayout* aOutInputLayout, ER_RHI_INPUT_ELEMENT_DESC* inputElementDescriptions, UINT inputElementDescriptionCount, const void* shaderBytecodeWithInputSignature, UINT byteCodeLength) override;
		
		virtual ER_RHI_GPUShader* CreateGPUShader() override;
		virtual ER_RHI_GPUBuffer* CreateGPUBuffer() override;
		virtual ER_RHI_GPUTexture* CreateGPUTexture() override;
		virtual ER_RHI_InputLayout* CreateInputLayout(ER_RHI_INPUT_ELEMENT_DESC* inputElementDescriptions, UINT inputElementDescriptionCount) override;

		virtual void CreateTexture(ER_RHI_GPUTexture* aOutTexture, UINT width, UINT height, UINT samples, ER_RHI_FORMAT format, ER_RHI_BIND_FLAG bindFlags = ER_BIND_NONE,
			int mip = 1, int depth = -1, int arraySize = 1, bool isCubemap = false, int cubemapArraySize = -1) override;
		virtual void CreateTexture(ER_RHI_GPUTexture* aOutTexture, const std::string& aPath, bool isFullPath = false) override;
		virtual void CreateTexture(ER_RHI_GPUTexture* aOutTexture, const std::wstring& aPath, bool isFullPath = false) override;

		virtual void CreateBuffer(ER_RHI_GPUBuffer* aOutBuffer, void* aData, UINT objectsCount, UINT byteStride, bool isDynamic = false, ER_RHI_BIND_FLAG bindFlags = ER_BIND_NONE, UINT cpuAccessFlags = 0, ER_RHI_RESOURCE_MISC_FLAG miscFlags = ER_RESOURCE_MISC_NONE, ER_RHI_FORMAT format = ER_FORMAT_UNKNOWN) override;
		virtual void CopyBuffer(ER_RHI_GPUBuffer* aDestBuffer, ER_RHI_GPUBuffer* aSrcBuffer) override;
		virtual void BeginBufferRead(ER_RHI_GPUBuffer* aBuffer, void** output) override;
		virtual void EndBufferRead(ER_RHI_GPUBuffer* aBuffer) override;

		virtual void CopyGPUTextureSubresourceRegion(ER_RHI_GPUResource* aDestBuffer, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ER_RHI_GPUResource* aSrcBuffer, UINT SrcSubresource) override;

		virtual void Draw(UINT VertexCount) override;
		virtual void DrawIndexed(UINT IndexCount) override;
		virtual void DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation) override;
		virtual void DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation) override;
		//TODO DrawIndirect

		virtual void Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ) override;
		//TODO DispatchIndirect

		virtual void GenerateMips(ER_RHI_GPUTexture* aTexture) override;

		virtual void PresentGraphics() override;
		virtual void PresentCompute() override;
		
		virtual bool ProjectCubemapToSH(ER_RHI_GPUTexture* aTexture, UINT order, float* resultR, float* resultG, float* resultB) override;
		
		virtual void SaveGPUTextureToFile(ER_RHI_GPUTexture* aTexture, const std::wstring& aPathName) override;

		virtual void SetMainRenderTargets() override;
		virtual void SetRenderTargets(const std::vector<ER_RHI_GPUTexture*>& aRenderTargets, ER_RHI_GPUTexture* aDepthTarget = nullptr, ER_RHI_GPUTexture* aUAV = nullptr, int rtvArrayIndex = -1) override;
		virtual void SetDepthTarget(ER_RHI_GPUTexture* aDepthTarget) override;
		virtual void SetRenderTargetFormats(const std::vector<ER_RHI_GPUTexture*>& aRenderTargets, ER_RHI_GPUTexture* aDepthTarget = nullptr) override;
		virtual void SetMainRenderTargetFormats() override;

		virtual void SetDepthStencilState(ER_RHI_DEPTH_STENCIL_STATE aDS, UINT stencilRef = 0xffffffff) override;
		//virtual ER_RHI_DEPTH_STENCIL_STATE GetCurrentDepthStencilState() override; //TODO

		virtual void SetBlendState(ER_RHI_BLEND_STATE aBS, const float BlendFactor[4] = nullptr, UINT SampleMask = 0xffffffff) override;
		
		virtual void SetRasterizerState(ER_RHI_RASTERIZER_STATE aRS) override;
		
		virtual void SetViewport(const ER_RHI_Viewport& aViewport) override;
		
		virtual void SetRect(const ER_RHI_Rect& rect) override;
		virtual void SetShaderResources(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_RHI_GPUResource*>& aSRVs, UINT startSlot = 0) override;
		virtual void SetUnorderedAccessResources(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_RHI_GPUResource*>& aUAVs, UINT startSlot = 0) override;
		virtual void SetConstantBuffers(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_RHI_GPUBuffer*>& aCBs, UINT startSlot = 0) override;
		virtual void SetShader(ER_RHI_GPUShader* aShader) override;
		virtual void SetSamplers(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_RHI_SAMPLER_STATE>& aSamplers, UINT startSlot = 0) override;
		virtual void SetInputLayout(ER_RHI_InputLayout* aIL) override;
		virtual void SetEmptyInputLayout() override;
		virtual void SetIndexBuffer(ER_RHI_GPUBuffer* aBuffer, UINT offset = 0) override;
		virtual void SetVertexBuffers(const std::vector<ER_RHI_GPUBuffer*>& aVertexBuffers) override;

		virtual void SetTopologyType(ER_RHI_PRIMITIVE_TYPE aType) override;
		virtual ER_RHI_PRIMITIVE_TYPE GetCurrentTopologyType() override;

		virtual bool IsPSOReady(const std::string& aName, bool isCompute = false) override;
		virtual void InitializePSO(const std::string& aName, bool isCompute = false) override;
		virtual void FinalizePSO(const std::string& aName, bool isCompute = false) override;
		virtual void SetPSO(const std::string& aName, bool isCompute = false) override;
		virtual void UnsetPSO() override;

		virtual void UnbindRenderTargets() override;
		virtual void UnbindResourcesFromShader(ER_RHI_SHADER_TYPE aShaderType, bool unbindShader = true) override {}; //Not needed on DX12

		virtual void UpdateBuffer(ER_RHI_GPUBuffer* aBuffer, void* aData, int dataSize) override;
		
		virtual void InitImGui() override;
		virtual void StartNewImGuiFrame() override;
		virtual void RenderDrawDataImGui() override;
		virtual void ShutdownImGui() override;

		ID3D12Device* GetDevice() const { return mDevice; }
		ID3D12Device5* GetDeviceRaytracing() const { return (ID3D12Device5*)mDevice; }

		DXGI_FORMAT GetFormat(ER_RHI_FORMAT aFormat);

		ER_GRAPHICS_API GetAPI() { return mAPI; }
	protected:
		//virtual void ExecuteCommandLists() override;
	private:
		D3D12_PRIMITIVE_TOPOLOGY GetTopologyType(ER_RHI_PRIMITIVE_TYPE aType);
		ER_RHI_PRIMITIVE_TYPE GetTopologyType(D3D11_PRIMITIVE_TOPOLOGY aType);

		void CreateSamplerStates();
		void CreateBlendStates();
		void CreateRasterizerStates();
		void CreateDepthStencilStates();

		D3D_FEATURE_LEVEL mFeatureLevel = D3D_FEATURE_LEVEL_12_1;
		
		IDXGIFactory4* mDXGIFactory = nullptr;
		DWORD mDXGIFactoryFlags;
		IDXGISwapChain3* mSwapChain = nullptr;
		ID3D12Device* mDevice = nullptr;

		DXGI_FORMAT mMainRTBufferFormat;
		DXGI_FORMAT mMainDepthBufferFormat;
		ID3D12Resource* mMainRenderTarget = nullptr;
		ID3D12Resource* mMainDepthStencil = nullptr;

		ID3D12CommandQueue* mCommandQueueGraphics = nullptr;
		ID3D12GraphicsCommandList* mCommandListGraphics[2] = { nullptr, nullptr };
		ID3D12CommandAllocator* mCommandAllocatorsGraphics = nullptr;
		ID3D12CommandQueue* mCommandQueueCompute = nullptr;
		ID3D12GraphicsCommandList* mCommandListCompute = nullptr;
		ID3D12CommandAllocator* mCommandAllocatorsCompute = nullptr;

		ID3D12Fence* mFenceGraphics = nullptr;
		UINT64 mFenceValuesGraphics;
		Wrappers::Event mFenceEventGraphics;

		ID3D12Fence* mFenceCompute = nullptr;
		UINT64 mFenceValuesCompute;
		Wrappers::Event mFenceEventCompute;

		std::map<ER_RHI_SAMPLER_STATE, D3D12_SAMPLER_DESC> mSamplerStates;
		std::map<ER_RHI_BLEND_STATE, D3D12_BLEND_DESC> mBlendStates;
		std::map<ER_RHI_RASTERIZER_STATE, D3D12_RASTERIZER_DESC> mRasterizerStates;
		std::map<ER_RHI_DEPTH_STENCIL_STATE, D3D12_DEPTH_STENCIL_DESC> mDepthStates;

		std::map<std::string, ER_RHI_DX12_GraphicsPSO> mGraphicsPSOs;
		std::map<std::string, ER_RHI_DX12_ComputePSO> mComputePSOs;
		std::string mCurrentGraphicsPSO;
		std::string mCurrentComputePSO;
		ER_RHI_DX12_PSO_STATE mCurrentPSOState = ER_RHI_DX12_PSO_STATE::UNSET;

		ER_RHI_Viewport mMainViewport;

		bool mIsRaytracingTierAvailable = false;
		bool mIsContextReadingBuffer = false;
	};
}