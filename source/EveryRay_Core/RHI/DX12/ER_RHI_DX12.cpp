#include "ER_RHI_DX12.h"
#include "ER_RHI_DX12_GPUBuffer.h"
#include "ER_RHI_DX12_GPUTexture.h"
#include "ER_RHI_DX12_GPUShader.h"
#include "ER_RHI_DX12_GPUPipelineStateObject.h"
#include "ER_RHI_DX12_GPURootSignature.h"
#include "ER_RHI_DX12_GPUDescriptorHeapManager.h"

#include "..\..\ER_CoreException.h"
#include "..\..\ER_Utility.h"

namespace EveryRay_Core
{
	static ER_RHI_DX12_DescriptorHandle sNullSRV2DHandle;
	static ER_RHI_DX12_DescriptorHandle sNullSRV3DHandle;
	int ER_RHI_DX12::mBackBufferIndex = 0;

	ER_RHI_DX12::ER_RHI_DX12()
	{
	}

	ER_RHI_DX12::~ER_RHI_DX12()
	{
		WaitForGpuOnGraphicsFence();
		DeleteObject(mGenerateMips2DCS);
		DeleteObject(mGenerateMips2DRS);
		DeleteObject(mGenerateMips3DCS);
		DeleteObject(mGenerateMips3DRS);
		DeleteObject(mClearUAV2DCS);
		DeleteObject(mClearUAV2DRS);
		DeleteObject(mClearUAV3DCS);
		DeleteObject(mClearUAV3DRS);

		ResetReplacementMippedTexturesPool();

		DeleteObject(mDescriptorHeapManager);
	}

	bool ER_RHI_DX12::Initialize(HWND windowHandle, UINT width, UINT height, bool isFullscreen, bool isReset)
	{
		mWindowHandle = windowHandle;
		mAPI = ER_GRAPHICS_API::DX12;
		assert(width > 0 && height > 0);
		HRESULT hr;

#if defined(_DEBUG) || defined (DEBUG)
		{
			ComPtr<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()))))
				debugController->EnableDebugLayer();
			else
				ER_OUTPUT_LOG(L"[ER Logger][ER_RHI_DX12] Direct3D Debug Device is not available\n");

			ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
			if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiInfoQueue.GetAddressOf()))))
			{
				mDXGIFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

				dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
				dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);

				DXGI_INFO_QUEUE_MESSAGE_ID hide[] = { 80, };
				DXGI_INFO_QUEUE_FILTER filter = {};
				filter.DenyList.NumIDs = _countof(hide);
				filter.DenyList.pIDList = hide;
				dxgiInfoQueue->AddStorageFilterEntries(DXGI_DEBUG_DXGI, &filter);
			}
		}
#endif

		if (FAILED(hr = CreateDXGIFactory2(mDXGIFactoryFlags, IID_PPV_ARGS(mDXGIFactory.ReleaseAndGetAddressOf()))))
			throw ER_CoreException("ER_RHI_DX12: CreateDXGIFactory2() failed", hr);

		ComPtr<IDXGIAdapter1> adapter;
		mDXGIFactory->EnumAdapters1(0, adapter.ReleaseAndGetAddressOf());

		// Create the DX12 API device object.
		if (FAILED(hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(mDevice.ReleaseAndGetAddressOf()))))
			throw ER_CoreException("ER_RHI_DX12: D3D12CreateDevice() failed", hr);

		ComPtr<ID3D12InfoQueue> pInfoQueue;
		if (SUCCEEDED(mDevice.As(&pInfoQueue)))
		{
			pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
			pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
			//pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
		}

		static const D3D_FEATURE_LEVEL s_featureLevels[] =
		{
			D3D_FEATURE_LEVEL_12_1,
			D3D_FEATURE_LEVEL_12_0,
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
		};

		D3D12_FEATURE_DATA_FEATURE_LEVELS featLevels =
		{
			_countof(s_featureLevels), s_featureLevels, D3D_FEATURE_LEVEL_11_0
		};

		hr = mDevice->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featLevels, sizeof(featLevels));
		if (SUCCEEDED(hr))
			mFeatureLevel = featLevels.MaxSupportedFeatureLevel;
		else
			mFeatureLevel = D3D_FEATURE_LEVEL_11_0;

		D3D12_FEATURE_DATA_D3D12_OPTIONS5 featureSupportData = {};
		HRESULT hr2 = mDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &featureSupportData, sizeof(featureSupportData));
		if (SUCCEEDED(hr2) && featureSupportData.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
			mIsRaytracingTierAvailable = true;
		else
			mIsRaytracingTierAvailable = false;


		// Create copy command queue data
		{
			D3D12_COMMAND_QUEUE_DESC queueDesc = {};
			queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; //maybe use D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT
			queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

			if (FAILED(mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(mCommandQueueCopy.ReleaseAndGetAddressOf()))))
				throw ER_CoreException("ER_RHI_DX12: Could not create copy command queue");

			// Create a command allocator
			if (FAILED(mDevice->CreateCommandAllocator(queueDesc.Type, IID_PPV_ARGS(mCommandAllocatorCopy.ReleaseAndGetAddressOf()))))
				throw ER_CoreException("ER_RHI_DX12: Could not create copy command allocator");
			
			if (FAILED(mDevice->CreateCommandList(0, queueDesc.Type, mCommandAllocatorCopy.Get(), nullptr, IID_PPV_ARGS(mCommandListCopy.ReleaseAndGetAddressOf()))))
				throw ER_CoreException("ER_RHI_DX12: Could not create copy command list");

			mCommandListCopy->Close();

			// fences
			{
				// Create a fence for tracking GPU execution progress.
				if (FAILED(mDevice->CreateFence(mFenceValuesCopy, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(mFenceCopy.ReleaseAndGetAddressOf()))))
					throw ER_CoreException("ER_RHI_DX12: Could not create copy fence");

				mFenceValuesCopy++;
				mFenceEventCopy.Attach(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
				if (!mFenceEventCopy.IsValid())
					throw ER_CoreException("ER_RHI_DX12: Could not create event for copy fence");
				mFenceCopy->SetName(L"ER_RHI_DX12: Copy fence");
			}
		}
		// Create graphics command queue data
		{
			D3D12_COMMAND_QUEUE_DESC queueDesc = {};
			queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

			if (FAILED(mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(mCommandQueueGraphics.ReleaseAndGetAddressOf()))))
				throw ER_CoreException("ER_RHI_DX12: Could not create graphics command queue");

			// Create descriptor heaps for render target views and depth stencil views.
			D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc = {};
			rtvDescriptorHeapDesc.NumDescriptors = DX12_MAX_BACK_BUFFER_COUNT;
			rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

			if (FAILED(mDevice->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(mRTVDescriptorHeap.ReleaseAndGetAddressOf()))))
				throw ER_CoreException("ER_RHI_DX12: Could not create descriptor heap for main RTV");

			mRTVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

			D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc = {};
			dsvDescriptorHeapDesc.NumDescriptors = 1;
			dsvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

			if (FAILED(mDevice->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(mDSVDescriptorHeap.ReleaseAndGetAddressOf()))))
				throw ER_CoreException("ER_RHI_DX12: Could not create descriptor heap for main DSV");

			for (int j = 0; j < DX12_MAX_BACK_BUFFER_COUNT; j++)
			{
				for (int i = 0; i < ER_RHI_MAX_GRAPHICS_COMMAND_LISTS; i++)
				{
					// Create a command allocator for each back buffer that will be rendered to.
					if (FAILED(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mCommandAllocatorsGraphics[j][i].ReleaseAndGetAddressOf()))))
					{
						std::string message = "ER_RHI_DX12: Could not create graphics command allocator " + std::to_string(j) + " " + std::to_string(i);
						throw ER_CoreException(message.c_str());
					}

					if (j == 0)
					{
						// Create a command list for recording graphics commands.
						if (FAILED(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocatorsGraphics[0][i].Get(), nullptr, IID_PPV_ARGS(mCommandListGraphics[i].ReleaseAndGetAddressOf()))))
						{
							std::string message = "ER_RHI_DX12: Could not create graphics command list " + std::to_string(i);
							throw ER_CoreException(message.c_str());
						}
						mCommandListGraphics[i]->Close();
					}
				}
			}

			// fences
			{
				// Create a fence for tracking GPU execution progress.
				if (FAILED(mDevice->CreateFence(mFenceValuesGraphics[mBackBufferIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(mFenceGraphics.ReleaseAndGetAddressOf()))))
					throw ER_CoreException("ER_RHI_DX12: Could not create main graphics fence");

				mFenceValuesGraphics[mBackBufferIndex]++;
				mFenceEventGraphics.Attach(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
				if (!mFenceEventGraphics.IsValid())
					throw ER_CoreException("ER_RHI_DX12: Could not create event for main graphics fence");
				mFenceGraphics->SetName(L"ER_RHI_DX12: Graphics fence (main)");

			}
		}

		//TODO create compute queue data 
		{
			//...
		}

		WaitForGpuOnGraphicsFence();

		// Create swapchain, main rtv, main dsv
		{
			for (UINT n = 0; n < DX12_MAX_BACK_BUFFER_COUNT; n++)
			{
				mMainRenderTarget[n].Reset();
				mFenceValuesGraphics[n] = mFenceValuesGraphics[mBackBufferIndex];
			}

			// swapchain
			{
				// Create a descriptor for the swap chain.
				DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
				swapChainDesc.Width = width;
				swapChainDesc.Height = height;
				swapChainDesc.Format = mMainRTBufferFormat;
				swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
				swapChainDesc.BufferCount = DX12_MAX_BACK_BUFFER_COUNT;
				swapChainDesc.SampleDesc.Count = 1;
				swapChainDesc.SampleDesc.Quality = 0;
				swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
				swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
				swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

				DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
				fsSwapChainDesc.RefreshRate.Denominator = DefaultFrameRate;
				fsSwapChainDesc.RefreshRate.Numerator = 1;
				fsSwapChainDesc.Windowed = TRUE;

				if (isReset)
					mSwapChain.Reset();

				ComPtr<IDXGISwapChain1> swapChain;
				// Create a swap chain for the window.
				if (FAILED(mDXGIFactory->CreateSwapChainForHwnd(mCommandQueueGraphics.Get(), windowHandle, &swapChainDesc, &fsSwapChainDesc, nullptr, swapChain.GetAddressOf())))
					throw ER_CoreException("ER_RHI_DX12: Could not create swapchain");
				swapChain.As(&mSwapChain);
				//ThrowIfFailed(mDXGIFactory->MakeWindowAssociation(mAppWindow, DXGI_MWA_NO_ALT_ENTER));
			}

			CreateMainRenderTargetAndDepth(width, height);
		}

		CreateSamplerStates();
		CreateRasterizerStates();
		CreateDepthStencilStates();
		CreateBlendStates();

		ResetDescriptorManager();

		//clear uav state and rs
		{
			mClearUAV2DCS = CreateGPUShader();
			mClearUAV2DCS->CompileShader(this, "content\\shaders\\ClearUAV2D.hlsl", "CSMain", ER_COMPUTE);
			mClearUAV3DCS = CreateGPUShader();
			mClearUAV3DCS->CompileShader(this, "content\\shaders\\ClearUAV3D.hlsl", "CSMain", ER_COMPUTE);

			//TODO add constant for clear value
			mClearUAV2DRS = CreateRootSignature(1, 0);
			if (mClearUAV2DRS)
			{
				mClearUAV2DRS->InitDescriptorTable(this, 0, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_UAV }, { 0 }, { 1 });
				mClearUAV2DRS->Finalize(this, "ER_RHI_GPURootSignature: Clear UAV 2D");
			}

			//TODO add constant for clear value
			mClearUAV3DRS = CreateRootSignature(1, 0);
			if (mClearUAV3DRS)
			{
				mClearUAV3DRS->InitDescriptorTable(this, 0, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_UAV }, { 0 }, { 1 });
				mClearUAV3DRS->Finalize(this, "ER_RHI_GPURootSignature: Clear UAV 3D");
			}
		}
		//generate mips 2D/3D state and rs
		{
			mGenerateMips2DCS = CreateGPUShader();
			mGenerateMips2DCS->CompileShader(this, "content\\shaders\\GenerateMips2D.hlsl", "CSMain", ER_COMPUTE);

			mGenerateMips3DCS = CreateGPUShader();
			mGenerateMips3DCS->CompileShader(this, "content\\shaders\\GenerateMips3D.hlsl", "CSMain", ER_COMPUTE);

			mGenerateMips2DRS = CreateRootSignature(3, 1);
			if (mGenerateMips2DRS)
			{
				mGenerateMips2DRS->InitStaticSampler(this, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_CLAMP);
				mGenerateMips2DRS->InitDescriptorTable(this, 0, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 1 });
				mGenerateMips2DRS->InitDescriptorTable(this, 1, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_UAV }, { 0 }, { 1 });
				mGenerateMips2DRS->InitConstant(this, 2, 0, 3);
				mGenerateMips2DRS->Finalize(this, "ER_RHI_GPURootSignature: Generate Mips 2D");
			}

			mGenerateMips3DRS = CreateRootSignature(3, 1);
			if (mGenerateMips3DRS)
			{
				mGenerateMips3DRS->InitStaticSampler(this, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_CLAMP);
				mGenerateMips3DRS->InitDescriptorTable(this, 0, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 1 });
				mGenerateMips3DRS->InitDescriptorTable(this, 1, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_UAV }, { 0 }, { 1 });
				mGenerateMips3DRS->InitConstant(this, 2, 0, 4);
				mGenerateMips3DRS->Finalize(this, "ER_RHI_GPURootSignature: Generate Mips 3D");
			}
		}

		// Create the command signatures used for indirect drawing.
		{
			// DrawInstanced call
			D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[1] = {};
			argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

			D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
			commandSignatureDesc.pArgumentDescs = argumentDescs;
			commandSignatureDesc.NumArgumentDescs = _countof(argumentDescs);
			commandSignatureDesc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);

			if (FAILED(mDevice->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(mCommandSignature_DrawIndexed.ReleaseAndGetAddressOf()))))
				throw ER_CoreException("ER_RHI_DX12: Could not create command signature (Draw Indexed)");
		}
		return true;
	}

	void ER_RHI_DX12::WaitForGpuOnGraphicsFence()
	{
		if (mCommandQueueGraphics && mFenceGraphics && mFenceEventGraphics.IsValid())
		{
			// Schedule a Signal command in the GPU queue.
			UINT64 fenceValue = mFenceValuesGraphics[mBackBufferIndex];
			if (SUCCEEDED(mCommandQueueGraphics->Signal(mFenceGraphics.Get(), fenceValue)))
			{
				// Wait until the Signal has been processed.
				if (SUCCEEDED(mFenceGraphics->SetEventOnCompletion(fenceValue, mFenceEventGraphics.Get())))
				{
					WaitForSingleObjectEx(mFenceEventGraphics.Get(), INFINITE, FALSE);

					// Increment the fence value for the current frame.
					mFenceValuesGraphics[mBackBufferIndex]++;
				}
			}
		}
	}

	void ER_RHI_DX12::WaitForGpuOnComputeFence()
	{
		//TODO
	}

	void ER_RHI_DX12::WaitForGpuOnCopyFence()
	{
		if (mCommandQueueCopy && mFenceCopy && mFenceEventCopy.IsValid())
		{
			// Schedule a Signal command in the GPU queue.
			UINT64 fenceValue = mFenceValuesCopy;
			if (SUCCEEDED(mCommandQueueCopy->Signal(mFenceCopy.Get(), fenceValue)))
			{
				// Wait until the Signal has been processed.
				if (SUCCEEDED(mFenceCopy->SetEventOnCompletion(fenceValue, mFenceEventCopy.Get())))
				{
					WaitForSingleObjectEx(mFenceEventCopy.Get(), INFINITE, FALSE);

					// Increment the fence value for the current frame.
					mFenceValuesCopy++;
				}
			}
		}
	}

	void ER_RHI_DX12::ResetReplacementMippedTexturesPool()
	{
		mGenerateMipsWithReplacementCurrentTextureIndexInPool = 0;
		// we must be sure that the scene was deallocated already (objects deleted the textures by themselves or by the texture cache system in ER_RuntimeCore)
		for (int i = 0; i < DX12_MAX_GENERATE_MIPS_TEXTURES_IN_POOL; i++)
		{
			mGenerateMipsWithReplacementReadyTexturesPool[i] = nullptr;
			//DeleteObject(mGenerateMipsWithReplacementReadyTexturesPool[i]);
		}
	}

	void ER_RHI_DX12::ResetDescriptorManager()
	{
		DeleteObject(mDescriptorHeapManager);
		mDescriptorHeapManager = new ER_RHI_DX12_GPUDescriptorHeapManager(mDevice.Get());

		// null SRVs descriptor handles
		{
			sNullSRV2DHandle = mDescriptorHeapManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			sNullSRV3DHandle = mDescriptorHeapManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			mDevice->CreateShaderResourceView(nullptr, &srvDesc, sNullSRV2DHandle.GetCPUHandle());

			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			srvDesc.Texture3D.MipLevels = 1;
			mDevice->CreateShaderResourceView(nullptr, &srvDesc, sNullSRV3DHandle.GetCPUHandle());
		}
	}

	void ER_RHI_DX12::ResetRHI(int width, int height, bool isFullscreen)
	{
		Initialize(mWindowHandle, width, height, isFullscreen, true);
	}

	void ER_RHI_DX12::BeginEventTag(const std::string& aName, bool isComputeQueue)
	{
		PIXBeginEvent(isComputeQueue ? mCommandListCompute[mCurrentComputeCommandListIndex].Get() : mCommandListGraphics[mCurrentGraphicsCommandListIndex].Get(), 0, aName.c_str());
	}

	void ER_RHI_DX12::EndEventTag(bool isComputeQueue)
	{
		PIXEndEvent(isComputeQueue ? mCommandListCompute[mCurrentComputeCommandListIndex].Get() : mCommandListGraphics[mCurrentGraphicsCommandListIndex].Get());
	}

	void ER_RHI_DX12::BeginGraphicsCommandList(int index)
	{
		assert(index < ER_RHI_MAX_GRAPHICS_COMMAND_LISTS);

		mCurrentGraphicsCommandListIndex = index;

		HRESULT hr;
		if (FAILED(hr = mCommandAllocatorsGraphics[mBackBufferIndex][index]->Reset()))
		{
			std::string message = "ER_RHI_DX12:: Could not Reset() command allocator (graphics) " + std::to_string(index);
			throw ER_CoreException(message.c_str());
		}

		if (FAILED(hr = mCommandListGraphics[index]->Reset(mCommandAllocatorsGraphics[mBackBufferIndex][index].Get(), nullptr)))
		{
			std::string message = "ER_RHI_DX12:: Could not Reset() command list (graphics) " + std::to_string(index);
			throw ER_CoreException(message.c_str());
		}
	}

	void ER_RHI_DX12::EndGraphicsCommandList(int index)
	{
		assert(index < ER_RHI_MAX_GRAPHICS_COMMAND_LISTS);
		mCurrentGraphicsCommandListIndex = -1;

		HRESULT hr;
		if (FAILED(hr = mCommandListGraphics[index]->Close()))
		{
			std::string message = "ER_RHI_DX12:: Could not close command list (graphics) " + std::to_string(index);
			throw ER_CoreException(message.c_str());
		}
	}

	void ER_RHI_DX12::BeginCopyCommandList(int index /*= 0*/)
	{
		HRESULT hr;
		if (FAILED(hr = mCommandAllocatorCopy->Reset()))
			throw ER_CoreException("ER_RHI_DX12:: Could not Reset() command allocator (copy) ");

		if (FAILED(hr = mCommandListCopy->Reset(mCommandAllocatorCopy.Get(), nullptr)))
			throw ER_CoreException("ER_RHI_DX12:: Could not Reset() command list (copy)");
	}

	void ER_RHI_DX12::EndCopyCommandList(int index /*= 0*/)
	{
		HRESULT hr;
		if (FAILED(hr = mCommandListCopy->Close()))
			throw ER_CoreException("ER_RHI_DX12:: Could not close command list (copy)");
	}

	void ER_RHI_DX12::ClearMainRenderTarget(float colors[4])
	{
		assert(mCurrentGraphicsCommandListIndex > -1);
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(mMainRenderTarget[mBackBufferIndex].Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mCommandListGraphics[mCurrentGraphicsCommandListIndex]->ResourceBarrier(1, &barrier);
		mCommandListGraphics[mCurrentGraphicsCommandListIndex]->ClearRenderTargetView(GetMainRenderTargetView(), colors, 0, nullptr);
	}

	void ER_RHI_DX12::ClearMainDepthStencilTarget(float depth, UINT stencil /*= 0*/)
	{
		assert(mCurrentGraphicsCommandListIndex > -1);
		mCommandListGraphics[mCurrentGraphicsCommandListIndex]->ClearDepthStencilView(GetMainDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0, nullptr);
	}

	void ER_RHI_DX12::ClearRenderTarget(ER_RHI_GPUTexture* aRenderTarget, float colors[4], int rtvArrayIndex)
	{
		assert(mCurrentGraphicsCommandListIndex > -1);
		assert(aRenderTarget);
		TransitionResources({ static_cast<ER_RHI_GPUResource*>(aRenderTarget) }, ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_RENDER_TARGET);
		if (rtvArrayIndex > 0)
		{
			ER_RHI_DX12_DescriptorHandle& handle = static_cast<ER_RHI_DX12_GPUTexture*>(aRenderTarget)->GetRTVHandle(rtvArrayIndex);
			mCommandListGraphics[mCurrentGraphicsCommandListIndex]->ClearRenderTargetView(handle.GetCPUHandle(), colors, 0, nullptr);
		}
		else
		{
			ER_RHI_DX12_DescriptorHandle& handle = static_cast<ER_RHI_DX12_GPUTexture*>(aRenderTarget)->GetRTVHandle();
			mCommandListGraphics[mCurrentGraphicsCommandListIndex]->ClearRenderTargetView(handle.GetCPUHandle(), colors, 0, nullptr);
		}
	}

	void ER_RHI_DX12::ClearDepthStencilTarget(ER_RHI_GPUTexture* aDepthTarget, float depth, UINT stencil)
	{
		assert(mCurrentGraphicsCommandListIndex > -1);
		assert(aDepthTarget);
		ER_RHI_DX12_GPUTexture* dtDX12 = static_cast<ER_RHI_DX12_GPUTexture*>(aDepthTarget);
		assert(dtDX12);
		TransitionResources({ aDepthTarget }, ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_DEPTH_WRITE);
		mCommandListGraphics[mCurrentGraphicsCommandListIndex]->ClearDepthStencilView(dtDX12->GetDSVHandle().GetCPUHandle(), (stencil == -1) ? D3D12_CLEAR_FLAG_DEPTH : D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0, nullptr);
	}

	// Two versions are available (shader and command). Shader is the default one at the moment
	void ER_RHI_DX12::ClearUAV(ER_RHI_GPUResource* aRenderTarget, float colors[4])
	{
		assert(mCurrentGraphicsCommandListIndex > -1);
		assert(aRenderTarget);
		ER_RHI_DX12_GPUTexture* uavDX12 = static_cast<ER_RHI_DX12_GPUTexture*>(aRenderTarget);
		assert(uavDX12);

		if (uavDX12->GetBackBufferIndex() != mBackBufferIndex)
			return; //quick fix for not touching the resource that was created on a different back buffer's heap

		bool is3D = uavDX12->GetDepth() > 0;
		TransitionResources({ aRenderTarget }, ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_UNORDERED_ACCESS, mCurrentGraphicsCommandListIndex);

		#pragma region SHADER_CLEAR
		auto cmdList = mCommandListGraphics[mCurrentGraphicsCommandListIndex];

		const std::string psoName = is3D ? mClearUAV3DPSOName : mClearUAV2DPSOName;
		ER_RHI_GPURootSignature* rs = is3D ? mClearUAV3DRS : mClearUAV2DRS;

		SetRootSignature(rs, true);
		if (!IsPSOReady(psoName, true))
		{
			InitializePSO(psoName, true);
			SetRootSignatureToPSO(psoName, rs, true);
			SetShader(is3D ? mClearUAV3DCS : mClearUAV2DCS);
			FinalizePSO(psoName, true);
		}
		SetPSO(psoName, true);

		int mipCount = uavDX12->GetMips();
		UINT srcWidth = uavDX12->GetWidth();
		UINT srcHeight = uavDX12->GetHeight();
		UINT srcDepth = uavDX12->GetDepth();

		for (int mip = 0; mip < mipCount; mip++)
		{
			UINT dstWidth = std::max(srcWidth >> mip, 1u);
			UINT dstHeight = std::max(srcHeight >> mip, 1u);
			UINT dstDepth = is3D ? std::max(srcDepth >> mip, 1u) : 1u;

			//TODO set root constant for clear values
			SetUnorderedAccessResources(ER_COMPUTE, { aRenderTarget }, mip, rs, 0, true);
			Dispatch(ER_CEIL(dstWidth, 8), ER_CEIL(dstHeight, 8), is3D ? ER_CEIL(dstDepth, 8) : 1u);
			cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(static_cast<ID3D12Resource*>(aRenderTarget->GetResource())));
		}
		UnsetPSO();
		TransitionResources({ aRenderTarget }, ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, mCurrentGraphicsCommandListIndex);
#pragma endregion

		#pragma region COMMAND_CLEAR
		// For some reason this giving some DX12 "unmatched descriptor" errors, although everything seems to be set up correctly with descriptors
		// Maybe driver thing, but you can try it on your machine

		//if (uavDX12->GetMips() <= 1)
		//{
		//	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = uavDX12->GetUAVHandle().GetCPUHandle();
		//	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = uavDX12->GetUAVHandleGPU().GetGPUHandle();
		//
		//	mCommandListGraphics[mCurrentGraphicsCommandListIndex]->ClearUnorderedAccessViewFloat(gpuHandle, cpuHandle, static_cast<ID3D12Resource*>(uavDX12->GetResource()), colors, 0, nullptr);
		//}
		//else
		//{
		//	for (int i = 0; i < static_cast<int>(uavDX12->GetMips()); i++)
		//	{
		//		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = uavDX12->GetUAVHandle(i).GetCPUHandle();
		//		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = uavDX12->GetUAVHandleGPU(i).GetGPUHandle();
		//
		//		mCommandListGraphics[mCurrentGraphicsCommandListIndex]->ClearUnorderedAccessViewFloat(gpuHandle, cpuHandle, static_cast<ID3D12Resource*>(uavDX12->GetResource()), colors, 0, nullptr);
		//	}
		//}
#pragma endregion
	}

	void ER_RHI_DX12::ClearUAV(ER_RHI_GPUBuffer* aBuffer, UINT value)
	{

	}

	ER_RHI_InputLayout* ER_RHI_DX12::CreateInputLayout(ER_RHI_INPUT_ELEMENT_DESC* inputElementDescriptions, UINT inputElementDescriptionCount)
	{
		return new ER_RHI_InputLayout(inputElementDescriptions, inputElementDescriptionCount);
	}

	ER_RHI_GPUShader* ER_RHI_DX12::CreateGPUShader()
	{
		return new ER_RHI_DX12_GPUShader();
	}

	ER_RHI_GPUBuffer* ER_RHI_DX12::CreateGPUBuffer(const std::string& aDebugName)
	{
		return new ER_RHI_DX12_GPUBuffer(aDebugName);
	}

	ER_RHI_GPUTexture* ER_RHI_DX12::CreateGPUTexture(const std::wstring& aDebugName)
	{
		return new ER_RHI_DX12_GPUTexture(aDebugName);
	}

	ER_RHI_GPURootSignature* ER_RHI_DX12::CreateRootSignature(UINT NumRootParams /*= 0*/, UINT NumStaticSamplers /*= 0*/)
	{
		return new ER_RHI_DX12_GPURootSignature(NumRootParams, NumStaticSamplers);
	}

	void ER_RHI_DX12::CreateTexture(ER_RHI_GPUTexture* aOutTexture, UINT width, UINT height, UINT samples, ER_RHI_FORMAT format, ER_RHI_BIND_FLAG bindFlags /*= ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET*/, int mip /*= 1*/, int depth /*= -1*/, int arraySize /*= 1*/, bool isCubemap /*= false*/, int cubemapArraySize /*= -1*/)
	{
		assert(aOutTexture);
		aOutTexture->CreateGPUTextureResource(this, width, height, samples, format, bindFlags, mip, depth, arraySize, isCubemap, cubemapArraySize);
	}

	void ER_RHI_DX12::CreateTexture(ER_RHI_GPUTexture* aOutTexture, const std::string& aPath, bool isFullPath /*= false*/)
	{
		assert(aOutTexture);
		aOutTexture->CreateGPUTextureResource(this, aPath, isFullPath);
	}

	void ER_RHI_DX12::CreateTexture(ER_RHI_GPUTexture* aOutTexture, const std::wstring& aPath, bool isFullPath /*= false*/)
	{
		assert(aOutTexture);
		aOutTexture->CreateGPUTextureResource(this, aPath, isFullPath);
	}

	void ER_RHI_DX12::CreateBuffer(ER_RHI_GPUBuffer* aOutBuffer, void* aData, UINT objectsCount, UINT byteStride, bool isDynamic /*= false*/, ER_RHI_BIND_FLAG bindFlags /*= 0*/, UINT cpuAccessFlags /*= 0*/, ER_RHI_RESOURCE_MISC_FLAG miscFlags /*= 0*/, ER_RHI_FORMAT format /*= ER_FORMAT_UNKNOWN*/)
	{
		assert(aOutBuffer);
		aOutBuffer->CreateGPUBufferResource(this, aData, objectsCount, byteStride, isDynamic, bindFlags, cpuAccessFlags, miscFlags, format);
	}

	void ER_RHI_DX12::CopyBuffer(ER_RHI_GPUBuffer* aDestBuffer, ER_RHI_GPUBuffer* aSrcBuffer, int cmdListIndex, bool isInCopyQueue)
	{
		if (!isInCopyQueue)
			assert(cmdListIndex > -1);
		assert(aDestBuffer);
		assert(aSrcBuffer);

		ER_RHI_DX12_GPUBuffer* dstResource = static_cast<ER_RHI_DX12_GPUBuffer*>(aDestBuffer);
		ER_RHI_DX12_GPUBuffer* srcResource = static_cast<ER_RHI_DX12_GPUBuffer*>(aSrcBuffer);
		
		assert(dstResource);
		assert(srcResource);

		if (!isInCopyQueue)
			TransitionResources({ static_cast<ER_RHI_GPUResource*>(aDestBuffer), static_cast<ER_RHI_GPUResource*>(aSrcBuffer) },
			{ ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_COPY_DEST, ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_COPY_SOURCE }, isInCopyQueue ? 0 : cmdListIndex, isInCopyQueue);

		if (!isInCopyQueue)
			mCommandListGraphics[cmdListIndex]->CopyResource(static_cast<ID3D12Resource*>(dstResource->GetResource()), static_cast<ID3D12Resource*>(srcResource->GetResource()));
		else
			mCommandListCopy->CopyResource(static_cast<ID3D12Resource*>(dstResource->GetResource()), static_cast<ID3D12Resource*>(srcResource->GetResource()));
		
		if (!isInCopyQueue)
			TransitionResources({ static_cast<ER_RHI_GPUResource*>(aDestBuffer), static_cast<ER_RHI_GPUResource*>(aSrcBuffer) },
			{ ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_COMMON }, isInCopyQueue ? 0 : cmdListIndex, isInCopyQueue);
	}

	void ER_RHI_DX12::BeginBufferRead(ER_RHI_GPUBuffer* aBuffer, void** output)
	{
		assert(aBuffer);
		assert(!mIsContextReadingBuffer);

		ER_RHI_DX12_GPUBuffer* buffer = static_cast<ER_RHI_DX12_GPUBuffer*>(aBuffer);
		assert(buffer);

		buffer->Map(this, output);
		mIsContextReadingBuffer = true;
	}

	void ER_RHI_DX12::EndBufferRead(ER_RHI_GPUBuffer* aBuffer)
	{
		ER_RHI_DX12_GPUBuffer* buffer = static_cast<ER_RHI_DX12_GPUBuffer*>(aBuffer);
		assert(buffer);

		buffer->Unmap(this);
		mIsContextReadingBuffer = false;
	}

	void ER_RHI_DX12::CopyGPUTextureSubresourceRegion(ER_RHI_GPUResource* aDestBuffer, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ER_RHI_GPUResource* aSrcBuffer, UINT SrcSubresource, bool isInCopyQueueOrSkipTransitions)
	{
		if (!isInCopyQueueOrSkipTransitions)
			assert(mCurrentGraphicsCommandListIndex > -1);
		assert(aDestBuffer);
		assert(aSrcBuffer);

		ER_RHI_DX12_GPUTexture* dstbuffer = static_cast<ER_RHI_DX12_GPUTexture*>(aDestBuffer);
		assert(dstbuffer);
		ER_RHI_DX12_GPUTexture* srcbuffer = static_cast<ER_RHI_DX12_GPUTexture*>(aSrcBuffer);
		assert(srcbuffer);

		if (!isInCopyQueueOrSkipTransitions)
		{
			TransitionResources({ static_cast<ER_RHI_GPUResource*>(dstbuffer), static_cast<ER_RHI_GPUResource*>(srcbuffer) },
				{ ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_COPY_DEST, ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_COPY_SOURCE }, mCurrentGraphicsCommandListIndex);
		}

		D3D12_TEXTURE_COPY_LOCATION dstLocation;
		dstLocation.pResource = static_cast<ID3D12Resource*>(dstbuffer->GetResource());
		dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dstLocation.SubresourceIndex = DstSubresource;

		D3D12_TEXTURE_COPY_LOCATION srcLocation;
		srcLocation.pResource = static_cast<ID3D12Resource*>(srcbuffer->GetResource());
		srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		srcLocation.SubresourceIndex = SrcSubresource;
		
		mCommandListGraphics[mCurrentGraphicsCommandListIndex]->CopyTextureRegion(&dstLocation, DstX, DstY, DstZ, &srcLocation, NULL);
		//else if (dstbuffer->GetTexture3D() && srcbuffer->GetTexture3D())
		//else
		//	throw ER_CoreException("ER_RHI_DX12:: One of the resources is NULL during CopyGPUTextureSubresourceRegion()");

		if (!isInCopyQueueOrSkipTransitions)
		{
			TransitionResources({ static_cast<ER_RHI_GPUResource*>(dstbuffer), static_cast<ER_RHI_GPUResource*>(srcbuffer) },
				{ ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_PIXEL_SHADER_RESOURCE }, mCurrentGraphicsCommandListIndex);
		}
	}

	void ER_RHI_DX12::Draw(UINT VertexCount)
	{
		assert(mCurrentGraphicsCommandListIndex > -1);
		assert(VertexCount > 0);
		mCommandListGraphics[mCurrentGraphicsCommandListIndex]->DrawInstanced(VertexCount, 1, 0, 0);
	}

	void ER_RHI_DX12::DrawIndexed(UINT IndexCount)
	{
		assert(mCurrentGraphicsCommandListIndex > -1);
		assert(IndexCount > 0);
		mCommandListGraphics[mCurrentGraphicsCommandListIndex]->DrawIndexedInstanced(IndexCount, 1, 0, 0, 0);
	}

	void ER_RHI_DX12::DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
	{
		assert(VertexCountPerInstance > 0);
		assert(InstanceCount > 0);
		assert(mCurrentGraphicsCommandListIndex > -1);

		mCommandListGraphics[mCurrentGraphicsCommandListIndex]->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
	}

	void ER_RHI_DX12::DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
	{
		assert(mCurrentGraphicsCommandListIndex > -1);
		assert(IndexCountPerInstance > 0);
		assert(InstanceCount > 0);

		mCommandListGraphics[mCurrentGraphicsCommandListIndex]->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
	}

	void ER_RHI_DX12::DrawIndexedInstancedIndirect(ER_RHI_GPUBuffer* anArgsBuffer, UINT alignedByteOffset)
	{
		assert(anArgsBuffer);
		assert(mCurrentGraphicsCommandListIndex > -1);

		TransitionResources({ anArgsBuffer }, ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_INDIRECT_ARGUMENT);

		mCommandListGraphics[mCurrentGraphicsCommandListIndex]->ExecuteIndirect(mCommandSignature_DrawIndexed.Get(), 1, static_cast<ID3D12Resource*>(anArgsBuffer->GetResource()), alignedByteOffset, nullptr, 0);
	}

	void ER_RHI_DX12::Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)
	{
		assert(mCurrentGraphicsCommandListIndex > -1);
		mCommandListGraphics[mCurrentGraphicsCommandListIndex]->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	}

	void ER_RHI_DX12::ExecuteCommandLists(int commandListIndex /*= 0*/, bool isCompute /*= false*/)
	{
		if (!isCompute)
		{
			ID3D12CommandList* ppCommandLists[] = { mCommandListGraphics[commandListIndex].Get() };
			mCommandQueueGraphics->ExecuteCommandLists(1, ppCommandLists);
		}
		//else TODO
	}

	void ER_RHI_DX12::ExecuteCopyCommandList()
	{
		ID3D12CommandList* ppCommandLists[] = { mCommandListCopy.Get() };
		mCommandQueueCopy->ExecuteCommandLists(1, ppCommandLists);

		WaitForGpuOnCopyFence();
	}

	void ER_RHI_DX12::GenerateMips(ER_RHI_GPUTexture* aTexture, ER_RHI_GPUTexture* aSRGBTexture)
	{
		if ((aTexture->GetWidth() != aTexture->GetHeight()) /*|| !(ER_IsPowerOfTwo(aTexture->GetWidth()) && ER_IsPowerOfTwo(aTexture->GetHeight()))*/)
			return; //TODO add support

		const UINT rootParamIndexSrv = 0;
		const UINT rootParamIndexUav = 1;
		const UINT rootParamIndexConstant = 2;

		assert(mCurrentGraphicsCommandListIndex > -1);

		UINT srcWidth = aTexture->GetWidth();
		UINT srcHeight = aTexture->GetHeight();
		UINT srcDepth = aTexture->GetDepth();
		bool is3D = srcDepth > 0;
		if (is3D && (srcDepth != srcHeight || srcDepth != srcWidth))
			return;

		UINT mipCount = (aTexture->GetMips() > 1) ? aTexture->GetMips() : aTexture->GetCalculatedMipCount();
		assert(mipCount > 1);

		auto cmdList = mCommandListGraphics[mCurrentGraphicsCommandListIndex];

		const std::string& psoName = is3D ? mGenerateMips3DPSOName : mGenerateMips2DPSOName;
		ER_RHI_GPURootSignature* rs = is3D ? mGenerateMips3DRS : mGenerateMips2DRS;

		SetRootSignature(rs, true);
		if (!IsPSOReady(psoName, true))
		{
			InitializePSO(psoName, true);
			SetRootSignatureToPSO(psoName, rs, true);
			SetShader(is3D ? mGenerateMips3DCS : mGenerateMips2DCS);
			FinalizePSO(psoName, true);
		}
		SetPSO(psoName, true);

		//transition first mip to non-pixel shader resource (because we will read from it) and all other mips to unordered access
		std::vector< CD3DX12_RESOURCE_BARRIER> barriers;
		barriers.emplace_back(CD3DX12_RESOURCE_BARRIER::Transition(static_cast<ID3D12Resource*>(aTexture->GetResource()), GetState(aTexture->GetCurrentState()), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 0));
		for (UINT mip = 1; mip < mipCount; mip++)
		{
			if (GetState(aTexture->GetCurrentState()) != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				barriers.emplace_back(CD3DX12_RESOURCE_BARRIER::Transition(static_cast<ID3D12Resource*>(aTexture->GetResource()), GetState(aTexture->GetCurrentState()), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, mip));
		}
		cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

		SetShaderResources(ER_COMPUTE, { !aSRGBTexture ? aTexture : aSRGBTexture }, 0, rs, rootParamIndexSrv, true, true);

		for (UINT mip = 0; mip < mipCount; mip++)
		{
			if (!aSRGBTexture && mip == 0)
				continue;

			UINT dstWidth = std::max(srcWidth >> mip, 1u);
			UINT dstHeight = std::max(srcHeight >> mip, 1u);
			UINT dstDepth = is3D ? std::max(srcDepth >> mip, 1u) : 1u;

			cmdList->SetComputeRoot32BitConstant(rootParamIndexConstant, dstWidth, 0);
			cmdList->SetComputeRoot32BitConstant(rootParamIndexConstant, dstHeight, 1);
			if (is3D)
				cmdList->SetComputeRoot32BitConstant(rootParamIndexConstant, dstDepth, 2);
			cmdList->SetComputeRoot32BitConstant(rootParamIndexConstant, /*TODO aSRGBTexture ? 1 : */0, is3D ? 3 : 2);

			SetUnorderedAccessResources(ER_COMPUTE, { aTexture }, mip, rs, rootParamIndexUav, true, true);
			Dispatch(ER_CEIL(dstWidth, 8), ER_CEIL(dstHeight, 8), is3D ? ER_CEIL(dstDepth, 8) : 1u);

			cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(static_cast<ID3D12Resource*>(aTexture->GetResource())));
		}
		UnsetPSO();

		//reset the transitions
		std::vector< CD3DX12_RESOURCE_BARRIER> barriersFinal;
		barriersFinal.emplace_back(CD3DX12_RESOURCE_BARRIER::Transition(static_cast<ID3D12Resource*>(aTexture->GetResource()), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0));
		for (UINT mip = 1; mip < mipCount; mip++)
			barriersFinal.emplace_back(CD3DX12_RESOURCE_BARRIER::Transition(static_cast<ID3D12Resource*>(aTexture->GetResource()), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, mip));
		cmdList->ResourceBarrier(static_cast<UINT>(barriersFinal.size()), barriersFinal.data());
		aTexture->SetCurrentState(ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	void ER_RHI_DX12::GenerateMipsWithTextureReplacement(ER_RHI_GPUTexture** aTexture, std::function<void(ER_RHI_GPUTexture**)> aReplacementCallback)
	{
		if ((*aTexture)->GetMips() > 1) //probably the texture already has mips
			return;

		if (mGenerateMipsWithReplacementCurrentTextureIndexInPool > DX12_MAX_GENERATE_MIPS_TEXTURES_IN_POOL)
			throw ER_CoreException("ER_RHI_DX12:: There is no space left in the temp texture pool for mip generation! Bump DX12_MAX_GENERATE_MIPS_TEXTURES_IN_POOL.");

		ER_RHI_DX12_GPUTexture* dx12Texture = static_cast<ER_RHI_DX12_GPUTexture*>(*aTexture);
		assert(dx12Texture);

		bool isSRGB = IsFormatSRGB(dx12Texture->GetFormat());
		DXGI_FORMAT newFormat;
		if (isSRGB)
			newFormat = ChangeFormatToNonSRGB(dx12Texture->GetFormat());
		else
			newFormat = ChangeFormatToUncompressed(dx12Texture->GetFormat());

		// create new texture with empty mips from the pool
		std::wstring name = dx12Texture->GetDebugName() + L" + mip maps";
		assert(!mGenerateMipsWithReplacementReadyTexturesPool[mGenerateMipsWithReplacementCurrentTextureIndexInPool]);

		mGenerateMipsWithReplacementReadyTexturesPool[mGenerateMipsWithReplacementCurrentTextureIndexInPool] = CreateGPUTexture(name);
		static_cast<ER_RHI_DX12_GPUTexture*>(mGenerateMipsWithReplacementReadyTexturesPool[mGenerateMipsWithReplacementCurrentTextureIndexInPool])->CreateSimpleGPUTexture2DResource(this, dx12Texture->GetWidth(), dx12Texture->GetHeight(), newFormat,
			ER_RHI_BIND_FLAG::ER_BIND_SHADER_RESOURCE | ER_RHI_BIND_FLAG::ER_BIND_UNORDERED_ACCESS, dx12Texture->GetCalculatedMipCount());

		// copy from main texture to 0 mip of new texture (if srgb, then the shader will write into mip 0)
		if (!isSRGB)
			CopyGPUTextureSubresourceRegion(mGenerateMipsWithReplacementReadyTexturesPool[mGenerateMipsWithReplacementCurrentTextureIndexInPool], 0, 0, 0, 0, *aTexture, 0);
		// generate the mip chain in the new texture (read from original texture in the compute shader if sRGB)
		GenerateMips(mGenerateMipsWithReplacementReadyTexturesPool[mGenerateMipsWithReplacementCurrentTextureIndexInPool], isSRGB ? *aTexture : nullptr);

		mGenerateMipsWithReplacementCallbacks[mGenerateMipsWithReplacementCurrentTextureIndexInPool] = aReplacementCallback;
		mGenerateMipsWithReplacementCurrentTextureIndexInPool++;
		// we delete the original textures and replace them with mipped in ReplaceOriginalTexturesWithMipped() (we can't delete before flushing the gfx queue)
	}

	// Running the callbacks from all systems/objects that requested to generate mips with replacement instead of their original non-mip textures
	// WARNING: should be called after flushing the GPU (otherwise everything that happens in GenerateMipsWithTextureReplacement() will work with deleted data and crash)
	// WARNING: if you don't wait for the GPU to finish GenerateMips() passes, then you might get corrupted textures here
	void ER_RHI_DX12::ReplaceOriginalTexturesWithMipped()
	{
		for (int i = 0; i < mGenerateMipsWithReplacementCurrentTextureIndexInPool; i++)
			mGenerateMipsWithReplacementCallbacks[i](&mGenerateMipsWithReplacementReadyTexturesPool[i]);
	}

	void ER_RHI_DX12::PresentGraphics()
	{
		HRESULT hr = mSwapChain->Present(0, 0);

		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
#if defined (_DEBUG) || (DEBUG)
			char buff[64] = {};
			sprintf_s(buff, "ER_RHI_DX12: Device Lost on Present: Reason code 0x%08X\n", (hr == DXGI_ERROR_DEVICE_REMOVED) ? mDevice->GetDeviceRemovedReason() : hr);
			OutputDebugStringA(buff);
#endif
			throw ER_CoreException("ER_RHI_DX12: Could not call Present() in the swapchain...", hr);
		}
		else
		{
			// Schedule a Signal command in the queue.
			const UINT64 currentFenceValue = mFenceValuesGraphics[mBackBufferIndex];
			if (FAILED(mCommandQueueGraphics->Signal(mFenceGraphics.Get(), currentFenceValue)))
				throw ER_CoreException("ER_RHI_DX12: Could not signal main graphics command queue during Present()");

			// Update the back buffer index.
			mBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

			// If the next frame is not ready to be rendered yet, wait until it is ready.
			if (mFenceGraphics->GetCompletedValue() < mFenceValuesGraphics[mBackBufferIndex])
			{
				mFenceGraphics->SetEventOnCompletion(mFenceValuesGraphics[mBackBufferIndex], mFenceEventGraphics.Get());
				WaitForSingleObjectEx(mFenceEventGraphics.Get(), INFINITE, FALSE);
			}

			// Set the fence value for the next frame.
			mFenceValuesGraphics[mBackBufferIndex] = currentFenceValue + 1;

			if (!mDXGIFactory->IsCurrent())
			{
				if (FAILED(CreateDXGIFactory2(mDXGIFactoryFlags, IID_PPV_ARGS(mDXGIFactory.ReleaseAndGetAddressOf()))))
					throw ER_CoreException("ER_RHI_DX12: Could not create DXGI factory during Present()");
			}
		}
	}

	void ER_RHI_DX12::PresentCompute()
	{
		//mCommandListCompute->Close();
		//
		//ID3D12CommandList* ppCommandLists[] = { mCommandListCompute };
		//mCommandQueueCompute->ExecuteCommandLists(1, ppCommandLists);

		//UINT64 fenceValue = mFenceValuesCompute;
		//mCommandQueueCompute->Signal(mFenceCompute.Get(), fenceValue);
		//mFenceValuesCompute++;
	}

	bool ER_RHI_DX12::ProjectCubemapToSH(ER_RHI_GPUTexture* aTexture, UINT order, float* resultR, float* resultG, float* resultB)
	{
		//TODO
		return false;
	}

	void ER_RHI_DX12::SaveGPUTextureToFile(ER_RHI_GPUTexture* aTexture, const std::wstring& aPathName)
	{
		//TODO
	}

	void ER_RHI_DX12::SetMainRenderTargets(int cmdListIndex)
	{
		mCommandListGraphics[cmdListIndex]->OMSetRenderTargets(1, &GetMainRenderTargetView(), false, &GetMainDepthStencilView());
	}

	void ER_RHI_DX12::SetRenderTargets(const std::vector<ER_RHI_GPUTexture*>& aRenderTargets, ER_RHI_GPUTexture* aDepthTarget /*= nullptr*/, ER_RHI_GPUTexture* aUAV /*= nullptr*/, int rtvArrayIndex)
	{
		assert(mCurrentGraphicsCommandListIndex > -1);
		if (!aUAV)
		{
			if (rtvArrayIndex > 0)
				assert(aRenderTargets.size() == 1);

			assert(aRenderTargets.size() > 0);
			assert(aRenderTargets.size() <= DX12_MAX_BOUND_RENDER_TARGETS_VIEWS);

			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[DX12_MAX_BOUND_RENDER_TARGETS_VIEWS] = {};
			UINT rtCount = static_cast<UINT>(aRenderTargets.size());
			std::vector<ER_RHI_GPUResource*> resources(rtCount);
			for (UINT i = 0; i < rtCount; i++)
			{
				assert(aRenderTargets[i]);
				if (rtvArrayIndex > 0)
					rtvHandles[i] = static_cast<ER_RHI_DX12_GPUTexture*>(aRenderTargets[i])->GetRTVHandle(rtvArrayIndex).GetCPUHandle();
				else
					rtvHandles[i] = static_cast<ER_RHI_DX12_GPUTexture*>(aRenderTargets[i])->GetRTVHandle().GetCPUHandle();

				resources[i] = static_cast<ER_RHI_GPUResource*>(aRenderTargets[i]);
			}

			if (aDepthTarget)
			{
				D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = static_cast<ER_RHI_DX12_GPUTexture*>(aDepthTarget)->GetDSVHandle().GetCPUHandle();

				std::vector<ER_RHI_RESOURCE_STATE> transitions(rtCount);
				for (UINT i = 0; i < rtCount; i++)
					transitions[i] = ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_RENDER_TARGET;

				resources.push_back(static_cast<ER_RHI_GPUResource*>(aDepthTarget));
				transitions.push_back(ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_DEPTH_WRITE);
				TransitionResources(resources, transitions);
				mCommandListGraphics[mCurrentGraphicsCommandListIndex]->OMSetRenderTargets(rtCount, rtvHandles, FALSE, &dsvHandle);
			}
			else
			{
				TransitionResources(resources, ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_RENDER_TARGET);
				mCommandListGraphics[mCurrentGraphicsCommandListIndex]->OMSetRenderTargets(rtCount, rtvHandles, FALSE, NULL);
			}

		}
		else
		{
			assert(0);
			//TODO: OMSetRenderTargetsAndUnorderedAccessViews() is not available on DX12, use SetUnorderedAccessResources() instead
		}
	}

	void ER_RHI_DX12::SetDepthTarget(ER_RHI_GPUTexture* aDepthTarget)
	{
		assert(mCurrentGraphicsCommandListIndex > -1);

		assert(aDepthTarget);
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = static_cast<ER_RHI_DX12_GPUTexture*>(aDepthTarget)->GetDSVHandle().GetCPUHandle();
		TransitionResources({ static_cast<ER_RHI_GPUResource*>(aDepthTarget) }, ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_DEPTH_WRITE);

		mCommandListGraphics[mCurrentGraphicsCommandListIndex]->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);
	}

	void ER_RHI_DX12::SetRenderTargetFormats(const std::vector<ER_RHI_GPUTexture*>& aRenderTargets, ER_RHI_GPUTexture* aDepthTarget /*= nullptr*/)
	{
		if (mCurrentPSOState == ER_RHI_DX12_PSO_STATE::COMPUTE)
			return;

		assert(mCurrentPSOState == ER_RHI_DX12_PSO_STATE::GRAPHICS);

		ER_RHI_DX12_GraphicsPSO& pso = mGraphicsPSONames.at(mCurrentGraphicsPSOName);
		int rtCount = static_cast<int>(aRenderTargets.size());
		assert(rtCount <= 8);

		std::vector<DXGI_FORMAT> formats;
		for (int i = 0; i < rtCount; i++)
			formats.push_back(static_cast<ER_RHI_DX12_GPUTexture*>(aRenderTargets[i])->GetFormat());
		pso.SetRenderTargetFormats(rtCount, rtCount > 0 ? &formats[0] : nullptr, aDepthTarget ? static_cast<ER_RHI_DX12_GPUTexture*>(aDepthTarget)->GetFormat() : DXGI_FORMAT_UNKNOWN);
	}

	void ER_RHI_DX12::SetMainRenderTargetFormats()
	{
		assert(mCurrentPSOState == ER_RHI_DX12_PSO_STATE::GRAPHICS);

		ER_RHI_DX12_GraphicsPSO& pso = mGraphicsPSONames.at(mCurrentGraphicsPSOName);
		pso.SetRenderTargetFormats(1, &mMainRTBufferFormat, mMainDepthBufferFormat);
	}

	void ER_RHI_DX12::SetDepthStencilState(ER_RHI_DEPTH_STENCIL_STATE aDS, UINT stencilRef)
	{
		if (mCurrentPSOState == ER_RHI_DX12_PSO_STATE::UNSET)
			return;

		assert(mCurrentPSOState == ER_RHI_DX12_PSO_STATE::GRAPHICS);

		auto it = mDepthStates.find(aDS);
		if (it != mDepthStates.end())
		{
			mCurrentDS = aDS;
			ER_RHI_DX12_GraphicsPSO& pso = mGraphicsPSONames.at(mCurrentGraphicsPSOName);
			pso.SetDepthStencilState(it->second);
		}
		else
			throw ER_CoreException("ER_RHI_DX11: DepthStencil state is not found.");
	}

	void ER_RHI_DX12::SetBlendState(ER_RHI_BLEND_STATE aBS, const float BlendFactor[4], UINT SampleMask)
	{
		if (mCurrentPSOState == ER_RHI_DX12_PSO_STATE::UNSET)
			return;

		assert(mCurrentPSOState == ER_RHI_DX12_PSO_STATE::GRAPHICS);

		auto it = mBlendStates.find(aBS);
		if (it != mBlendStates.end())
		{
			mCurrentBS = aBS;
			ER_RHI_DX12_GraphicsPSO& pso = mGraphicsPSONames.at(mCurrentGraphicsPSOName);
			pso.SetBlendState(it->second);
		}
		else
			throw ER_CoreException("ER_RHI_DX11: Blend state is not found.");
	}

	void ER_RHI_DX12::SetRasterizerState(ER_RHI_RASTERIZER_STATE aRS)
	{
		if (mCurrentPSOState == ER_RHI_DX12_PSO_STATE::UNSET)
			return;

		assert(mCurrentPSOState == ER_RHI_DX12_PSO_STATE::GRAPHICS);

		auto it = mRasterizerStates.find(aRS);
		if (it != mRasterizerStates.end())
		{
			mCurrentRS = aRS;
			ER_RHI_DX12_GraphicsPSO& pso = mGraphicsPSONames.at(mCurrentGraphicsPSOName);
			pso.SetRasterizerState(it->second);
		}
		else
			throw ER_CoreException("ER_RHI_DX11: Rasterizer state is not found.");
	}

	void ER_RHI_DX12::SetViewport(const ER_RHI_Viewport& aViewport)
	{
		D3D12_VIEWPORT viewport;
		viewport.TopLeftX = aViewport.TopLeftX;
		viewport.TopLeftY = aViewport.TopLeftY;
		viewport.Width = aViewport.Width;
		viewport.Height = aViewport.Height;
		viewport.MinDepth = aViewport.MinDepth;
		viewport.MaxDepth = aViewport.MaxDepth;

		mCurrentViewport = aViewport;
		assert(mCurrentGraphicsCommandListIndex > -1);

		mCommandListGraphics[mCurrentGraphicsCommandListIndex]->RSSetViewports(1, &viewport);
	}

	void ER_RHI_DX12::SetRect(const ER_RHI_Rect& rect)
	{
		mCurrentRect = rect;
		assert(mCurrentGraphicsCommandListIndex > -1);

		D3D12_RECT currentRect = { rect.left, rect.top, rect.right, rect.bottom };
		mCommandListGraphics[mCurrentGraphicsCommandListIndex]->RSSetScissorRects(1, &currentRect);
	}

	void ER_RHI_DX12::SetShader(ER_RHI_GPUShader* aShader)
	{
		assert(aShader);

		assert(mCurrentPSOState != ER_RHI_DX12_PSO_STATE::UNSET);

		ER_RHI_DX12_GPUShader* aDX12_Shader = static_cast<ER_RHI_DX12_GPUShader*>(aShader);
		assert(aDX12_Shader);

		ID3DBlob* blob = static_cast<ID3DBlob*>(aDX12_Shader->GetShaderObject());
		assert(blob);

		if (mCurrentPSOState == ER_RHI_DX12_PSO_STATE::GRAPHICS)
		{
			ER_RHI_DX12_GraphicsPSO& pso = mGraphicsPSONames.at(mCurrentGraphicsPSOName);

			switch (aShader->mShaderType)
			{
			case ER_RHI_SHADER_TYPE::ER_VERTEX:
				pso.SetVertexShader(blob->GetBufferPointer(), blob->GetBufferSize());
				break;
			case ER_RHI_SHADER_TYPE::ER_GEOMETRY:
				pso.SetGeometryShader(blob->GetBufferPointer(), blob->GetBufferSize());
				break;
			case ER_RHI_SHADER_TYPE::ER_TESSELLATION_HULL:
				pso.SetHullShader(blob->GetBufferPointer(), blob->GetBufferSize());
				break;
			case ER_RHI_SHADER_TYPE::ER_TESSELLATION_DOMAIN:
				pso.SetDomainShader(blob->GetBufferPointer(), blob->GetBufferSize());
				break;
			case ER_RHI_SHADER_TYPE::ER_PIXEL:
				pso.SetPixelShader(blob->GetBufferPointer(), blob->GetBufferSize());
				break;
			}
		}
		else
		{
			ER_RHI_DX12_ComputePSO& pso = mComputePSONames.at(mCurrentComputePSOName);
			pso.SetComputeShader(blob->GetBufferPointer(), blob->GetBufferSize());
		}
	}

	void ER_RHI_DX12::SetShaderResources(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_RHI_GPUResource*>& aSRVs, UINT startSlot /*= 0*/,
		ER_RHI_GPURootSignature* rs, int rootParamIndex, bool isComputeRS, bool skipAutomaticTransition)
	{
		int srvCount = static_cast<int>(aSRVs.size());
		assert(rs);
		assert(rootParamIndex >= 0 && rootParamIndex < rs->GetRootParameterCount());
		assert(srvCount > 0 && srvCount <= DX12_MAX_BOUND_SHADER_RESOURCE_VIEWS);
		//assert(srvCount <= rs->GetRootParameterSRVCount(rootParamIndex));
		assert(mDescriptorHeapManager);
		assert(mCurrentGraphicsCommandListIndex > -1);

		ER_RHI_DX12_GPUDescriptorHeap* gpuDescriptorHeap = mDescriptorHeapManager->GetGPUHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		ER_RHI_DX12_DescriptorHandle& srvHandle = gpuDescriptorHeap->GetHandleBlock(srvCount);
		for (int i = 0; i < srvCount; i++)
		{
			if (aSRVs[i])
			{
				if (aSRVs[i]->IsBuffer())
					gpuDescriptorHeap->AddToHandle(mDevice.Get(), srvHandle, static_cast<ER_RHI_DX12_GPUBuffer*>(aSRVs[i])->GetSRVDescriptorHandle());
				else
					gpuDescriptorHeap->AddToHandle(mDevice.Get(), srvHandle, static_cast<ER_RHI_DX12_GPUTexture*>(aSRVs[i])->GetSRVHandle());
			}
			else
				gpuDescriptorHeap->AddToHandle(mDevice.Get(), srvHandle, sNullSRV2DHandle);

		}

		if (!skipAutomaticTransition)
			TransitionResources(aSRVs, aShaderType == ER_RHI_SHADER_TYPE::ER_PIXEL ? ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, mCurrentGraphicsCommandListIndex);

		if (!isComputeRS)
			mCommandListGraphics[mCurrentGraphicsCommandListIndex]->SetGraphicsRootDescriptorTable(rootParamIndex, srvHandle.GetGPUHandle());
		else
			mCommandListGraphics[mCurrentGraphicsCommandListIndex]->SetComputeRootDescriptorTable(rootParamIndex, srvHandle.GetGPUHandle());

		//TODO compute queue
	}

	void ER_RHI_DX12::SetUnorderedAccessResources(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_RHI_GPUResource*>& aUAVs, UINT startSlot /*= 0*/,
		ER_RHI_GPURootSignature* rs, int rootParamIndex, bool isComputeRS, bool skipAutomaticTransition)
	{
		int uavCount = static_cast<int>(aUAVs.size());
		assert(rs);
		assert(rootParamIndex >= 0 && rootParamIndex < rs->GetRootParameterCount());
		assert(uavCount > 0 && uavCount <= DX12_MAX_BOUND_UNORDERED_ACCESS_VIEWS);
		//assert(uavCount <= rs->GetRootParameterUAVCount(rootParamIndex));
		assert(mDescriptorHeapManager);
		assert(mCurrentGraphicsCommandListIndex > -1);

		ER_RHI_DX12_GPUDescriptorHeap* gpuDescriptorHeap = mDescriptorHeapManager->GetGPUHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		ER_RHI_DX12_DescriptorHandle& uavHandle = gpuDescriptorHeap->GetHandleBlock(uavCount);
		for (int i = 0; i < uavCount; i++)
		{
			assert(aUAVs[i]);
			if (aUAVs[i]->IsBuffer())
				gpuDescriptorHeap->AddToHandle(mDevice.Get(), uavHandle, static_cast<ER_RHI_DX12_GPUBuffer*>(aUAVs[i])->GetUAVDescriptorHandle());
			else
				gpuDescriptorHeap->AddToHandle(mDevice.Get(), uavHandle, static_cast<ER_RHI_DX12_GPUTexture*>(aUAVs[i])->GetUAVHandle(startSlot));
		}

		if (!skipAutomaticTransition)
			TransitionResources(aUAVs, ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_UNORDERED_ACCESS, mCurrentGraphicsCommandListIndex);

		if (!isComputeRS)
			mCommandListGraphics[mCurrentGraphicsCommandListIndex]->SetGraphicsRootDescriptorTable(rootParamIndex, uavHandle.GetGPUHandle());
		else
			mCommandListGraphics[mCurrentGraphicsCommandListIndex]->SetComputeRootDescriptorTable(rootParamIndex, uavHandle.GetGPUHandle());

		//TODO compute queue
	}

	void ER_RHI_DX12::SetConstantBuffers(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_RHI_GPUBuffer*>& aCBs, UINT startSlot /*= 0*/,
		ER_RHI_GPURootSignature* rs, int rootParamIndex, bool isComputeRS)
	{
		int cbvCount = static_cast<int>(aCBs.size());
		assert(rs);
		assert(rootParamIndex >= 0 && rootParamIndex < rs->GetRootParameterCount());
		assert(cbvCount > 0 && cbvCount <= DX12_MAX_BOUND_CONSTANT_BUFFERS);
		//assert(cbvCount <= rs->GetRootParameterCBVCount(rootParamIndex));
		assert(mDescriptorHeapManager);
		assert(mCurrentGraphicsCommandListIndex > -1);

		ER_RHI_DX12_GPUDescriptorHeap* gpuDescriptorHeap = mDescriptorHeapManager->GetGPUHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		ER_RHI_DX12_DescriptorHandle& cbvHandle = gpuDescriptorHeap->GetHandleBlock(cbvCount);
		for (int i = 0; i < cbvCount; i++)
		{
			assert(aCBs[i]);
			gpuDescriptorHeap->AddToHandle(mDevice.Get(), cbvHandle, static_cast<ER_RHI_DX12_GPUBuffer*>(aCBs[i])->GetCBVDescriptorHandle());
		}

		if (!isComputeRS)
			mCommandListGraphics[mCurrentGraphicsCommandListIndex]->SetGraphicsRootDescriptorTable(rootParamIndex, cbvHandle.GetGPUHandle());
		else
			mCommandListGraphics[mCurrentGraphicsCommandListIndex]->SetComputeRootDescriptorTable(rootParamIndex, cbvHandle.GetGPUHandle());

		//TODO compute queue
	}

	void ER_RHI_DX12::SetSamplers(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_RHI_SAMPLER_STATE>& aSamplers, UINT startSlot /*= 0*/, ER_RHI_GPURootSignature* rs)
	{
		//assert(rs);
		//assert(rs->GetStaticSamplersCount() == aSamplers.size()); // we can do better checks (compare samplers), but thats ok for now

		//nothing here, because we bind static samplers in root signatures
	}

	void ER_RHI_DX12::SetInputLayout(ER_RHI_InputLayout* aIL)
	{
		assert(mCurrentPSOState == ER_RHI_DX12_PSO_STATE::GRAPHICS);
		assert(aIL);

		ER_RHI_DX12_GraphicsPSO& pso = mGraphicsPSONames.at(mCurrentGraphicsPSOName);
		pso.SetInputLayout(this, aIL->mInputElementDescriptionCount, aIL->mInputElementDescriptions);
	}

	void ER_RHI_DX12::SetEmptyInputLayout()
	{
		//is it possible on DX12 without PSO?
	}

	void ER_RHI_DX12::SetIndexBuffer(ER_RHI_GPUBuffer* aBuffer, UINT offset /*= 0*/)
	{
		assert(mCurrentGraphicsCommandListIndex > -1);

		assert(aBuffer);
		ER_RHI_DX12_GPUBuffer* buf = static_cast<ER_RHI_DX12_GPUBuffer*>(aBuffer);
		assert(buf);

		D3D12_INDEX_BUFFER_VIEW view = buf->GetIndexBufferView();
		mCommandListGraphics[mCurrentGraphicsCommandListIndex]->IASetIndexBuffer(&view);
	}

	void ER_RHI_DX12::SetVertexBuffers(const std::vector<ER_RHI_GPUBuffer*>& aVertexBuffers)
	{
		assert(mCurrentGraphicsCommandListIndex > -1);

		assert(aVertexBuffers.size() > 0 && aVertexBuffers.size() <= ER_RHI_MAX_BOUND_VERTEX_BUFFERS);
		if (aVertexBuffers.size() == 1)
		{
			assert(aVertexBuffers[0]);

			ER_RHI_DX12_GPUBuffer* buffer = static_cast<ER_RHI_DX12_GPUBuffer*>(aVertexBuffers[0]);
			assert(buffer);

			D3D12_VERTEX_BUFFER_VIEW view = buffer->GetVertexBufferView();
			mCommandListGraphics[mCurrentGraphicsCommandListIndex]->IASetVertexBuffers(0, 1, &view);
			if (mIsInstancedBufferBound)
			{
				mCommandListGraphics[mCurrentGraphicsCommandListIndex]->IASetVertexBuffers(1, 1, nullptr);
				mIsInstancedBufferBound = false;
			}
		}
		else //+ instance buffer
		{
			UINT strides[2] = { aVertexBuffers[0]->GetStride(), aVertexBuffers[1]->GetStride() };
			UINT offsets[2] = { 0, 0 };

			assert(aVertexBuffers[0]);
			assert(aVertexBuffers[1]);
			ER_RHI_DX12_GPUBuffer* vertexBuffer = static_cast<ER_RHI_DX12_GPUBuffer*>(aVertexBuffers[0]);
			ER_RHI_DX12_GPUBuffer* instanceBuffer = static_cast<ER_RHI_DX12_GPUBuffer*>(aVertexBuffers[1]);
			assert(vertexBuffer);
			assert(instanceBuffer);

			D3D12_VERTEX_BUFFER_VIEW views[2] = { vertexBuffer->GetVertexBufferView(), instanceBuffer->GetVertexBufferView() };
			mCommandListGraphics[mCurrentGraphicsCommandListIndex]->IASetVertexBuffers(0, 2, views);

			mIsInstancedBufferBound = true;
		}
	}

	void ER_RHI_DX12::SetTopologyType(ER_RHI_PRIMITIVE_TYPE aType)
	{
		assert(mCurrentGraphicsCommandListIndex > -1);
		mCommandListGraphics[mCurrentGraphicsCommandListIndex]->IASetPrimitiveTopology(GetTopology(aType));
	}

	void ER_RHI_DX12::SetRootSignature(ER_RHI_GPURootSignature* rs, bool isCompute)
	{
		assert(rs);
		assert(mCurrentGraphicsCommandListIndex > -1);
		if (!isCompute)
			mCommandListGraphics[mCurrentGraphicsCommandListIndex]->SetGraphicsRootSignature(static_cast<ER_RHI_DX12_GPURootSignature*>(rs)->GetSignature());
		else
			mCommandListGraphics[mCurrentGraphicsCommandListIndex]->SetComputeRootSignature(static_cast<ER_RHI_DX12_GPURootSignature*>(rs)->GetSignature());

		//TODO compute queue
	}

	void ER_RHI_DX12::SetRootConstant(UINT aConstant, UINT aRootIndex, UINT anOffset, bool isCompute)
	{
		if (!isCompute)
			mCommandListGraphics[mCurrentGraphicsCommandListIndex]->SetGraphicsRoot32BitConstant(aRootIndex, aConstant, anOffset);
		else
			mCommandListGraphics[mCurrentGraphicsCommandListIndex]->SetComputeRoot32BitConstant(aRootIndex, aConstant, anOffset);

		//TODO compute queue
	}

	void ER_RHI_DX12::SetTopologyTypeToPSO(const std::string& aName, ER_RHI_PRIMITIVE_TYPE aType)
	{
		if (mCurrentPSOState == ER_RHI_DX12_PSO_STATE::UNSET)
			return;

		assert(mCurrentPSOState == ER_RHI_DX12_PSO_STATE::GRAPHICS);
		assert(mCurrentGraphicsPSOName == aName);
		mGraphicsPSONames.at(aName).SetPrimitiveTopologyType(GetTopologyType(aType));
	}

	ER_RHI_PRIMITIVE_TYPE ER_RHI_DX12::GetCurrentTopologyType()
	{
		//TODO
		return ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_LINELIST;
	}

	void ER_RHI_DX12::SetGPUDescriptorHeap(ER_RHI_DESCRIPTOR_HEAP_TYPE aType, bool aReset)
	{
		assert(mDescriptorHeapManager);
		assert(mCurrentGraphicsCommandListIndex > -1);

		ER_RHI_DX12_GPUDescriptorHeap* gpuDescriptorHeap = mDescriptorHeapManager->GetGPUHeap(GetHeapType(aType));
		if (aReset)
			gpuDescriptorHeap->Reset();

		ID3D12DescriptorHeap* ppHeaps[] = { gpuDescriptorHeap->GetHeap() };
		mCommandListGraphics[mCurrentGraphicsCommandListIndex]->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	}

	void ER_RHI_DX12::SetGPUDescriptorHeapImGui(int cmdListIndex)
	{
		ID3D12DescriptorHeap* ppHeaps[] = { mImGuiDescriptorHeap.Get() };
		mCommandListGraphics[cmdListIndex]->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	}

	bool ER_RHI_DX12::IsPSOReady(const std::string& aName, bool isCompute)
	{
		if (!isCompute)
		{
			auto it = mGraphicsPSONames.find(aName);
			if (it != mGraphicsPSONames.end())
				return true;
			else
				return false;
		}
		else
		{
			auto it2 = mComputePSONames.find(aName);
			if (it2 != mComputePSONames.end())
				return true;
			else
				return false;
		}
	}

	void ER_RHI_DX12::InitializePSO(const std::string& aName, bool isCompute)
	{
		if (isCompute)
		{
			mComputePSONames.insert(std::make_pair(aName, ER_RHI_DX12_ComputePSO(aName)));
			mCurrentComputePSOName = aName;
			mCurrentPSOState = ER_RHI_DX12_PSO_STATE::COMPUTE;
		}
		else
		{
			mGraphicsPSONames.insert(std::make_pair(aName, ER_RHI_DX12_GraphicsPSO(aName)));
			mCurrentGraphicsPSOName = aName;
			mCurrentPSOState = ER_RHI_DX12_PSO_STATE::GRAPHICS;
			SetRasterizerState(ER_RHI_RASTERIZER_STATE::ER_BACK_CULLING); // set default RS to all gfx PSO on init
		}
	}

	void ER_RHI_DX12::SetRootSignatureToPSO(const std::string& aName, ER_RHI_GPURootSignature* rs, bool isCompute /*= false*/)
	{
		assert(rs);
		ER_RHI_DX12_GPURootSignature* rsDX12 = static_cast<ER_RHI_DX12_GPURootSignature*>(rs);
		assert(rsDX12);

		if (!isCompute)
		{
			assert(mCurrentGraphicsPSOName == aName);
			mGraphicsPSONames.at(aName).SetRootSignature(*rsDX12);
		}
		else
		{
			assert(mCurrentComputePSOName == aName);
			mComputePSONames.at(aName).SetRootSignature(*rsDX12);
		}
	}

	void ER_RHI_DX12::FinalizePSO(const std::string& aName, bool isCompute /*= false*/)
	{
		if (!isCompute)
		{
			assert(mCurrentGraphicsPSOName == aName);
			mGraphicsPSONames.at(aName).Finalize(mDevice.Get());
		}
		else
		{
			assert(mCurrentComputePSOName == aName);
			mComputePSONames.at(aName).Finalize(mDevice.Get());
		}
	}

	void ER_RHI_DX12::SetPSO(const std::string& aName, bool isCompute)
	{
		assert(mCurrentGraphicsCommandListIndex > -1);
		auto resetPSO = [&](const std::string& name, bool comp)
		{
			std::wstring msg = L"[ER Logger][ER_RHI_DX12] Could not find PSO to set, adding it now and trying to reset: " + ER_Utility::ToWideString(aName) + L'\n';
			ER_OUTPUT_LOG(msg.c_str());
			InitializePSO(name, comp);
			SetPSO(name, comp);
		};

		if (!isCompute)
		{
			auto it = mGraphicsPSONames.find(aName);
			if (it != mGraphicsPSONames.end())
			{
				if (mCurrentGraphicsPSOName == aName && mCurrentSetGraphicsPSOName == aName)
				{
					mCurrentPSOState = ER_RHI_DX12_PSO_STATE::GRAPHICS;
					return;
				}
				else
				{
					mCommandListGraphics[mCurrentGraphicsCommandListIndex]->SetPipelineState(it->second.GetPipelineStateObject());
					mCurrentGraphicsPSOName = it->first;
					mCurrentSetGraphicsPSOName = mCurrentGraphicsPSOName;
					mCurrentPSOState = ER_RHI_DX12_PSO_STATE::GRAPHICS;
				}
			}
			else
				resetPSO(aName, isCompute);
		}
		else
		{
			auto it = mComputePSONames.find(aName);
			if (it != mComputePSONames.end())
			{
				if (mCurrentComputePSOName == aName && mCurrentSetComputePSOName == aName)
				{
					mCurrentPSOState = ER_RHI_DX12_PSO_STATE::COMPUTE;
					return;
				}
				{
					mCommandListGraphics[mCurrentGraphicsCommandListIndex]->SetPipelineState(it->second.GetPipelineStateObject());
					mCurrentComputePSOName = it->first;
					mCurrentSetComputePSOName = mCurrentComputePSOName;
					mCurrentPSOState = ER_RHI_DX12_PSO_STATE::COMPUTE;
				}
			}
			else
				resetPSO(aName, isCompute);
		}
	}

	void ER_RHI_DX12::UnsetPSO()
	{
		mCurrentPSOState = ER_RHI_DX12_PSO_STATE::UNSET;
		mCurrentSetGraphicsPSOName = "";
		mCurrentSetComputePSOName = "";
	}

	void ER_RHI_DX12::TransitionResources(const std::vector<ER_RHI_GPUResource*>& aResources, const std::vector<ER_RHI_RESOURCE_STATE>& aStates, int cmdListIndex, bool isCopyQueue, int subresourceIndex)
	{
		int size = static_cast<int>(aResources.size());
		assert(size > 0 && size == aStates.size());
		std::vector<CD3DX12_RESOURCE_BARRIER> barriers;

		for (int i = 0; i < size; i++)
		{
			if (aStates[i] == ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_PIXEL_SHADER_RESOURCE &&
				aResources[i]->GetCurrentState() == ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				continue;

			if (aResources[i] && aResources[i]->GetCurrentState() != aStates[i])
			{
				barriers.emplace_back(CD3DX12_RESOURCE_BARRIER::Transition(static_cast<ID3D12Resource*>(aResources[i]->GetResource()), GetState(aResources[i]->GetCurrentState()), GetState(aStates[i]),
					subresourceIndex < 0 ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES : subresourceIndex)
				);
				aResources[i]->SetCurrentState(aStates[i]);
			}
		}

		if (barriers.size() > 0)
		{
			if (!isCopyQueue)
				mCommandListGraphics[cmdListIndex]->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
			else
				mCommandListCopy->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
		}
	}

	void ER_RHI_DX12::TransitionResources(const std::vector<ER_RHI_GPUResource*>& aResources, ER_RHI_RESOURCE_STATE aState, int cmdListIndex /*= 0*/, bool isCopyQueue, int subresourceIndex)
	{
		int size = static_cast<int>(aResources.size());
		std::vector<CD3DX12_RESOURCE_BARRIER> barriers;

		for (int i = 0; i < size; i++)
		{
			if (!aResources[i])
				continue;

			if (aState == ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_PIXEL_SHADER_RESOURCE &&
				aResources[i]->GetCurrentState() == ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				continue;

			if (aResources[i] && aResources[i]->GetCurrentState() != aState)
			{
				barriers.emplace_back(CD3DX12_RESOURCE_BARRIER::Transition(static_cast<ID3D12Resource*>(aResources[i]->GetResource()), GetState(aResources[i]->GetCurrentState()), GetState(aState),
					subresourceIndex < 0 ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES : subresourceIndex)
				);
				aResources[i]->SetCurrentState(aState);
			}
		}

		if (barriers.size() > 0)
		{
			if (!isCopyQueue)
				mCommandListGraphics[cmdListIndex]->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
			else
				mCommandListCopy->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
		}
	}

	void ER_RHI_DX12::TransitionMainRenderTargetToPresent(int cmdListIndex)
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(mMainRenderTarget[mBackBufferIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		mCommandListGraphics[cmdListIndex]->ResourceBarrier(1, &barrier);
	}

	void ER_RHI_DX12::UnbindRenderTargets()
	{
		assert(mCurrentGraphicsCommandListIndex > -1);
		mCommandListGraphics[mCurrentGraphicsCommandListIndex]->OMSetRenderTargets(0, nullptr, false, nullptr);
	}

	void ER_RHI_DX12::UpdateBuffer(ER_RHI_GPUBuffer* aBuffer, void* aData, int dataSize, bool updateForAllBackBuffers)
	{
		assert(aBuffer->GetSize() >= dataSize);

		ER_RHI_DX12_GPUBuffer* buffer = static_cast<ER_RHI_DX12_GPUBuffer*>(aBuffer);
		assert(buffer);

		buffer->Update(this, aData, dataSize, updateForAllBackBuffers);
	}

	void ER_RHI_DX12::InitImGui()
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 1;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		if (FAILED(mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mImGuiDescriptorHeap))))
			throw ER_CoreException("ER_RHI_DX12: Could not create a descriptor heap for ImGui");

		ImGui_ImplDX12_Init(mDevice.Get(), DX12_MAX_BACK_BUFFER_COUNT, mMainRTBufferFormat, mImGuiDescriptorHeap.Get(), mImGuiDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), mImGuiDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	}

	void ER_RHI_DX12::StartNewImGuiFrame()
	{
		ImGui_ImplDX12_NewFrame();
	}

	void ER_RHI_DX12::RenderDrawDataImGui(int cmdListIndex)
	{
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandListGraphics[cmdListIndex].Get());
	}

	void ER_RHI_DX12::ShutdownImGui()
	{
		ImGui_ImplDX12_Shutdown();
	}

	const D3D12_SAMPLER_DESC& ER_RHI_DX12::FindSamplerState(ER_RHI_SAMPLER_STATE aState)
	{
		auto it = mSamplerStates.find(aState);
		if (it != mSamplerStates.end())
		{
			return it->second;
		}
		else
		{
			return mEmptySampler;
			throw ER_CoreException("ER_RHI_DX12: Sampler state is not found.");
		}
	}

	DXGI_FORMAT ER_RHI_DX12::GetFormat(ER_RHI_FORMAT aFormat)
	{
		switch (aFormat)
		{
		case ER_RHI_FORMAT::ER_FORMAT_R32G32B32A32_TYPELESS:
			return DXGI_FORMAT_R32G32B32A32_TYPELESS;
		case ER_RHI_FORMAT::ER_FORMAT_R32G32B32A32_FLOAT:
			return DXGI_FORMAT_R32G32B32A32_FLOAT;
		case ER_RHI_FORMAT::ER_FORMAT_R32G32B32A32_UINT:
			return DXGI_FORMAT_R32G32B32A32_UINT;
		case ER_RHI_FORMAT::ER_FORMAT_R32G32B32_TYPELESS:
			return DXGI_FORMAT_R32G32B32_TYPELESS;
		case ER_RHI_FORMAT::ER_FORMAT_R32G32B32_FLOAT:
			return DXGI_FORMAT_R32G32B32_FLOAT;
		case ER_RHI_FORMAT::ER_FORMAT_R32G32B32_UINT:
			return DXGI_FORMAT_R32G32B32_UINT;
		case ER_RHI_FORMAT::ER_FORMAT_R16G16B16A16_TYPELESS:
			return DXGI_FORMAT_R16G16B16A16_TYPELESS;
		case ER_RHI_FORMAT::ER_FORMAT_R16G16B16A16_FLOAT:
			return DXGI_FORMAT_R16G16B16A16_FLOAT;
		case ER_RHI_FORMAT::ER_FORMAT_R16G16B16A16_UNORM:
			return DXGI_FORMAT_R16G16B16A16_UNORM;
		case ER_RHI_FORMAT::ER_FORMAT_R16G16B16A16_UINT:
			return DXGI_FORMAT_R16G16B16A16_UINT;
		case ER_RHI_FORMAT::ER_FORMAT_R32G32_TYPELESS:
			return DXGI_FORMAT_R32G32_TYPELESS;
		case ER_RHI_FORMAT::ER_FORMAT_R32G32_FLOAT:
			return DXGI_FORMAT_R32G32_FLOAT;
		case ER_RHI_FORMAT::ER_FORMAT_R32G32_UINT:
			return DXGI_FORMAT_R32G32_UINT;
		case ER_RHI_FORMAT::ER_FORMAT_R10G10B10A2_TYPELESS:
			return DXGI_FORMAT_R10G10B10A2_TYPELESS;
		case ER_RHI_FORMAT::ER_FORMAT_R10G10B10A2_UNORM:
			return DXGI_FORMAT_R10G10B10A2_UNORM;
		case ER_RHI_FORMAT::ER_FORMAT_R10G10B10A2_UINT:
			return DXGI_FORMAT_R10G10B10A2_UINT;
		case ER_RHI_FORMAT::ER_FORMAT_R11G11B10_FLOAT:
			return DXGI_FORMAT_R11G11B10_FLOAT;
		case ER_RHI_FORMAT::ER_FORMAT_R8G8B8A8_UNORM_sRGB:
			return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		case ER_RHI_FORMAT::ER_FORMAT_R8G8B8A8_TYPELESS:
			return DXGI_FORMAT_R8G8B8A8_TYPELESS;
		case ER_RHI_FORMAT::ER_FORMAT_R8G8B8A8_UNORM:
			return DXGI_FORMAT_R8G8B8A8_UNORM;
		case ER_RHI_FORMAT::ER_FORMAT_R8G8B8A8_UINT:
			return DXGI_FORMAT_R8G8B8A8_UINT;
		case ER_RHI_FORMAT::ER_FORMAT_R16G16_TYPELESS:
			return DXGI_FORMAT_R16G16_TYPELESS;
		case ER_RHI_FORMAT::ER_FORMAT_R16G16_FLOAT:
			return DXGI_FORMAT_R16G16_FLOAT;
		case ER_RHI_FORMAT::ER_FORMAT_R16G16_UNORM:
			return DXGI_FORMAT_R16G16_UNORM;
		case ER_RHI_FORMAT::ER_FORMAT_R16G16_UINT:
			return DXGI_FORMAT_R16G16_UINT;
		case ER_RHI_FORMAT::ER_FORMAT_R32_TYPELESS:
			return DXGI_FORMAT_R32_TYPELESS;
		case ER_RHI_FORMAT::ER_FORMAT_D32_FLOAT:
			return DXGI_FORMAT_D32_FLOAT;
		case ER_RHI_FORMAT::ER_FORMAT_R32_FLOAT:
			return DXGI_FORMAT_R32_FLOAT;
		case ER_RHI_FORMAT::ER_FORMAT_R32_UINT:
			return DXGI_FORMAT_R32_UINT;
		case ER_RHI_FORMAT::ER_FORMAT_D24_UNORM_S8_UINT:
			return DXGI_FORMAT_D24_UNORM_S8_UINT;
		case ER_RHI_FORMAT::ER_FORMAT_D16_UNORM:
			return DXGI_FORMAT_D16_UNORM;
		case ER_RHI_FORMAT::ER_FORMAT_R8G8_TYPELESS:
			return DXGI_FORMAT_R8G8_TYPELESS;
		case ER_RHI_FORMAT::ER_FORMAT_R8G8_UNORM:
			return DXGI_FORMAT_R8G8_UNORM;
		case ER_RHI_FORMAT::ER_FORMAT_R8G8_UINT:
			return DXGI_FORMAT_R8G8_UINT;
		case ER_RHI_FORMAT::ER_FORMAT_R16_TYPELESS:
			return DXGI_FORMAT_R16_TYPELESS;
		case ER_RHI_FORMAT::ER_FORMAT_R16_FLOAT:
			return DXGI_FORMAT_R16_FLOAT;
		case ER_RHI_FORMAT::ER_FORMAT_R16_UNORM:
			return DXGI_FORMAT_R16_UNORM;
		case ER_RHI_FORMAT::ER_FORMAT_R16_UINT:
			return DXGI_FORMAT_R16_UINT;
		case ER_RHI_FORMAT::ER_FORMAT_R8_TYPELESS:
			return DXGI_FORMAT_R8_TYPELESS;
		case ER_RHI_FORMAT::ER_FORMAT_R8_UNORM:
			return DXGI_FORMAT_R8_UNORM;
		case ER_RHI_FORMAT::ER_FORMAT_R8_UINT:
			return DXGI_FORMAT_R8_UINT;
		default:
			return DXGI_FORMAT_UNKNOWN;
		}
	}

	ER_RHI_RESOURCE_STATE ER_RHI_DX12::GetState(D3D12_RESOURCE_STATES aState)
	{
		switch (aState)
		{
		//case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON:
		//	return ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_COMMON;
		case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER:
			return ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_INDEX_BUFFER:
			return ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_INDEX_BUFFER;
		case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RENDER_TARGET:
			return ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_RENDER_TARGET;
		case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
			return ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_UNORDERED_ACCESS;
		case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_DEPTH_WRITE:
			return ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_DEPTH_WRITE;
		case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_DEPTH_READ:
			return ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_DEPTH_READ;
		case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE:
			return ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:
			return ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT:
			return ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_INDIRECT_ARGUMENT;
		case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST:
			return ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_COPY_DEST;
		case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_SOURCE:
			return ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_COPY_SOURCE;
		case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_GENERIC_READ:
			return ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_GENERIC_READ;
		case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_PRESENT:
			return ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_PRESENT;
		default:
			//throw ER_CoreException("ER_RHI_DX12: Could not convert the resource state!");
			return ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_COMMON;
		}
	}

	D3D12_RESOURCE_STATES ER_RHI_DX12::GetState(ER_RHI_RESOURCE_STATE aState)
	{
		switch (aState)
		{
		case ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_COMMON:
			return D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;
		case ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER:
			return D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		case ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_INDEX_BUFFER:
			return D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_INDEX_BUFFER;
		case ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_RENDER_TARGET:
			return D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RENDER_TARGET;
		case ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_UNORDERED_ACCESS:
			return D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		case ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_DEPTH_WRITE:
			return D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_DEPTH_WRITE;
		case ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_DEPTH_READ:
			return D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_DEPTH_READ;
		case ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE:
			return D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		case ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:
			return D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		case ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_INDIRECT_ARGUMENT:
			return D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
		case ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_COPY_DEST:
			return D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST;
		case ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_COPY_SOURCE:
			return D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_SOURCE;
		case ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_GENERIC_READ:
			return D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_GENERIC_READ;
		case ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_PRESENT:
			return D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_PRESENT;
		default:
			throw ER_CoreException("ER_RHI_DX12: Could not convert the resource state!");
			return D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;
		}
	}

	D3D12_SHADER_VISIBILITY ER_RHI_DX12::GetShaderVisibility(ER_RHI_SHADER_VISIBILITY aVis)
	{
		switch (aVis)
		{
		case ER_RHI_SHADER_VISIBILITY::ER_RHI_SHADER_VISIBILITY_ALL:
			return D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_ALL;
		case ER_RHI_SHADER_VISIBILITY::ER_RHI_SHADER_VISIBILITY_VERTEX:
			return D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_VERTEX;
		case ER_RHI_SHADER_VISIBILITY::ER_RHI_SHADER_VISIBILITY_HULL:
			return D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_HULL;
		case ER_RHI_SHADER_VISIBILITY::ER_RHI_SHADER_VISIBILITY_DOMAIN:
			return D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_DOMAIN;
		case ER_RHI_SHADER_VISIBILITY::ER_RHI_SHADER_VISIBILITY_GEOMETRY:
			return D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_GEOMETRY;
		case ER_RHI_SHADER_VISIBILITY::ER_RHI_SHADER_VISIBILITY_PIXEL:
			return D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_PIXEL;
		default:
		{
			assert(0);
			return D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_ALL;
		}
		}
	}

	D3D12_DESCRIPTOR_RANGE_TYPE ER_RHI_DX12::GetDescriptorRangeType(ER_RHI_DESCRIPTOR_RANGE_TYPE aDesc)
	{
		switch (aDesc)
		{
		case ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV:
			return D3D12_DESCRIPTOR_RANGE_TYPE::D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		case ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_UAV:
			return D3D12_DESCRIPTOR_RANGE_TYPE::D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		case ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV:
			return D3D12_DESCRIPTOR_RANGE_TYPE::D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		default:
		{
			assert(0);
			return D3D12_DESCRIPTOR_RANGE_TYPE::D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		}
		}
	}

	void ER_RHI_DX12::OnWindowSizeChanged(int width, int height)
	{
		WaitForGpuOnGraphicsFence();

		// Release resources that are tied to the swap chain and update fence values.
		for (UINT n = 0; n < DX12_MAX_BACK_BUFFER_COUNT; n++)
		{
			mMainRenderTarget[n].Reset();
			mFenceValuesGraphics[n] = mFenceValuesGraphics[mBackBufferIndex];
		}

		if (FAILED(mSwapChain->ResizeBuffers(DX12_MAX_BACK_BUFFER_COUNT, width, height, mMainRTBufferFormat, 0u)))
			throw ER_CoreException("ER_RHI_DX12: Could not resize swapchain!");

		CreateMainRenderTargetAndDepth(width, height);
	}

	D3D12_PRIMITIVE_TOPOLOGY_TYPE ER_RHI_DX12::GetTopologyType(ER_RHI_PRIMITIVE_TYPE aType)
	{
		switch (aType)
		{
		case ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_POINTLIST:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE::D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		case ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_LINELIST:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE::D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		case ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE::D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		case ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE::D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		case ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_CONTROL_POINT_PATCHLIST:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE::D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
		default:
		{
			assert(0);
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE::D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
		}
		}
	}

	ER_RHI_PRIMITIVE_TYPE ER_RHI_DX12::GetTopologyType(D3D_PRIMITIVE_TOPOLOGY aType)
	{
		switch (aType)
		{
		case D3D12_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
			return ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_POINTLIST;
		case D3D12_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_LINELIST:
			return ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_LINELIST;
		case D3D12_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
			return ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		case D3D12_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
			return ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		case D3D12_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST:
			return ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_CONTROL_POINT_PATCHLIST;
		default:
		{
			assert(0);
			return ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		}
		}
	}

	D3D12_PRIMITIVE_TOPOLOGY ER_RHI_DX12::GetTopology(ER_RHI_PRIMITIVE_TYPE aType)
	{
		switch (aType)
		{
		case ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_POINTLIST:
			return D3D12_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
		case ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_LINELIST:
			return D3D12_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_LINELIST;
		case ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
			return D3D12_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		case ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
			return D3D12_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		case ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_CONTROL_POINT_PATCHLIST:
			return D3D12_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST;
		default:
		{
			assert(0);
			return D3D12_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
		}
		}
	}

	D3D12_DESCRIPTOR_HEAP_TYPE ER_RHI_DX12::GetHeapType(ER_RHI_DESCRIPTOR_HEAP_TYPE aType)
	{
		switch (aType)
		{
		case ER_RHI_DESCRIPTOR_HEAP_TYPE::ER_RHI_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
			return D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		case ER_RHI_DESCRIPTOR_HEAP_TYPE::ER_RHI_DESCRIPTOR_HEAP_TYPE_SAMPLER:
			return D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		case ER_RHI_DESCRIPTOR_HEAP_TYPE::ER_RHI_DESCRIPTOR_HEAP_TYPE_RTV:
			return D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		case ER_RHI_DESCRIPTOR_HEAP_TYPE::ER_RHI_DESCRIPTOR_HEAP_TYPE_DSV:
			return D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		default:
		{
			assert(0);
			return D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		}
		}
	}

	DXGI_FORMAT ER_RHI_DX12::ChangeFormatToNonSRGB(DXGI_FORMAT aFormat)
	{
		switch (aFormat)
		{
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			return DXGI_FORMAT_R8G8B8A8_UNORM;
		case DXGI_FORMAT_BC1_UNORM_SRGB:
			return DXGI_FORMAT_BC1_UNORM;
		case DXGI_FORMAT_BC2_UNORM_SRGB:
			return DXGI_FORMAT_BC2_UNORM;
		case DXGI_FORMAT_BC3_UNORM_SRGB:
			return DXGI_FORMAT_BC3_UNORM;
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			return DXGI_FORMAT_BC7_UNORM;
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			return DXGI_FORMAT_B8G8R8A8_UNORM;

		default:
			{
				assert(0);
				return DXGI_FORMAT_UNKNOWN;
			}
		}
	}

	DXGI_FORMAT ER_RHI_DX12::ChangeFormatToUncompressed(DXGI_FORMAT aFormat)
	{
		switch (aFormat)
		{
		case DXGI_FORMAT_BC1_UNORM:
			return DXGI_FORMAT_R16G16B16A16_FLOAT;
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC7_UNORM:
			return DXGI_FORMAT_R32G32B32A32_FLOAT;

		default:
		{
			return aFormat;
		}
		}
	}

	bool ER_RHI_DX12::IsFormatSRGB(DXGI_FORMAT aFormat)
	{
		return 
			(aFormat == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) ||
			(aFormat == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) ||
			(aFormat == DXGI_FORMAT_BC1_UNORM_SRGB) ||
			(aFormat == DXGI_FORMAT_BC3_UNORM_SRGB) ||
			(aFormat == DXGI_FORMAT_BC2_UNORM_SRGB) || 
			(aFormat == DXGI_FORMAT_BC7_UNORM_SRGB);
	}

	void ER_RHI_DX12::CreateMainRenderTargetAndDepth(int width, int height)
	{
		// main RTV
		for (int i = 0; i < DX12_MAX_BACK_BUFFER_COUNT; i++)
		{
			mSwapChain->GetBuffer(i, IID_PPV_ARGS(mMainRenderTarget[i].GetAddressOf()));
			wchar_t name[25] = {};
			swprintf_s(name, L"Main Render target %u", i);
			mMainRenderTarget[i]->SetName(name);

			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
			rtvDesc.Format = mMainRTBufferFormat;
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

			CD3DX12_CPU_DESCRIPTOR_HANDLE mainRTVDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), i, mRTVDescriptorSize);
			mDevice->CreateRenderTargetView(mMainRenderTarget[i].Get(), &rtvDesc, mainRTVDescriptorHandle);
		}
		// Reset the index to the current back buffer.
		mBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

		// main DSV
		{
			CD3DX12_HEAP_PROPERTIES depthHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

			D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(mMainDepthBufferFormat, width, height, 1, 1);
			depthStencilDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			depthStencilDesc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

			D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
			depthOptimizedClearValue.Format = mMainDepthBufferFormat;
			depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
			depthOptimizedClearValue.DepthStencil.Stencil = 0;

			if (FAILED(mDevice->CreateCommittedResource(&depthHeapProperties, D3D12_HEAP_FLAG_NONE, &depthStencilDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthOptimizedClearValue, IID_PPV_ARGS(mMainDepthStencilTarget.ReleaseAndGetAddressOf()))))
				throw ER_CoreException("ER_RHI_DX12: Could not create comitted resource for main depth-stencil target");

			mMainDepthStencilTarget->SetName(L"Main Depth-Stencil target");

			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
			dsvDesc.Format = mMainDepthBufferFormat;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

			mDevice->CreateDepthStencilView(mMainDepthStencilTarget.Get(), &dsvDesc, mDSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		}
	}

	void ER_RHI_DX12::CreateSamplerStates()
	{
		float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		float black[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		// trilinear samplers
		D3D12_SAMPLER_DESC samplerStateDesc;
		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerStateDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerStateDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerStateDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerStateDesc.MipLODBias = 0;
		samplerStateDesc.MaxAnisotropy = 1;
		samplerStateDesc.MinLOD = -1000.0f;
		samplerStateDesc.MaxLOD = 1000.0f;
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, samplerStateDesc));

		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerStateDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		samplerStateDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		samplerStateDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_TRILINEAR_MIRROR, samplerStateDesc));

		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerStateDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerStateDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerStateDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_TRILINEAR_CLAMP, samplerStateDesc));

		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerStateDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerStateDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerStateDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		memcpy(samplerStateDesc.BorderColor, reinterpret_cast<FLOAT*>(&black), sizeof(FLOAT) * 4);
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_TRILINEAR_BORDER, samplerStateDesc));

		// bilinear samplers
		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		samplerStateDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerStateDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerStateDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_BILINEAR_WRAP, samplerStateDesc));

		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		samplerStateDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		samplerStateDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		samplerStateDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_BILINEAR_MIRROR, samplerStateDesc));

		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		samplerStateDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerStateDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerStateDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_BILINEAR_CLAMP, samplerStateDesc));

		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		samplerStateDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerStateDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerStateDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		memcpy(samplerStateDesc.BorderColor, reinterpret_cast<FLOAT*>(&black), sizeof(FLOAT) * 4);
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_BILINEAR_BORDER, samplerStateDesc));

		// anisotropic samplers
		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D12_FILTER_ANISOTROPIC;
		samplerStateDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerStateDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerStateDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_ANISOTROPIC_WRAP, samplerStateDesc));

		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D12_FILTER_ANISOTROPIC;
		samplerStateDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		samplerStateDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		samplerStateDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_ANISOTROPIC_MIRROR, samplerStateDesc));

		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D12_FILTER_ANISOTROPIC;
		samplerStateDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerStateDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerStateDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_ANISOTROPIC_CLAMP, samplerStateDesc));

		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D12_FILTER_ANISOTROPIC;
		samplerStateDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerStateDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerStateDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		memcpy(samplerStateDesc.BorderColor, reinterpret_cast<FLOAT*>(&black), sizeof(FLOAT) * 4);
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_ANISOTROPIC_BORDER, samplerStateDesc));

		// shadow sampler state
		samplerStateDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		samplerStateDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerStateDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerStateDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		memcpy(samplerStateDesc.BorderColor, reinterpret_cast<FLOAT*>(&white), sizeof(FLOAT) * 4);
		samplerStateDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_SHADOW_SS, samplerStateDesc));
	}

	void ER_RHI_DX12::CreateBlendStates()
	{
		D3D12_BLEND_DESC blendStateDescription;
		ZeroMemory(&blendStateDescription, sizeof(D3D12_BLEND_DESC));

		blendStateDescription.RenderTarget[0].BlendEnable = TRUE;
		blendStateDescription.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendStateDescription.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		blendStateDescription.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		blendStateDescription.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
		blendStateDescription.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		blendStateDescription.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		blendStateDescription.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
		mBlendStates.insert(std::make_pair(ER_RHI_BLEND_STATE::ER_ALPHA_BLEND, blendStateDescription));

		blendStateDescription.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		blendStateDescription.AlphaToCoverageEnable = TRUE;
		blendStateDescription.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		blendStateDescription.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		blendStateDescription.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		mBlendStates.insert(std::make_pair(ER_RHI_BLEND_STATE::ER_ALPHA_TO_COVERAGE, blendStateDescription));

		{
			D3D12_BLEND_DESC blendStateDescriptionMultiTarget = blendStateDescription;
			blendStateDescriptionMultiTarget.IndependentBlendEnable = TRUE;
			int i;
			for (i = 0; i < 4; i++)
				blendStateDescriptionMultiTarget.RenderTarget[i] = blendStateDescriptionMultiTarget.RenderTarget[0];
			for (i = 4; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
			{
				blendStateDescriptionMultiTarget.RenderTarget[i].BlendEnable = FALSE;
				blendStateDescriptionMultiTarget.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
			}
			mBlendStates.insert(std::make_pair(ER_RHI_BLEND_STATE::ER_ALPHA_TO_COVERAGE_4_TARGETS, blendStateDescriptionMultiTarget));
		}

		blendStateDescription.RenderTarget[0].BlendEnable = FALSE;
		blendStateDescription.AlphaToCoverageEnable = FALSE;
		mBlendStates.insert(std::make_pair(ER_RHI_BLEND_STATE::ER_NO_BLEND, blendStateDescription));
	}

	void ER_RHI_DX12::CreateRasterizerStates()
	{
		D3D12_RASTERIZER_DESC rasterizerStateDesc;
		ZeroMemory(&rasterizerStateDesc, sizeof(rasterizerStateDesc));
		rasterizerStateDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizerStateDesc.CullMode = D3D12_CULL_MODE_BACK;
		rasterizerStateDesc.DepthClipEnable = true;
		mRasterizerStates.insert(std::make_pair(ER_RHI_RASTERIZER_STATE::ER_BACK_CULLING, rasterizerStateDesc));

		ZeroMemory(&rasterizerStateDesc, sizeof(rasterizerStateDesc));
		rasterizerStateDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizerStateDesc.CullMode = D3D12_CULL_MODE_BACK;
		rasterizerStateDesc.FrontCounterClockwise = true;
		rasterizerStateDesc.DepthClipEnable = true;
		mRasterizerStates.insert(std::make_pair(ER_RHI_RASTERIZER_STATE::ER_FRONT_CULLING, rasterizerStateDesc));

		ZeroMemory(&rasterizerStateDesc, sizeof(rasterizerStateDesc));
		rasterizerStateDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizerStateDesc.CullMode = D3D12_CULL_MODE_NONE;
		rasterizerStateDesc.DepthClipEnable = true;
		mRasterizerStates.insert(std::make_pair(ER_RHI_RASTERIZER_STATE::ER_NO_CULLING, rasterizerStateDesc));

		ZeroMemory(&rasterizerStateDesc, sizeof(rasterizerStateDesc));
		rasterizerStateDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
		rasterizerStateDesc.CullMode = D3D12_CULL_MODE_NONE;
		rasterizerStateDesc.DepthClipEnable = true;
		mRasterizerStates.insert(std::make_pair(ER_RHI_RASTERIZER_STATE::ER_WIREFRAME, rasterizerStateDesc));

		ZeroMemory(&rasterizerStateDesc, sizeof(rasterizerStateDesc));
		rasterizerStateDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizerStateDesc.CullMode = D3D12_CULL_MODE_NONE;
		rasterizerStateDesc.DepthClipEnable = false;
		//rasterizerStateDesc.ScissorEnable = true; TODO!
		mRasterizerStates.insert(std::make_pair(ER_RHI_RASTERIZER_STATE::ER_NO_CULLING_NO_DEPTH_SCISSOR_ENABLED, rasterizerStateDesc));

		ZeroMemory(&rasterizerStateDesc, sizeof(rasterizerStateDesc));
		rasterizerStateDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizerStateDesc.CullMode = D3D12_CULL_MODE_BACK;
		rasterizerStateDesc.DepthClipEnable = false;
		rasterizerStateDesc.DepthBias = 0; //0.05f
		rasterizerStateDesc.SlopeScaledDepthBias = 3.0f;
		rasterizerStateDesc.FrontCounterClockwise = false;
		mRasterizerStates.insert(std::make_pair(ER_RHI_RASTERIZER_STATE::ER_SHADOW_RS, rasterizerStateDesc));
	}

	void ER_RHI_DX12::CreateDepthStencilStates()
	{
		D3D12_DEPTH_STENCIL_DESC depthStencilStateDesc;
		depthStencilStateDesc.DepthEnable = TRUE;
		depthStencilStateDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		depthStencilStateDesc.DepthFunc = D3D12_COMPARISON_FUNC_NEVER;
		depthStencilStateDesc.StencilEnable = FALSE;
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_READ_COMPARISON_NEVER, depthStencilStateDesc));

		// read only 
		depthStencilStateDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_READ_COMPARISON_LESS, depthStencilStateDesc));

		depthStencilStateDesc.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_READ_COMPARISON_EQUAL, depthStencilStateDesc));

		depthStencilStateDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_READ_COMPARISON_LESS_EQUAL, depthStencilStateDesc));

		depthStencilStateDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_READ_COMPARISON_GREATER, depthStencilStateDesc));

		depthStencilStateDesc.DepthFunc = D3D12_COMPARISON_FUNC_NOT_EQUAL;
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_READ_COMPARISON_NOT_EQUAL, depthStencilStateDesc));

		depthStencilStateDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_READ_COMPARISON_GREATER_EQUAL, depthStencilStateDesc));

		depthStencilStateDesc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_READ_COMPARISON_ALWAYS, depthStencilStateDesc));

		//write
		depthStencilStateDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		depthStencilStateDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_WRITE_COMPARISON_LESS, depthStencilStateDesc));

		depthStencilStateDesc.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_WRITE_COMPARISON_EQUAL, depthStencilStateDesc));

		depthStencilStateDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_WRITE_COMPARISON_LESS_EQUAL, depthStencilStateDesc));

		depthStencilStateDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_WRITE_COMPARISON_GREATER, depthStencilStateDesc));

		depthStencilStateDesc.DepthFunc = D3D12_COMPARISON_FUNC_NOT_EQUAL;
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_WRITE_COMPARISON_NOT_EQUAL, depthStencilStateDesc));

		depthStencilStateDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_WRITE_COMPARISON_GREATER_EQUAL, depthStencilStateDesc));

		depthStencilStateDesc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_WRITE_COMPARISON_ALWAYS, depthStencilStateDesc));

		depthStencilStateDesc.DepthEnable = FALSE;
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DISABLED, depthStencilStateDesc));
	}
}