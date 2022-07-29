#pragma once
#include "..\ER_RHI.h"

#include <d3d11_1.h>
#include <D3DCompiler.h>

#include "imgui_impl_dx11.h"

namespace Library
{
	class ER_RHI_DX11_InputLayout : public ER_RHI_InputLayout
	{
	public:
		ER_RHI_DX11_InputLayout();
		virtual ~ER_RHI_DX11_InputLayout() {
			ReleaseObject(mInputLayout);
		};
		ID3D11InputLayout* mInputLayout = nullptr;
	};

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
		virtual void ClearRenderTarget(ER_RHI_GPUTexture* aRenderTarget, float colors[4]) override;
		virtual void ClearDepthStencilTarget(ER_RHI_GPUTexture* aDepthTarget, float depth, UINT stencil = 0) override;
		virtual void ClearUAV(ER_RHI_GPUResource* aRenderTarget, float colors[4]) override;
		virtual void CreateInputLayout(ER_RHI_InputLayout* aOutInputLayout, ER_RHI_INPUT_ELEMENT_DESC* inputElementDescriptions, UINT inputElementDescriptionCount, const void* shaderBytecodeWithInputSignature, UINT byteCodeLength) = 0;
		
		virtual void CreateTexture(ER_RHI_GPUTexture* aOutTexture, UINT width, UINT height, UINT samples, ER_RHI_FORMAT format, ER_RHI_BIND_FLAG bindFlags = ER_BIND_NONE,
			int mip = 1, int depth = -1, int arraySize = 1, bool isCubemap = false, int cubemapArraySize = -1) = 0;
		virtual void CreateTexture(ER_RHI_GPUTexture* aOutTexture, const std::string& aPath, bool isFullPath = false) override;
		virtual void CreateTexture(ER_RHI_GPUTexture* aOutTexture, const std::wstring& aPath, bool isFullPath = false) override;

		virtual void CreateBuffer(ER_RHI_GPUBuffer* aOutBuffer, void* aData, UINT objectsCount, UINT byteStride, bool isDynamic = false, ER_RHI_BIND_FLAG bindFlags = ER_BIND_NONE, UINT cpuAccessFlags = 0, ER_RHI_RESOURCE_MISC_FLAG miscFlags = ER_RESOURCE_MISC_NONE, ER_RHI_FORMAT format = ER_FORMAT_UNKNOWN) override;
		virtual void CopyBuffer(ER_RHI_GPUBuffer* aDestBuffer, ER_RHI_GPUBuffer* aSrcBuffer) override;
		virtual void BeginBufferRead(ER_RHI_GPUBuffer* aBuffer, void* output) override;
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
		virtual void PresentCompute() override; //not supported on DX11

		virtual void SetRenderTargets(const std::vector<ER_RHI_GPUTexture*>& aRenderTargets, ER_RHI_GPUTexture* aDepthTarget = nullptr, ER_RHI_GPUTexture* aUAV = nullptr) override;
		virtual void SetDepthTarget(ER_RHI_GPUTexture* aDepthTarget) override;

		virtual void SetDepthStencilState(ER_RHI_DEPTH_STENCIL_STATE aDS, UINT stencilRef = 0xffffffff) override;
		virtual ER_RHI_DEPTH_STENCIL_STATE GetCurrentDepthStencilState() override; //TODO

		virtual void SetBlendState(ER_RHI_BLEND_STATE aBS, const float BlendFactor[4], UINT SampleMask) override;

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

		virtual void UnbindRenderTargets() override;
		virtual void UnbindResourcesFromShader(ER_RHI_SHADER_TYPE aShaderType, bool unbindShader = true) override;

		virtual void UpdateBuffer(ER_RHI_GPUBuffer* aBuffer, void* aData, int dataSize) override;
		
		virtual void InitImGui() override;
		virtual void StartNewImGuiFrame() override;
		virtual void RenderDrawDataImGui() override;
		virtual void ShutdownImGui() override;

		ID3D11Device1* GetDevice() { return mDirect3DDevice; }
		ID3D11DeviceContext1* GetContext() { return mDirect3DDeviceContext; }
		DXGI_FORMAT GetFormat(ER_RHI_FORMAT aFormat);

		ER_GRAPHICS_API GetAPI() { return mAPI; }
	protected:
		virtual void ExecuteCommandLists() = 0;
	private:
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

		ID3D11RasterizerState* BackCullingRS = nullptr;
		ID3D11RasterizerState* FrontCullingRS = nullptr;
		ID3D11RasterizerState* NoCullingRS = nullptr;
		ID3D11RasterizerState* WireframeRS = nullptr;
		ID3D11RasterizerState* NoCullingNoDepthEnabledScissorRS = nullptr;
		ID3D11RasterizerState* ShadowRS = nullptr;
		std::map<ER_RHI_RASTERIZER_STATE, ID3D11RasterizerState*> mRasterizerStates;

		ID3D11DepthStencilState* DisabledDS = nullptr;
		ID3D11DepthStencilState* DepthOnlyReadComparisonNeverDS = nullptr;
		ID3D11DepthStencilState* DepthOnlyReadComparisonLessDS = nullptr;
		ID3D11DepthStencilState* DepthOnlyReadComparisonEqualDS = nullptr;
		ID3D11DepthStencilState* DepthOnlyReadComparisonLessEqualDS = nullptr;
		ID3D11DepthStencilState* DepthOnlyReadComparisonGreaterDS = nullptr;
		ID3D11DepthStencilState* DepthOnlyReadComparisonNotEqualDS = nullptr;
		ID3D11DepthStencilState* DepthOnlyReadComparisonGreaterEqualDS = nullptr;
		ID3D11DepthStencilState* DepthOnlyReadComparisonAlwaysDS = nullptr;
		ID3D11DepthStencilState* DepthOnlyWriteComparisonNeverDS = nullptr;
		ID3D11DepthStencilState* DepthOnlyWriteComparisonLessDS = nullptr;
		ID3D11DepthStencilState* DepthOnlyWriteComparisonEqualDS = nullptr;
		ID3D11DepthStencilState* DepthOnlyWriteComparisonLessEqualDS = nullptr;
		ID3D11DepthStencilState* DepthOnlyWriteComparisonGreaterDS = nullptr;
		ID3D11DepthStencilState* DepthOnlyWriteComparisonNotEqualDS = nullptr;
		ID3D11DepthStencilState* DepthOnlyWriteComparisonGreaterEqualDS = nullptr;
		ID3D11DepthStencilState* DepthOnlyWriteComparisonAlwaysDS = nullptr;
		std::map<ER_RHI_DEPTH_STENCIL_STATE, ID3D11DepthStencilState*> mDepthStates;

		ER_RHI_Viewport mMainViewport;

		bool mIsContextReadingBuffer = false;
	};
}