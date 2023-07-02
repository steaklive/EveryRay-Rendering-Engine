#pragma once
#include "..\Common.h"

#define ER_RHI_MAX_GRAPHICS_COMMAND_LISTS 8
#define ER_RHI_MAX_COMPUTE_COMMAND_LISTS 2
#define ER_RHI_MAX_BOUND_VERTEX_BUFFERS 2 //we only support 1 vertex buffer + 1 instance buffer

namespace EveryRay_Core
{
	static const int DefaultFrameRate = 60;
	static inline void AbstractRHIMethodAssert() { assert(("You called an abstract method from ER_RHI", 0)); }

	enum ER_GRAPHICS_API
	{
		DX11,
		DX12
	};

	enum ER_RHI_SHADER_TYPE
	{
		ER_VERTEX,
		ER_GEOMETRY,
		ER_TESSELLATION_HULL,
		ER_TESSELLATION_DOMAIN,
		ER_PIXEL,
		ER_COMPUTE
	};

	enum ER_RHI_DEPTH_STENCIL_STATE
	{
		ER_DISABLED,
		ER_DEPTH_ONLY_READ_COMPARISON_NEVER,
		ER_DEPTH_ONLY_READ_COMPARISON_LESS,
		ER_DEPTH_ONLY_READ_COMPARISON_EQUAL,
		ER_DEPTH_ONLY_READ_COMPARISON_LESS_EQUAL,
		ER_DEPTH_ONLY_READ_COMPARISON_GREATER,
		ER_DEPTH_ONLY_READ_COMPARISON_NOT_EQUAL,
		ER_DEPTH_ONLY_READ_COMPARISON_GREATER_EQUAL,
		ER_DEPTH_ONLY_READ_COMPARISON_ALWAYS,
		ER_DEPTH_ONLY_WRITE_COMPARISON_NEVER,
		ER_DEPTH_ONLY_WRITE_COMPARISON_LESS,
		ER_DEPTH_ONLY_WRITE_COMPARISON_EQUAL,
		ER_DEPTH_ONLY_WRITE_COMPARISON_LESS_EQUAL,
		ER_DEPTH_ONLY_WRITE_COMPARISON_GREATER,
		ER_DEPTH_ONLY_WRITE_COMPARISON_NOT_EQUAL,
		ER_DEPTH_ONLY_WRITE_COMPARISON_GREATER_EQUAL,
		ER_DEPTH_ONLY_WRITE_COMPARISON_ALWAYS
		//TODO: add support for stencil
	};

	enum ER_RHI_BLEND_STATE
	{
		ER_NO_BLEND,
		ER_ALPHA_TO_COVERAGE,
		ER_ALPHA_BLEND
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
		ER_NO_CULLING_NO_DEPTH_SCISSOR_ENABLED,
		ER_SHADOW_RS/* this is dirty */
	};

	enum ER_RHI_RESOURCE_STATE
	{
		ER_RESOURCE_STATE_COMMON,
		ER_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		ER_RESOURCE_STATE_INDEX_BUFFER,
		ER_RESOURCE_STATE_RENDER_TARGET,
		ER_RESOURCE_STATE_UNORDERED_ACCESS,
		ER_RESOURCE_STATE_DEPTH_WRITE,
		ER_RESOURCE_STATE_DEPTH_READ,
		ER_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		ER_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ,
		ER_RESOURCE_STATE_INDIRECT_ARGUMENT,
		ER_RESOURCE_STATE_COPY_DEST,
		ER_RESOURCE_STATE_COPY_SOURCE,
		ER_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
		ER_RESOURCE_STATE_GENERIC_READ,
		ER_RESOURCE_STATE_PRESENT
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
		ER_FORMAT_R8G8B8A8_UNORM_sRGB,
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
		ER_FORMAT_D16_UNORM,
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

	typedef enum ER_RHI_BIND_FLAG
	{
		ER_BIND_NONE = 0x0L,
		ER_BIND_VERTEX_BUFFER = 0x1L,
		ER_BIND_INDEX_BUFFER = 0x2L,
		ER_BIND_CONSTANT_BUFFER = 0x4L,
		ER_BIND_SHADER_RESOURCE = 0x8L,
		ER_BIND_STREAM_OUTPUT = 0x10L,
		ER_BIND_RENDER_TARGET = 0x20L,
		ER_BIND_DEPTH_STENCIL = 0x40L,
		ER_BIND_UNORDERED_ACCESS = 0x80L
	} ER_RHI_BIND_FLAG;

	constexpr inline ER_RHI_BIND_FLAG operator~ (ER_RHI_BIND_FLAG a) { return static_cast<ER_RHI_BIND_FLAG>(~static_cast<std::underlying_type<ER_RHI_BIND_FLAG>::type>(a)); }
	constexpr inline ER_RHI_BIND_FLAG operator| (ER_RHI_BIND_FLAG a, ER_RHI_BIND_FLAG b) { return static_cast<ER_RHI_BIND_FLAG>(static_cast<std::underlying_type<ER_RHI_BIND_FLAG>::type>(a) | static_cast<std::underlying_type<ER_RHI_BIND_FLAG>::type>(b)); }
	constexpr inline ER_RHI_BIND_FLAG operator& (ER_RHI_BIND_FLAG a, ER_RHI_BIND_FLAG b) { return static_cast<ER_RHI_BIND_FLAG>(static_cast<std::underlying_type<ER_RHI_BIND_FLAG>::type>(a) & static_cast<std::underlying_type<ER_RHI_BIND_FLAG>::type>(b)); }
	constexpr inline ER_RHI_BIND_FLAG operator^ (ER_RHI_BIND_FLAG a, ER_RHI_BIND_FLAG b) { return static_cast<ER_RHI_BIND_FLAG>(static_cast<std::underlying_type<ER_RHI_BIND_FLAG>::type>(a) ^ static_cast<std::underlying_type<ER_RHI_BIND_FLAG>::type>(b)); }

	enum ER_RHI_RESOURCE_MISC_FLAG
	{
		ER_RESOURCE_MISC_NONE = 0x0L,
		ER_RESOURCE_MISC_TEXTURECUBE = 0x4L,
		ER_RESOURCE_MISC_DRAWINDIRECT_ARGS = 0x10L,
		ER_RESOURCE_MISC_BUFFER_STRUCTURED = 0x40L,
	};

	enum ER_RHI_DESCRIPTOR_HEAP_TYPE
	{
		ER_RHI_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV = 0,
		ER_RHI_DESCRIPTOR_HEAP_TYPE_SAMPLER = (ER_RHI_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV + 1),
		ER_RHI_DESCRIPTOR_HEAP_TYPE_RTV = (ER_RHI_DESCRIPTOR_HEAP_TYPE_SAMPLER + 1),
		ER_RHI_DESCRIPTOR_HEAP_TYPE_DSV = (ER_RHI_DESCRIPTOR_HEAP_TYPE_RTV + 1)
	};

	enum ER_RHI_SHADER_VISIBILITY
	{
		ER_RHI_SHADER_VISIBILITY_ALL = 0,
		ER_RHI_SHADER_VISIBILITY_VERTEX = 1,
		ER_RHI_SHADER_VISIBILITY_HULL = 2,
		ER_RHI_SHADER_VISIBILITY_DOMAIN = 3,
		ER_RHI_SHADER_VISIBILITY_GEOMETRY = 4,
		ER_RHI_SHADER_VISIBILITY_PIXEL = 5,
		ER_RHI_SHADER_VISIBILITY_AMPLIFICATION = 6,
		ER_RHI_SHADER_VISIBILITY_MESH = 7
	};

	enum ER_RHI_DESCRIPTOR_RANGE_TYPE
	{
		ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV = 0,
		ER_RHI_DESCRIPTOR_RANGE_TYPE_UAV = 1,
		ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV = 2
	};

	struct ER_RHI_INPUT_ELEMENT_DESC
	{
		LPCSTR SemanticName;
		UINT SemanticIndex;
		ER_RHI_FORMAT Format;
		UINT InputSlot;
		UINT AlignedByteOffset;
		bool IsPerVertex = true;
		UINT InstanceDataStepRate = 0;
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

	struct ER_RHI_Rect
	{
		LONG left;
		LONG top;
		LONG right;
		LONG bottom;
	};

	class ER_RHI_InputLayout
	{
	public:
		ER_RHI_InputLayout(ER_RHI_INPUT_ELEMENT_DESC* inputElementDescriptions, UINT inputElementDescriptionCount)
		{
			mInputElementDescriptions = new ER_RHI_INPUT_ELEMENT_DESC[inputElementDescriptionCount];
			memcpy(mInputElementDescriptions, inputElementDescriptions, inputElementDescriptionCount * sizeof(ER_RHI_INPUT_ELEMENT_DESC));
			mInputElementDescriptionCount = inputElementDescriptionCount;
		};
		virtual ~ER_RHI_InputLayout() {
			DeleteObjects(mInputElementDescriptions);
		}

		ER_RHI_INPUT_ELEMENT_DESC* mInputElementDescriptions;
		UINT mInputElementDescriptionCount;
	};
	
	class ER_RHI_GPURootSignature;
	class ER_RHI_GPUResource;
	class ER_RHI_GPUTexture;
	class ER_RHI_GPUBuffer;
	class ER_RHI_GPUShader;

	class ER_RHI
	{
	public:
		ER_RHI() {}
		virtual ~ER_RHI() {}

		virtual bool Initialize(HWND windowHandle, UINT width, UINT height, bool isFullscreen, bool isReset = false) = 0;
		
		virtual void BeginGraphicsCommandList(int index = 0) = 0;
		virtual void EndGraphicsCommandList(int index = 0) = 0;

		virtual void BeginComputeCommandList(int index = 0) = 0;
		virtual void EndComputeCommandList(int index = 0) = 0;

		virtual void BeginCopyCommandList(int index = 0) = 0;
		virtual void EndCopyCommandList(int index = 0) = 0;

		virtual void ClearMainRenderTarget(float colors[4]) = 0;
		virtual void ClearMainDepthStencilTarget(float depth, UINT stencil = 0) = 0;
		virtual void ClearRenderTarget(ER_RHI_GPUTexture* aRenderTarget, float colors[4], int rtvArrayIndex = -1) = 0;
		virtual void ClearDepthStencilTarget(ER_RHI_GPUTexture* aDepthTarget, float depth, UINT stencil = -1) = 0;
		virtual void ClearUAV(ER_RHI_GPUResource* aRenderTarget, float colors[4]) = 0;
		virtual void ClearUAV(ER_RHI_GPUBuffer* aBuffer, UINT clear) = 0;

		virtual ER_RHI_GPUShader* CreateGPUShader() = 0;
		virtual ER_RHI_GPUBuffer* CreateGPUBuffer(const std::string& aDebugName) = 0;
		virtual ER_RHI_GPUTexture* CreateGPUTexture(const std::wstring& aDebugName) = 0;
		virtual ER_RHI_GPURootSignature* CreateRootSignature(UINT NumRootParams = 0, UINT NumStaticSamplers = 0) = 0;
		virtual ER_RHI_InputLayout* CreateInputLayout(ER_RHI_INPUT_ELEMENT_DESC* inputElementDescriptions, UINT inputElementDescriptionCount) = 0;

		virtual void CreateTexture(ER_RHI_GPUTexture* aOutTexture, UINT width, UINT height, UINT samples, ER_RHI_FORMAT format, ER_RHI_BIND_FLAG bindFlags,
			int mip = 1, int depth = -1, int arraySize = 1, bool isCubemap = false, int cubemapArraySize = -1) = 0;
		virtual void CreateTexture(ER_RHI_GPUTexture* aOutTexture, const std::string& aPath, bool isFullPath = false) = 0;
		virtual void CreateTexture(ER_RHI_GPUTexture* aOutTexture, const std::wstring& aPath, bool isFullPath = false) = 0;

		virtual void CreateBuffer(ER_RHI_GPUBuffer* aOutBuffer, void* aData, UINT objectsCount, UINT byteStride, bool isDynamic = false, ER_RHI_BIND_FLAG bindFlags = ER_BIND_NONE, UINT cpuAccessFlags = 0, ER_RHI_RESOURCE_MISC_FLAG miscFlags = ER_RESOURCE_MISC_NONE, ER_RHI_FORMAT format = ER_FORMAT_UNKNOWN) = 0;
		virtual void CopyBuffer(ER_RHI_GPUBuffer* aDestBuffer, ER_RHI_GPUBuffer* aSrcBuffer, int cmdListIndex, bool isInCopyQueue = false) = 0;
		virtual void BeginBufferRead(ER_RHI_GPUBuffer* aBuffer, void** output) = 0;
		virtual void EndBufferRead(ER_RHI_GPUBuffer* aBuffer) = 0;

		virtual void CopyGPUTextureSubresourceRegion(ER_RHI_GPUResource* aDestBuffer, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ER_RHI_GPUResource* aSrcBuffer, UINT SrcSubresource, bool isInCopyQueueOrSkipTransitions = false) = 0;

		virtual void Draw(UINT VertexCount) = 0;
		virtual void DrawIndexed(UINT IndexCount) = 0;
		virtual void DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation) = 0;
		virtual void DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation) = 0;
		virtual void DrawIndexedInstancedIndirect(ER_RHI_GPUBuffer* anArgsBuffer, UINT alignedByteOffset) = 0;

		virtual void Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ) = 0;
		//TODO DispatchIndirect

		virtual void GenerateMips(ER_RHI_GPUTexture* aTexture, ER_RHI_GPUTexture* aSRGBTexture = nullptr) = 0;
		virtual void GenerateMipsWithTextureReplacement(ER_RHI_GPUTexture** aTexture, std::function<void(ER_RHI_GPUTexture**)> aReplacementCallback) = 0;
		virtual void ReplaceOriginalTexturesWithMipped() = 0;

		virtual void ExecuteCommandLists(int commandListIndex = 0, bool isCompute = false) = 0;
		virtual void ExecuteCopyCommandList() = 0;

		virtual void PresentGraphics() = 0;
		virtual void PresentCompute() = 0;

		virtual bool ProjectCubemapToSH(ER_RHI_GPUTexture* aTexture, UINT order, float* resultR, float* resultG, float* resultB) = 0; //WARNING: only works on DX11 for now

		virtual void SaveGPUTextureToFile(ER_RHI_GPUTexture* aTexture, const std::wstring& aPathName) = 0; //WARNING: only works on DX11 for now

		virtual void SetMainRenderTargets(int cmdListIndex = 0) = 0;
		virtual void SetRenderTargets(const std::vector<ER_RHI_GPUTexture*>& aRenderTargets, ER_RHI_GPUTexture* aDepthTarget = nullptr, ER_RHI_GPUTexture* aUAV = nullptr, int rtvArrayIndex = -1) = 0;
		virtual void SetDepthTarget(ER_RHI_GPUTexture* aDepthTarget) = 0;
		virtual void SetRenderTargetFormats(const std::vector<ER_RHI_GPUTexture*>& aRenderTargets, ER_RHI_GPUTexture* aDepthTarget = nullptr) = 0;
		virtual void SetMainRenderTargetFormats() = 0;

		virtual void SetDepthStencilState(ER_RHI_DEPTH_STENCIL_STATE aDS, UINT stencilRef = 0xffffffff) = 0;
		virtual ER_RHI_DEPTH_STENCIL_STATE GetCurrentDepthStencilState() { return mCurrentDS; }

		virtual void SetBlendState(ER_RHI_BLEND_STATE aBS, const float BlendFactor[4] = nullptr, UINT SampleMask = 0xffffffff) = 0;
		virtual ER_RHI_BLEND_STATE GetCurrentBlendState() { return mCurrentBS; }

		virtual void SetRasterizerState(ER_RHI_RASTERIZER_STATE aRS) = 0;
		virtual ER_RHI_RASTERIZER_STATE GetCurrentRasterizerState() { return mCurrentRS; }

		virtual void SetViewport(const ER_RHI_Viewport& aViewport) = 0;
		virtual const ER_RHI_Viewport& GetCurrentViewport() { return mCurrentViewport; }

		virtual void SetRect(const ER_RHI_Rect& rect) = 0;
		virtual const ER_RHI_Rect& GetCurrentRect() { return mCurrentRect; }

		virtual void SetShader(ER_RHI_GPUShader* aShader) = 0;
		
		virtual void SetShaderResources(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_RHI_GPUResource*>& aSRVs, UINT startSlot = 0,
			ER_RHI_GPURootSignature* rs = nullptr, int rootParamIndex = -1, bool isComputeRS = false, bool skipAutomaticTransition = false) = 0;
		virtual void SetUnorderedAccessResources(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_RHI_GPUResource*>& aUAVs, UINT startSlot = 0,
			ER_RHI_GPURootSignature* rs = nullptr, int rootParamIndex = -1, bool isComputeRS = false, bool skipAutomaticTransition = false) = 0;
		virtual void SetConstantBuffers(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_RHI_GPUBuffer*>& aCBs, UINT startSlot = 0,
			ER_RHI_GPURootSignature* rs = nullptr, int rootParamIndex = -1, bool isComputeRS = false) = 0;
		virtual void SetSamplers(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_RHI_SAMPLER_STATE>& aSamplers, UINT startSlot = 0, ER_RHI_GPURootSignature* rs = nullptr) = 0;
		
		virtual void SetRootSignature(ER_RHI_GPURootSignature* rs, bool isCompute = false) = 0;
		virtual void SetRootConstant(UINT aConstant, UINT aRootIndex, UINT anOffset = 0, bool isCompute = false) = 0;

		virtual void SetIndexBuffer(ER_RHI_GPUBuffer* aBuffer, UINT offset = 0) = 0;
		virtual void SetVertexBuffers(const std::vector<ER_RHI_GPUBuffer*>& aVertexBuffers) = 0;
		virtual void SetInputLayout(ER_RHI_InputLayout* aIL) = 0;
		virtual void SetEmptyInputLayout() = 0;

		virtual void SetTopologyType(ER_RHI_PRIMITIVE_TYPE aType) = 0;
		virtual ER_RHI_PRIMITIVE_TYPE GetCurrentTopologyType() = 0;

		virtual void SetGPUDescriptorHeap(ER_RHI_DESCRIPTOR_HEAP_TYPE aType, bool aReset) = 0;
		virtual void SetGPUDescriptorHeapImGui(int cmdListIndex) = 0;

		virtual void TransitionResources(const std::vector<ER_RHI_GPUResource*>& aResources, const std::vector<ER_RHI_RESOURCE_STATE>& aStates, int cmdListIndex = 0, bool isCopyQueue = false, int subresourceIndex = -1) = 0;
		virtual void TransitionResources(const std::vector<ER_RHI_GPUResource*>& aResources, ER_RHI_RESOURCE_STATE aState, int cmdListIndex = 0, bool isCopyQueue = false, int subresourceIndex = -1) = 0;
		virtual void TransitionMainRenderTargetToPresent(int cmdListIndex = 0) = 0;

		virtual bool IsPSOReady(const std::string& aName, bool isCompute = false) = 0;
		virtual void InitializePSO(const std::string& aName, bool isCompute = false) = 0;
		virtual void SetRootSignatureToPSO(const std::string& aName, ER_RHI_GPURootSignature* rs, bool isCompute = false) = 0;
		virtual void SetTopologyTypeToPSO(const std::string& aName, ER_RHI_PRIMITIVE_TYPE aType) = 0;
		virtual void FinalizePSO(const std::string& aName, bool isCompute = false) = 0;
		virtual void SetPSO(const std::string& aName, bool isCompute = false) = 0;
		virtual void UnsetPSO() = 0;

		virtual void UnbindRenderTargets() = 0;
		virtual void UnbindResourcesFromShader(ER_RHI_SHADER_TYPE aShaderType, bool unbindShader = true) = 0;

		virtual void UpdateBuffer(ER_RHI_GPUBuffer* aBuffer, void* aData, int dataSize, bool updateForAllBackBuffers = false) = 0;

		virtual bool IsHardwareRaytracingSupported() = 0;

		virtual void InitImGui() = 0;
		virtual void StartNewImGuiFrame() = 0;
		virtual void RenderDrawDataImGui(int cmdListIndex = 0) = 0;
		virtual void ShutdownImGui() = 0;

		virtual void OnWindowSizeChanged(int width, int height) = 0;
		
		virtual void WaitForGpuOnGraphicsFence() = 0;
		virtual void WaitForGpuOnComputeFence() = 0;
		virtual void WaitForGpuOnCopyFence() = 0;

		virtual void ResetReplacementMippedTexturesPool() = 0;
		virtual void ResetDescriptorManager() = 0;
		virtual void ResetRHI(int width, int height, bool isFullscreen) = 0;

		virtual void BeginEventTag(const std::string& aName, bool isComputeQueue = false) = 0;
		virtual void EndEventTag(bool isComputeQueue = false) = 0;

		inline const int GetPrepareGraphicsCommandListIndex() { return mPrepareGraphicsCommandListIndex; }
		inline const int GetCurrentGraphicsCommandListIndex() { return mCurrentGraphicsCommandListIndex; }
		inline const int GetCurrentComputeCommandListIndex() { return mCurrentComputeCommandListIndex; }

		ER_GRAPHICS_API GetAPI() { return mAPI; }
	protected:
		HWND mWindowHandle;

		ER_GRAPHICS_API mAPI;
		bool mIsFullScreen = false;

		ER_RHI_RASTERIZER_STATE mCurrentRS;
		ER_RHI_BLEND_STATE mCurrentBS;
		ER_RHI_DEPTH_STENCIL_STATE mCurrentDS;

		ER_RHI_Viewport mCurrentViewport;
		ER_RHI_Rect mCurrentRect;

		const int mPrepareGraphicsCommandListIndex = ER_RHI_MAX_GRAPHICS_COMMAND_LISTS - 1; // command list for prepare commands (on init)
		int mCurrentGraphicsCommandListIndex = -1;
		int mCurrentComputeCommandListIndex = -1;
	};

	class ER_RHI_GPURootSignature
	{
	public:
		ER_RHI_GPURootSignature(UINT NumRootParams = 0, UINT NumStaticSamplers = 0) {}
		virtual ~ER_RHI_GPURootSignature() {}

		virtual void InitConstant(ER_RHI* rhi, UINT index, UINT regIndex, UINT numDWORDs, ER_RHI_SHADER_VISIBILITY visibility = ER_RHI_SHADER_VISIBILITY_ALL) { AbstractRHIMethodAssert(); };
		virtual void InitStaticSampler(ER_RHI* rhi, UINT regIndex, const ER_RHI_SAMPLER_STATE& sampler, ER_RHI_SHADER_VISIBILITY visibility = ER_RHI_SHADER_VISIBILITY_ALL) { AbstractRHIMethodAssert();	}
		virtual void InitDescriptorTable(ER_RHI* rhi, int rootParamIndex, const std::vector<ER_RHI_DESCRIPTOR_RANGE_TYPE>& ranges, const std::vector<UINT>& registerIndices,
			const std::vector<UINT>& descriptorCounters, ER_RHI_SHADER_VISIBILITY visibility = ER_RHI_SHADER_VISIBILITY_ALL) {	AbstractRHIMethodAssert(); }
		virtual void Finalize(ER_RHI* rhi, const std::string& name, bool needsInputAssembler = false) { AbstractRHIMethodAssert(); }

		virtual int GetStaticSamplersCount() { AbstractRHIMethodAssert(); return 0; }
		virtual int GetRootParameterCount() { AbstractRHIMethodAssert(); return 0; }
		virtual int GetRootParameterCBVCount(int paramIndex) { AbstractRHIMethodAssert(); return 0; }
		virtual int GetRootParameterSRVCount(int paramIndex) { AbstractRHIMethodAssert(); return 0; }
		virtual int GetRootParameterUAVCount(int paramIndex) { AbstractRHIMethodAssert(); return 0; }
	};

	class ER_RHI_GPUResource
	{
	public:
		ER_RHI_GPUResource() {}
		virtual ~ER_RHI_GPUResource() {}

		virtual void* GetSRV() = 0;
		virtual void* GetUAV() = 0;
		virtual void* GetResource() = 0;

		virtual ER_RHI_RESOURCE_STATE GetCurrentState() = 0;
		virtual void SetCurrentState(ER_RHI_RESOURCE_STATE aState) = 0;

		inline virtual bool IsBuffer() = 0;
	};

	class ER_RHI_GPUTexture : public ER_RHI_GPUResource
	{
	public:
		ER_RHI_GPUTexture() {}
		virtual ~ER_RHI_GPUTexture() {}

		virtual void CreateGPUTextureResource(ER_RHI* aRHI, UINT width, UINT height, UINT samples, ER_RHI_FORMAT format, ER_RHI_BIND_FLAG bindFlags = ER_BIND_NONE,
			int mip = 1, int depth = -1, int arraySize = 1, bool isCubemap = false, int cubemapArraySize = -1) { AbstractRHIMethodAssert();	}
		virtual void CreateGPUTextureResource(ER_RHI* aRHI, const std::string& aPath, bool isFullPath = false, bool is3D = false, bool skipFallback = false, bool* statusFlag = nullptr, bool isSilent = false) { AbstractRHIMethodAssert(); }
		virtual void CreateGPUTextureResource(ER_RHI* aRHI, const std::wstring& aPath, bool isFullPath = false, bool is3D = false, bool skipFallback = false, bool* statusFlag = nullptr, bool isSilent = false) { AbstractRHIMethodAssert(); }

		virtual void* GetRTV(void* aEmpty = nullptr) { AbstractRHIMethodAssert(); return nullptr; }
		virtual void* GetRTV(int index) { AbstractRHIMethodAssert(); return nullptr; }
		virtual void* GetDSV() { AbstractRHIMethodAssert(); return nullptr; }
		virtual void* GetSRV() { AbstractRHIMethodAssert(); return nullptr; }
		virtual void* GetUAV() { AbstractRHIMethodAssert(); return nullptr; }
		virtual void* GetResource() { AbstractRHIMethodAssert(); return nullptr; }

		virtual UINT GetMips() { AbstractRHIMethodAssert(); return 0; }
		virtual UINT GetCalculatedMipCount() { AbstractRHIMethodAssert(); return 0; }
		virtual UINT GetWidth() { AbstractRHIMethodAssert(); return 0; }
		virtual UINT GetHeight() { AbstractRHIMethodAssert(); return 0; }
		virtual UINT GetDepth() { AbstractRHIMethodAssert(); return 0; }

		virtual ER_RHI_RESOURCE_STATE GetCurrentState() { AbstractRHIMethodAssert(); return ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_COMMON; }
		virtual void SetCurrentState(ER_RHI_RESOURCE_STATE aState) { AbstractRHIMethodAssert(); }

		inline virtual bool IsBuffer() { AbstractRHIMethodAssert(); return false; }

		std::wstring debugName;
	};

	class ER_RHI_GPUBuffer : public ER_RHI_GPUResource
	{
	public:
		ER_RHI_GPUBuffer() {}
		virtual ~ER_RHI_GPUBuffer() {}

		virtual void CreateGPUBufferResource(ER_RHI* aRHI, void* aData, UINT objectsCount, UINT byteStride, bool isDynamic = false,
			ER_RHI_BIND_FLAG bindFlags = ER_BIND_NONE, UINT cpuAccessFlags = 0, ER_RHI_RESOURCE_MISC_FLAG miscFlags = ER_RESOURCE_MISC_NONE,
			ER_RHI_FORMAT format = ER_FORMAT_UNKNOWN) {	AbstractRHIMethodAssert(); }
		virtual void* GetBuffer() { AbstractRHIMethodAssert();  return nullptr; }
		virtual int GetSize() { AbstractRHIMethodAssert(); return 0; }
		virtual UINT GetStride() { AbstractRHIMethodAssert(); return 0; }
		virtual ER_RHI_FORMAT GetFormatRhi() { AbstractRHIMethodAssert(); return ER_RHI_FORMAT::ER_FORMAT_UNKNOWN; }

		virtual void* GetSRV() { AbstractRHIMethodAssert(); return nullptr; }
		virtual void* GetUAV() { AbstractRHIMethodAssert(); return nullptr; }
	};

	class ER_RHI_GPUShader
	{
	public:
		ER_RHI_GPUShader() {}
		virtual ~ER_RHI_GPUShader() {}

		virtual void CompileShader(ER_RHI* aRHI, const std::string& path, const std::string& shaderEntry, ER_RHI_SHADER_TYPE type, ER_RHI_InputLayout* aIL = nullptr) { AbstractRHIMethodAssert(); };
		virtual void* GetShaderObject() { AbstractRHIMethodAssert(); return nullptr; }

		ER_RHI_SHADER_TYPE mShaderType;
	};

	template<typename T> 
	class ER_RHI_GPUConstantBuffer
	{
	public:
		T Data;
	protected:
		ER_RHI_GPUBuffer* buffer = nullptr;
		bool initialized;
	public:
		ER_RHI_GPUConstantBuffer() : initialized(false)
		{
			ZeroMemory(&Data, sizeof(T));
		}

		void Release() {
			DeleteObject(buffer);
		}

		ER_RHI_GPUBuffer* Buffer() const
		{
			return buffer;
		}

		void Initialize(ER_RHI* rhi, const std::string& aName)
		{
			buffer = rhi->CreateGPUBuffer(aName);
			buffer->CreateGPUBufferResource(rhi, &Data, 1, ER_BitmaskAlign(static_cast<UINT>(sizeof(T)), ER_GPU_BUFFER_ALIGNMENT), true, ER_BIND_CONSTANT_BUFFER);
			initialized = true;
		}

		void ApplyChanges(ER_RHI* rhi)
		{
			assert(initialized);
			assert(rhi);
			assert(buffer);

			rhi->UpdateBuffer(buffer, &Data, sizeof(T));
		}
	};
}