#include "ER_RHI_DX11.h"
#include "ER_RHI_DX11_GPUBuffer.h"
#include "ER_RHI_DX11_GPUTexture.h"
#include "ER_RHI_DX11_GPUShader.h"
#include "..\..\ER_CoreException.h"
#include "..\..\ER_Utility.h"

#include "DirectXSH.h"

#define DX11_MAX_BOUND_RENDER_TARGETS_VIEWS 8
#define DX11_MAX_BOUND_SHADER_RESOURCE_VIEWS 64 
#define DX11_MAX_BOUND_UNORDERED_ACCESS_VIEWS 8 
#define DX11_MAX_BOUND_CONSTANT_BUFFERS 8 
#define DX11_MAX_BOUND_SAMPLERS 8 

namespace EveryRay_Core
{
	ER_RHI_DX11::ER_RHI_DX11()
	{
	}

	ER_RHI_DX11::~ER_RHI_DX11()
	{
		ReleaseObject(mMainRenderTargetView);
		ReleaseObject(mMainDepthStencilView);
		ReleaseObject(mSwapChain);
		ReleaseObject(mDepthStencilBuffer);

		ReleaseObject(BilinearWrapSS);
		ReleaseObject(BilinearMirrorSS);
		ReleaseObject(BilinearClampSS);
		ReleaseObject(BilinearBorderSS);
		ReleaseObject(TrilinearWrapSS);
		ReleaseObject(TrilinearMirrorSS);
		ReleaseObject(TrilinearClampSS);
		ReleaseObject(TrilinearBorderSS);
		ReleaseObject(AnisotropicWrapSS);
		ReleaseObject(AnisotropicMirrorSS);
		ReleaseObject(AnisotropicClampSS);
		ReleaseObject(AnisotropicBorderSS);
		ReleaseObject(ShadowSS);

		ReleaseObject(mNoBlendState);
		ReleaseObject(mAlphaToCoverageState);

		ReleaseObject(BackCullingRS);
		ReleaseObject(FrontCullingRS);
		ReleaseObject(NoCullingRS);
		ReleaseObject(WireframeRS);
		ReleaseObject(NoCullingNoDepthEnabledScissorRS);
		ReleaseObject(ShadowRS);

		ReleaseObject(DepthOnlyReadComparisonNeverDS);
		ReleaseObject(DepthOnlyReadComparisonLessDS);
		ReleaseObject(DepthOnlyReadComparisonEqualDS);
		ReleaseObject(DepthOnlyReadComparisonLessEqualDS);
		ReleaseObject(DepthOnlyReadComparisonGreaterDS);
		ReleaseObject(DepthOnlyReadComparisonNotEqualDS);
		ReleaseObject(DepthOnlyReadComparisonGreaterEqualDS);
		ReleaseObject(DepthOnlyReadComparisonAlwaysDS);
		ReleaseObject(DepthOnlyWriteComparisonNeverDS);
		ReleaseObject(DepthOnlyWriteComparisonLessDS);
		ReleaseObject(DepthOnlyWriteComparisonEqualDS);
		ReleaseObject(DepthOnlyWriteComparisonLessEqualDS);
		ReleaseObject(DepthOnlyWriteComparisonGreaterDS);
		ReleaseObject(DepthOnlyWriteComparisonNotEqualDS);
		ReleaseObject(DepthOnlyWriteComparisonGreaterEqualDS);
		ReleaseObject(DepthOnlyWriteComparisonAlwaysDS);

		if (mDirect3DDeviceContext)
			mDirect3DDeviceContext->ClearState();

		ReleaseObject(mDirect3DDeviceContext);
		ReleaseObject(mDirect3DDevice);
		ReleaseObject(mUserDefinedAnnotation);
	}

	bool ER_RHI_DX11::Initialize(HWND windowHandle, UINT width, UINT height, bool isFullscreen, bool isReset)
	{
		mAPI = ER_GRAPHICS_API::DX11;
		assert(width > 0 && height > 0);

		HRESULT hr;
		UINT createDeviceFlags = 0;

#if defined(DEBUG) || defined(_DEBUG)  
		createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

		D3D_FEATURE_LEVEL featureLevels[] = {
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0
		};

		ID3D11Device* direct3DDevice = nullptr;
		ID3D11DeviceContext* direct3DDeviceContext = nullptr;
		if (FAILED(hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION, &direct3DDevice, &mFeatureLevel, &direct3DDeviceContext)))
			throw ER_CoreException("ER_RHI_DX11: D3D11CreateDevice() failed", hr);

		if (FAILED(hr = direct3DDevice->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&mDirect3DDevice))))
			throw ER_CoreException("ER_RHI_DX11: ID3D11Device::QueryInterface() failed", hr);

		if (FAILED(hr = direct3DDeviceContext->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&mDirect3DDeviceContext))))
			throw ER_CoreException("ER_RHI_DX11: ID3D11Device::QueryInterface() failed", hr);

		if (FAILED(hr = direct3DDeviceContext->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), reinterpret_cast<void**>(&mUserDefinedAnnotation))))
			throw ER_CoreException("ER_RHI_DX11: ID3D11Device::QueryInterface() failed", hr);

		ReleaseObject(direct3DDevice);
		ReleaseObject(direct3DDeviceContext);

		DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
		ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
		swapChainDesc.Width = width;
		swapChainDesc.Height = height;
		swapChainDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 1;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		IDXGIDevice* dxgiDevice = nullptr;
		if (FAILED(hr = mDirect3DDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice))))
		{
			throw ER_CoreException("ER_RHI_DX11: ID3D11Device::QueryInterface() failed", hr);
		}

		IDXGIAdapter* dxgiAdapter = nullptr;
		if (FAILED(hr = dxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&dxgiAdapter))))
		{
			ReleaseObject(dxgiDevice);
			throw ER_CoreException("ER_RHI_DX11: IDXGIDevice::GetParent() failed retrieving adapter.", hr);
		}

		IDXGIFactory2* dxgiFactory = nullptr;
		if (FAILED(hr = dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory))))
		{
			ReleaseObject(dxgiDevice);
			ReleaseObject(dxgiAdapter);
			throw ER_CoreException("ER_RHI_DX11: IDXGIAdapter::GetParent() failed retrieving factory.", hr);
		}

		DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullScreenDesc;
		ZeroMemory(&fullScreenDesc, sizeof(fullScreenDesc));
		fullScreenDesc.RefreshRate.Numerator = DefaultFrameRate;
		fullScreenDesc.RefreshRate.Denominator = 1;
		fullScreenDesc.Windowed = !isFullscreen;

		if (FAILED(hr = dxgiFactory->CreateSwapChainForHwnd(dxgiDevice, windowHandle, &swapChainDesc, &fullScreenDesc, nullptr, &mSwapChain)))
		{
			ReleaseObject(dxgiDevice);
			ReleaseObject(dxgiAdapter);
			ReleaseObject(dxgiFactory);
			throw ER_CoreException("ER_RHI_DX11: IDXGIDevice::CreateSwapChainForHwnd() failed.", hr);
		}

		ReleaseObject(dxgiDevice);
		ReleaseObject(dxgiAdapter);
		ReleaseObject(dxgiFactory);

		ID3D11Texture2D* backBuffer;
		if (FAILED(hr = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer))))
		{
			throw ER_CoreException("ER_RHI_DX11: IDXGISwapChain::GetBuffer() failed.", hr);
		}

		backBuffer->GetDesc(&mBackBufferDesc);


		if (FAILED(hr = mDirect3DDevice->CreateRenderTargetView(backBuffer, nullptr, &mMainRenderTargetView)))
		{
			ReleaseObject(backBuffer);
			throw ER_CoreException("ER_RHI_DX11: Failed to create main RenderTargetView.", hr);
		}

		ReleaseObject(backBuffer);

		{
			D3D11_TEXTURE2D_DESC depthStencilDesc;
			ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));
			depthStencilDesc.Width = width;
			depthStencilDesc.Height = height;
			depthStencilDesc.MipLevels = 1;
			depthStencilDesc.ArraySize = 1;
			depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
			depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
			depthStencilDesc.SampleDesc.Count = 1;
			depthStencilDesc.SampleDesc.Quality = 0;

			if (FAILED(hr = mDirect3DDevice->CreateTexture2D(&depthStencilDesc, nullptr, &mDepthStencilBuffer)))
			{
				throw ER_CoreException("ER_RHI_DX11: Failed to create main DepthStencil Texture2D.", hr);
			}

			if (FAILED(hr = mDirect3DDevice->CreateDepthStencilView(mDepthStencilBuffer, nullptr, &mMainDepthStencilView)))
			{
				throw ER_CoreException("ER_RHI_DX11: Failed to create main DepthStencilView.", hr);
			}
		}

		mMainViewport.TopLeftX = 0.0f;
		mMainViewport.TopLeftY = 0.0f;
		mMainViewport.Width = static_cast<float>(width);
		mMainViewport.Height = static_cast<float>(height);
		mMainViewport.MinDepth = 0.0f;
		mMainViewport.MaxDepth = 1.0f;

		D3D11_VIEWPORT viewport;
		viewport.TopLeftX = mMainViewport.TopLeftX;
		viewport.TopLeftY = mMainViewport.TopLeftY;
		viewport.Width = mMainViewport.Width;
		viewport.Height = mMainViewport.Height;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		mDirect3DDeviceContext->OMSetRenderTargets(1, &mMainRenderTargetView, mMainDepthStencilView);
		mDirect3DDeviceContext->RSSetViewports(1, &viewport);

		CreateSamplerStates();
		CreateRasterizerStates();
		CreateDepthStencilStates();
		CreateBlendStates();

		return true;
	}

	void ER_RHI_DX11::ClearMainRenderTarget(float colors[4])
	{
		mDirect3DDeviceContext->ClearRenderTargetView(mMainRenderTargetView, colors);
	}

	void ER_RHI_DX11::ClearMainDepthStencilTarget(float depth, UINT stencil /*= 0*/)
	{
		mDirect3DDeviceContext->ClearDepthStencilView(mMainDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}

	void ER_RHI_DX11::ClearRenderTarget(ER_RHI_GPUTexture* aRenderTarget, float colors[4], int rtvArrayIndex)
	{
		assert(aRenderTarget);

		ID3D11RenderTargetView* rtv;
		if (rtvArrayIndex > 0)
			rtv = static_cast<ID3D11RenderTargetView*>(aRenderTarget->GetRTV(rtvArrayIndex));
		else
			rtv = static_cast<ID3D11RenderTargetView*>(aRenderTarget->GetRTV());
		mDirect3DDeviceContext->ClearRenderTargetView(rtv, colors);
	}

	void ER_RHI_DX11::ClearDepthStencilTarget(ER_RHI_GPUTexture* aDepthTarget, float depth, UINT stencil /*= 0*/)
	{
		assert(aDepthTarget);
		ID3D11DepthStencilView* pDepthStencilView = static_cast<ID3D11DepthStencilView*>(aDepthTarget->GetDSV());
		assert(pDepthStencilView);
		mDirect3DDeviceContext->ClearDepthStencilView(pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, stencil);
	}

	void ER_RHI_DX11::ClearUAV(ER_RHI_GPUResource* aRenderTarget, float colors[4])
	{
		assert(aRenderTarget);
		ID3D11UnorderedAccessView* pUnorderedAccessView = static_cast<ID3D11UnorderedAccessView*>(aRenderTarget->GetUAV());
		assert(pUnorderedAccessView);
		mDirect3DDeviceContext->ClearUnorderedAccessViewFloat(pUnorderedAccessView, colors);
	}

	void ER_RHI_DX11::CreateInputLayout(ER_RHI_InputLayout* aOutInputLayout, ER_RHI_INPUT_ELEMENT_DESC* inputElementDescriptions, UINT inputElementDescriptionCount, const void* shaderBytecodeWithInputSignature, UINT byteCodeLength)
	{
		assert(inputElementDescriptions);
		assert(aOutInputLayout);

		D3D11_INPUT_ELEMENT_DESC* descriptions = new D3D11_INPUT_ELEMENT_DESC[inputElementDescriptionCount];
		for (UINT i = 0; i < inputElementDescriptionCount; i++)
		{
			descriptions[i].SemanticName = inputElementDescriptions[i].SemanticName;
			descriptions[i].SemanticIndex = inputElementDescriptions[i].SemanticIndex;
			descriptions[i].Format = GetFormat(inputElementDescriptions[i].Format);
			descriptions[i].InputSlot = inputElementDescriptions[i].InputSlot;
			descriptions[i].AlignedByteOffset = inputElementDescriptions[i].AlignedByteOffset;
			descriptions[i].InputSlotClass = inputElementDescriptions[i].IsPerVertex ? D3D11_INPUT_PER_VERTEX_DATA : D3D11_INPUT_PER_INSTANCE_DATA;
			descriptions[i].InstanceDataStepRate = inputElementDescriptions[i].InstanceDataStepRate;
		}
		mDirect3DDevice->CreateInputLayout(descriptions, inputElementDescriptionCount, shaderBytecodeWithInputSignature, byteCodeLength, &(static_cast<ER_RHI_DX11_InputLayout*>(aOutInputLayout)->mInputLayout));
		DeleteObject(descriptions);
	}

	ER_RHI_InputLayout* ER_RHI_DX11::CreateInputLayout(ER_RHI_INPUT_ELEMENT_DESC* inputElementDescriptions, UINT inputElementDescriptionCount)
	{
		return new ER_RHI_DX11_InputLayout(inputElementDescriptions, inputElementDescriptionCount);
	}

	ER_RHI_GPUShader* ER_RHI_DX11::CreateGPUShader()
	{
		return new ER_RHI_DX11_GPUShader();
	}

	ER_RHI_GPUBuffer* ER_RHI_DX11::CreateGPUBuffer(const std::string& aDebugName)
	{
		return new ER_RHI_DX11_GPUBuffer(aDebugName);
	}

	ER_RHI_GPUTexture* ER_RHI_DX11::CreateGPUTexture(const std::wstring& aDebugName)
	{
		return new ER_RHI_DX11_GPUTexture(aDebugName);
	}

	void ER_RHI_DX11::CreateTexture(ER_RHI_GPUTexture* aOutTexture, UINT width, UINT height, UINT samples, ER_RHI_FORMAT format, ER_RHI_BIND_FLAG bindFlags /*= ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET*/, int mip /*= 1*/, int depth /*= -1*/, int arraySize /*= 1*/, bool isCubemap /*= false*/, int cubemapArraySize /*= -1*/)
	{
		assert(aOutTexture);
		aOutTexture->CreateGPUTextureResource(this, width, height, samples, format, bindFlags, mip, depth, arraySize, isCubemap, cubemapArraySize);
	}

	void ER_RHI_DX11::CreateTexture(ER_RHI_GPUTexture* aOutTexture, const std::string& aPath, bool isFullPath /*= false*/)
	{
		assert(aOutTexture);
		aOutTexture->CreateGPUTextureResource(this, aPath, isFullPath);
	}

	void ER_RHI_DX11::CreateTexture(ER_RHI_GPUTexture* aOutTexture, const std::wstring& aPath, bool isFullPath /*= false*/)
	{
		assert(aOutTexture);
		aOutTexture->CreateGPUTextureResource(this, aPath, isFullPath);
	}

	void ER_RHI_DX11::CreateBuffer(ER_RHI_GPUBuffer* aOutBuffer, void* aData, UINT objectsCount, UINT byteStride, bool isDynamic /*= false*/, ER_RHI_BIND_FLAG bindFlags /*= 0*/, UINT cpuAccessFlags /*= 0*/, ER_RHI_RESOURCE_MISC_FLAG miscFlags /*= 0*/, ER_RHI_FORMAT format /*= ER_FORMAT_UNKNOWN*/)
	{
		assert(aOutBuffer);
		aOutBuffer->CreateGPUBufferResource(this, aData, objectsCount, byteStride, isDynamic, bindFlags, cpuAccessFlags, miscFlags, format);
	}

	void ER_RHI_DX11::CopyBuffer(ER_RHI_GPUBuffer* aDestBuffer, ER_RHI_GPUBuffer* aSrcBuffer, int cmdListIndex, bool isInCopyQueue)
	{
		assert(aDestBuffer);
		assert(aSrcBuffer);

		ID3D11Resource* dstResource = static_cast<ID3D11Resource*>(aDestBuffer->GetBuffer());
		ID3D11Resource* srcResource = static_cast<ID3D11Resource*>(aSrcBuffer->GetBuffer());
		
		assert(dstResource);
		assert(srcResource);

		mDirect3DDeviceContext->CopyResource(dstResource, srcResource);
	}

	void ER_RHI_DX11::BeginBufferRead(ER_RHI_GPUBuffer* aBuffer, void** output)
	{
		assert(aBuffer);
		assert(!mIsContextReadingBuffer);

		ER_RHI_DX11_GPUBuffer* buffer = static_cast<ER_RHI_DX11_GPUBuffer*>(aBuffer);
		assert(buffer);

		D3D11_MAPPED_SUBRESOURCE mappedResource;
		buffer->Map(this, D3D11_MAP_READ, &mappedResource);
		*output = mappedResource.pData;
		mIsContextReadingBuffer = true;
	}

	void ER_RHI_DX11::EndBufferRead(ER_RHI_GPUBuffer* aBuffer)
	{
		ER_RHI_DX11_GPUBuffer* buffer = static_cast<ER_RHI_DX11_GPUBuffer*>(aBuffer);
		assert(buffer);

		buffer->Unmap(this);
		mIsContextReadingBuffer = false;
	}

	void ER_RHI_DX11::CopyGPUTextureSubresourceRegion(ER_RHI_GPUResource* aDestBuffer, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ER_RHI_GPUResource* aSrcBuffer, UINT SrcSubresource, bool isInCopyQueueOrSkipTransitions)
	{
		assert(aDestBuffer);
		assert(aSrcBuffer);

		ER_RHI_DX11_GPUTexture* dstbuffer = static_cast<ER_RHI_DX11_GPUTexture*>(aDestBuffer);
		assert(dstbuffer);
		ER_RHI_DX11_GPUTexture* srcbuffer = static_cast<ER_RHI_DX11_GPUTexture*>(aSrcBuffer);
		assert(srcbuffer);

		if (dstbuffer->GetTexture2D() && srcbuffer->GetTexture2D())
			mDirect3DDeviceContext->CopySubresourceRegion(dstbuffer->GetTexture2D(), DstSubresource, DstX, DstY, DstZ, srcbuffer->GetTexture2D(), SrcSubresource, NULL);
		else if (dstbuffer->GetTexture3D() && srcbuffer->GetTexture3D())
			mDirect3DDeviceContext->CopySubresourceRegion(dstbuffer->GetTexture3D(), DstSubresource, DstX, DstY, DstZ, srcbuffer->GetTexture3D(), SrcSubresource, NULL);
		else
			throw ER_CoreException("ER_RHI_DX11:: One of the resources is NULL during CopyGPUTextureSubresourceRegion()");
	}

	void ER_RHI_DX11::Draw(UINT VertexCount)
	{
		assert(VertexCount > 0);
		mDirect3DDeviceContext->DrawInstanced(VertexCount, 1, 0, 0);
	}

	void ER_RHI_DX11::DrawIndexed(UINT IndexCount)
	{
		assert(IndexCount > 0);
		mDirect3DDeviceContext->DrawIndexedInstanced(IndexCount, 1, 0, 0, 0);
	}

	void ER_RHI_DX11::DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
	{
		assert(VertexCountPerInstance > 0);
		assert(InstanceCount > 0);

		mDirect3DDeviceContext->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
	}

	void ER_RHI_DX11::DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
	{
		assert(IndexCountPerInstance > 0);
		assert(InstanceCount > 0);

		mDirect3DDeviceContext->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
	}

	void ER_RHI_DX11::Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)
	{
		mDirect3DDeviceContext->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	}

	void ER_RHI_DX11::GenerateMips(ER_RHI_GPUTexture* aTexture, ER_RHI_GPUTexture* aSRGBTexture)
	{
		assert(aTexture);
		ID3D11ShaderResourceView* pShaderResourceView = static_cast<ID3D11ShaderResourceView*>(aTexture->GetSRV());
		assert(pShaderResourceView);

		mDirect3DDeviceContext->GenerateMips(pShaderResourceView);
	}

	void ER_RHI_DX11::PresentGraphics()
	{
		HRESULT hr = mSwapChain->Present(0, 0);
		if (FAILED(hr))
			throw ER_CoreException("ER_RHI_DX11: IDXGISwapChain::Present() failed.", hr);
	}

	bool ER_RHI_DX11::ProjectCubemapToSH(ER_RHI_GPUTexture* aTexture, UINT order, float* resultR, float* resultG, float* resultB)
	{
		assert(aTexture);

		ER_RHI_DX11_GPUTexture* tex = static_cast<ER_RHI_DX11_GPUTexture*>(aTexture);
		assert(tex);

		return !(FAILED(DirectX::SHProjectCubeMap(mDirect3DDeviceContext, order, tex->GetTexture2D(), resultR, resultG, resultB)));
	}

	void ER_RHI_DX11::SaveGPUTextureToFile(ER_RHI_GPUTexture* aTexture, const std::wstring& aPathName)
	{
		assert(aTexture);

		ER_RHI_DX11_GPUTexture* tex = static_cast<ER_RHI_DX11_GPUTexture*>(aTexture);
		assert(tex);

		DirectX::ScratchImage tempImage;
		HRESULT res = DirectX::CaptureTexture(mDirect3DDevice, mDirect3DDeviceContext, tex->GetTexture2D(), tempImage);
		if (FAILED(res))
			throw ER_CoreException("Failed to capture a probe texture when saving!", res);

		res = DirectX::SaveToDDSFile(tempImage.GetImages(), tempImage.GetImageCount(), tempImage.GetMetadata(), DDS_FLAGS_NONE, aPathName.c_str());
		if (FAILED(res))
		{
			throw ER_CoreException("Failed to save a texture to a file: ");
			//std::string str(aPathName.begin(), aPathName.end());
			//std::string msg = "Failed to save a texture to a file: " + str;
			//throw ER_CoreException(msg.c_str());
		}
	}

	void ER_RHI_DX11::SetMainRenderTargets(int cmdListIndex)
	{
		mDirect3DDeviceContext->OMSetRenderTargets(1, &mMainRenderTargetView, NULL);
	}

	void ER_RHI_DX11::SetRenderTargets(const std::vector<ER_RHI_GPUTexture*>& aRenderTargets, ER_RHI_GPUTexture* aDepthTarget /*= nullptr*/, ER_RHI_GPUTexture* aUAV /*= nullptr*/, int rtvArrayIndex)
	{
		if (!aUAV)
		{
			if (rtvArrayIndex > 0)
				assert(aRenderTargets.size() == 1);

			assert(aRenderTargets.size() > 0);
			assert(aRenderTargets.size() <= DX11_MAX_BOUND_RENDER_TARGETS_VIEWS);

			ID3D11RenderTargetView* RTVs[DX11_MAX_BOUND_RENDER_TARGETS_VIEWS] = {};
			UINT rtCount = static_cast<UINT>(aRenderTargets.size());
			for (UINT i = 0; i < rtCount; i++)
			{
				assert(aRenderTargets[i]);
				if (rtvArrayIndex > 0)
					RTVs[i] = static_cast<ID3D11RenderTargetView*>(aRenderTargets[i]->GetRTV(rtvArrayIndex));
				else
					RTVs[i] = static_cast<ID3D11RenderTargetView*>(aRenderTargets[i]->GetRTV());
				assert(RTVs[i]);
			}
			if (aDepthTarget)
			{
				ID3D11DepthStencilView* DSV = static_cast<ID3D11DepthStencilView*>(aDepthTarget->GetDSV());
				mDirect3DDeviceContext->OMSetRenderTargets(rtCount, RTVs, DSV);
			}
			else
				mDirect3DDeviceContext->OMSetRenderTargets(rtCount, RTVs, NULL);

		}
		else
		{
			ID3D11RenderTargetView* nullRTVs[1] = { NULL };
			ID3D11UnorderedAccessView* UAVs[1] = { static_cast<ID3D11UnorderedAccessView*>(aUAV->GetUAV()) };
			assert(UAVs[0]);
			if (aDepthTarget)
			{
				ID3D11DepthStencilView* dsv = static_cast<ID3D11DepthStencilView*>(aDepthTarget->GetDSV());
				assert(dsv);
				mDirect3DDeviceContext->OMSetRenderTargetsAndUnorderedAccessViews(0, nullRTVs, dsv, 0, 1, UAVs, NULL);
			}
			else
				mDirect3DDeviceContext->OMSetRenderTargetsAndUnorderedAccessViews(0, nullRTVs, NULL, 0, 1, UAVs, NULL);
		}
	}

	void ER_RHI_DX11::SetDepthTarget(ER_RHI_GPUTexture* aDepthTarget)
	{
		ID3D11RenderTargetView* nullRTVs[1] = { NULL };

		assert(aDepthTarget);
		ID3D11DepthStencilView* dsv = static_cast<ID3D11DepthStencilView*>(aDepthTarget->GetDSV());
		assert(dsv);

		mDirect3DDeviceContext->OMSetRenderTargets(1, nullRTVs, dsv);
	}

	void ER_RHI_DX11::SetDepthStencilState(ER_RHI_DEPTH_STENCIL_STATE aDS, UINT stencilRef)
	{
		if (aDS == ER_DISABLED)
		{
			mDirect3DDeviceContext->OMSetDepthStencilState(nullptr, 0xffffffff);
			return;
		}

		auto it = mDepthStates.find(aDS);
		if (it != mDepthStates.end())
			mDirect3DDeviceContext->OMSetDepthStencilState(it->second, stencilRef);
		else
			throw ER_CoreException("ER_RHI_DX11: DepthStencil state is not found.");
	}

	void ER_RHI_DX11::SetBlendState(ER_RHI_BLEND_STATE aBS, const float BlendFactor[4], UINT SampleMask)
	{
		if (aBS == ER_RHI_BLEND_STATE::ER_NO_BLEND)
		{
			mDirect3DDeviceContext->OMSetBlendState(nullptr, NULL, 0xffffffff);
			return;
		}

		auto it = mBlendStates.find(aBS);
		if (it != mBlendStates.end())
		{
			mCurrentBS = aBS;
			mDirect3DDeviceContext->OMSetBlendState(it->second, BlendFactor, SampleMask);
		}
		else
			throw ER_CoreException("ER_RHI_DX11: Blend state is not found.");
	}

	void ER_RHI_DX11::SetRasterizerState(ER_RHI_RASTERIZER_STATE aRS)
	{
		auto it = mRasterizerStates.find(aRS);
		if (it != mRasterizerStates.end())
		{
			mCurrentRS = aRS;
			mDirect3DDeviceContext->RSSetState(it->second);
		}
		else
			throw ER_CoreException("ER_RHI_DX11: Rasterizer state is not found.");
	}

	void ER_RHI_DX11::SetViewport(const ER_RHI_Viewport& aViewport)
	{
		D3D11_VIEWPORT viewport;
		viewport.TopLeftX = aViewport.TopLeftX;
		viewport.TopLeftY = aViewport.TopLeftY;
		viewport.Width = aViewport.Width;
		viewport.Height = aViewport.Height;
		viewport.MinDepth = aViewport.MinDepth;
		viewport.MaxDepth = aViewport.MaxDepth;

		mCurrentViewport = aViewport;

		mDirect3DDeviceContext->RSSetViewports(1, &viewport);
	}

	void ER_RHI_DX11::SetRect(const ER_RHI_Rect& rect)
	{
		D3D11_RECT currentRect = { rect.left, rect.top, rect.right, rect.bottom };
		mDirect3DDeviceContext->RSSetScissorRects(1, &currentRect);
	}

	void ER_RHI_DX11::SetShaderResources(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_RHI_GPUResource*>& aSRVs, UINT startSlot,
		ER_RHI_GPURootSignature* rs, int rootParamIndex, bool isComputeRS, bool skipAutomaticTransition)
	{
		assert(aSRVs.size() > 0);
		assert(aSRVs.size() <= DX11_MAX_BOUND_SHADER_RESOURCE_VIEWS);

		ID3D11ShaderResourceView* SRs[DX11_MAX_BOUND_SHADER_RESOURCE_VIEWS] = {};
		UINT srCount = static_cast<UINT>(aSRVs.size());
		for (UINT i = 0; i < srCount; i++)
		{
			if (!aSRVs[i])
				SRs[i] = nullptr;
			else
			{
				SRs[i] = static_cast<ID3D11ShaderResourceView*>(aSRVs[i]->GetSRV());
				assert(SRs[i]);
			}
		}

		switch (aShaderType)
		{
		case ER_RHI_SHADER_TYPE::ER_VERTEX:
			mDirect3DDeviceContext->VSSetShaderResources(startSlot, srCount, SRs);
			break;
		case ER_RHI_SHADER_TYPE::ER_GEOMETRY:
			mDirect3DDeviceContext->GSSetShaderResources(startSlot, srCount, SRs);
			break;
		case ER_RHI_SHADER_TYPE::ER_TESSELLATION_HULL:
			mDirect3DDeviceContext->HSSetShaderResources(startSlot, srCount, SRs);
			break;
		case ER_RHI_SHADER_TYPE::ER_TESSELLATION_DOMAIN:
			mDirect3DDeviceContext->DSSetShaderResources(startSlot, srCount, SRs);
			break;
		case ER_RHI_SHADER_TYPE::ER_PIXEL:
			mDirect3DDeviceContext->PSSetShaderResources(startSlot, srCount, SRs);
			break;
		case ER_RHI_SHADER_TYPE::ER_COMPUTE:
			mDirect3DDeviceContext->CSSetShaderResources(startSlot, srCount, SRs);
			break;
		}
	}

	void ER_RHI_DX11::SetUnorderedAccessResources(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_RHI_GPUResource*>& aUAVs, UINT startSlot,
		ER_RHI_GPURootSignature* rs, int rootParamIndex, bool isComputeRS, bool skipAutomaticTransition)
	{
		assert(aUAVs.size() > 0);
		assert(aUAVs.size() <= DX11_MAX_BOUND_UNORDERED_ACCESS_VIEWS);

		ID3D11UnorderedAccessView* UAVs[DX11_MAX_BOUND_UNORDERED_ACCESS_VIEWS] = {};
		UINT uavCount = static_cast<UINT>(aUAVs.size());
		for (UINT i = 0; i < uavCount; i++)
		{
			assert(aUAVs[i]);
			UAVs[i] = static_cast<ID3D11UnorderedAccessView*>(aUAVs[i]->GetUAV());
			assert(UAVs[i]);
		}

		switch (aShaderType)
		{
		case ER_RHI_SHADER_TYPE::ER_VERTEX:
			throw ER_CoreException("ER_RHI_DX11: Binding UAV to this shader stage is not possible."); //TODO possible with 11_3 i think?
			break;
		case ER_RHI_SHADER_TYPE::ER_GEOMETRY:
			throw ER_CoreException("ER_RHI_DX11: Binding UAV to this shader stage is not possible.");
			break;
		case ER_RHI_SHADER_TYPE::ER_TESSELLATION_HULL:
		case ER_RHI_SHADER_TYPE::ER_TESSELLATION_DOMAIN:
			throw ER_CoreException("ER_RHI_DX11: Binding UAV to this shader stage is not possible.");
			break;
		case ER_RHI_SHADER_TYPE::ER_PIXEL:
			throw ER_CoreException("ER_RHI_DX11: Binding UAV to this shader stage is not possible.");
			break;
		case ER_RHI_SHADER_TYPE::ER_COMPUTE:
			mDirect3DDeviceContext->CSSetUnorderedAccessViews(startSlot, uavCount, UAVs, NULL);
			break;
		}
	}

	void ER_RHI_DX11::SetConstantBuffers(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_RHI_GPUBuffer*>& aCBs, UINT startSlot,
		ER_RHI_GPURootSignature* rs, int rootParamIndex, bool isComputeRS)
	{
		assert(aCBs.size() > 0);
		assert(aCBs.size() <= DX11_MAX_BOUND_CONSTANT_BUFFERS);

		ID3D11Buffer* CBs[DX11_MAX_BOUND_CONSTANT_BUFFERS] = {};
		UINT cbsCount = static_cast<UINT>(aCBs.size());
		for (UINT i = 0; i < cbsCount; i++)
		{
			assert(aCBs[i]);
			CBs[i] = static_cast<ID3D11Buffer*>(aCBs[i]->GetBuffer());
			assert(CBs[i]);
		}

		switch (aShaderType)
		{
		case ER_RHI_SHADER_TYPE::ER_VERTEX:
			mDirect3DDeviceContext->VSSetConstantBuffers(startSlot, cbsCount, CBs);
			break;
		case ER_RHI_SHADER_TYPE::ER_GEOMETRY:
			mDirect3DDeviceContext->GSSetConstantBuffers(startSlot, cbsCount, CBs);
			break;
		case ER_RHI_SHADER_TYPE::ER_TESSELLATION_HULL:
			mDirect3DDeviceContext->HSSetConstantBuffers(startSlot, cbsCount, CBs);
			break;
		case ER_RHI_SHADER_TYPE::ER_TESSELLATION_DOMAIN:
			mDirect3DDeviceContext->DSSetConstantBuffers(startSlot, cbsCount, CBs);
			break;
		case ER_RHI_SHADER_TYPE::ER_PIXEL:
			mDirect3DDeviceContext->PSSetConstantBuffers(startSlot, cbsCount, CBs);
			break;
		case ER_RHI_SHADER_TYPE::ER_COMPUTE:
			mDirect3DDeviceContext->CSSetConstantBuffers(startSlot, cbsCount, CBs);
			break;
		}
	}

	void ER_RHI_DX11::SetShader(ER_RHI_GPUShader* aShader)
	{
		assert(aShader);

		switch (aShader->mShaderType)
		{
		case ER_RHI_SHADER_TYPE::ER_VERTEX:
		{
			ID3D11VertexShader* vs = static_cast<ID3D11VertexShader*>(aShader->GetShaderObject());
			assert(vs);
			mDirect3DDeviceContext->VSSetShader(vs, NULL, NULL);
		}
			break;
		case ER_RHI_SHADER_TYPE::ER_GEOMETRY:
		{
			ID3D11GeometryShader* gs = static_cast<ID3D11GeometryShader*>(aShader->GetShaderObject());
			assert(gs);
			mDirect3DDeviceContext->GSSetShader(gs, NULL, NULL);
		}
			break;
		case ER_RHI_SHADER_TYPE::ER_TESSELLATION_HULL:
		{
			ID3D11HullShader* hs = static_cast<ID3D11HullShader*>(aShader->GetShaderObject());
			assert(hs);
			mDirect3DDeviceContext->HSSetShader(hs, NULL, NULL);
		}
			break;
		case ER_RHI_SHADER_TYPE::ER_TESSELLATION_DOMAIN:
		{
			ID3D11DomainShader* ds = static_cast<ID3D11DomainShader*>(aShader->GetShaderObject());
			assert(ds);
			mDirect3DDeviceContext->DSSetShader(ds, NULL, NULL);
		}
			break;
		case ER_RHI_SHADER_TYPE::ER_PIXEL:
		{
			ID3D11PixelShader* ps = static_cast<ID3D11PixelShader*>(aShader->GetShaderObject());
			assert(ps);
			mDirect3DDeviceContext->PSSetShader(ps, NULL, NULL);
		}
			break;
		case ER_RHI_SHADER_TYPE::ER_COMPUTE:
		{
			ID3D11ComputeShader* cs = static_cast<ID3D11ComputeShader*>(aShader->GetShaderObject());
			assert(cs);
			mDirect3DDeviceContext->CSSetShader(cs, NULL, NULL);
		}
			break;
		}
	}

	void ER_RHI_DX11::SetSamplers(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_RHI_SAMPLER_STATE>& aSamplers, UINT startSlot, ER_RHI_GPURootSignature* rs)
	{
		assert(aSamplers.size() > 0);
		assert(aSamplers.size() <= DX11_MAX_BOUND_SAMPLERS);

		ID3D11SamplerState* SS[DX11_MAX_BOUND_SAMPLERS] = {};
		UINT ssCount = static_cast<UINT>(aSamplers.size());
		for (UINT i = 0; i < ssCount; i++)
		{
			auto it = mSamplerStates.find(aSamplers[i]);
			if (it == mSamplerStates.end())
				throw ER_CoreException("ER_RHI_DX11: Could not find a sampler state that you want to bind to the shader. It is probably not implemented in your graphics API");				
			else
				SS[i] = it->second;
		}

		switch (aShaderType)
		{
		case ER_RHI_SHADER_TYPE::ER_VERTEX:
			mDirect3DDeviceContext->VSSetSamplers(startSlot, ssCount, SS);
			break;
		case ER_RHI_SHADER_TYPE::ER_GEOMETRY:
			mDirect3DDeviceContext->GSSetSamplers(startSlot, ssCount, SS);
			break;
		case ER_RHI_SHADER_TYPE::ER_TESSELLATION_HULL:
			mDirect3DDeviceContext->HSSetSamplers(startSlot, ssCount, SS);
			break;
		case ER_RHI_SHADER_TYPE::ER_TESSELLATION_DOMAIN:
			mDirect3DDeviceContext->DSSetSamplers(startSlot, ssCount, SS);
			break;
		case ER_RHI_SHADER_TYPE::ER_PIXEL:
			mDirect3DDeviceContext->PSSetSamplers(startSlot, ssCount, SS);
			break;
		case ER_RHI_SHADER_TYPE::ER_COMPUTE:
			mDirect3DDeviceContext->CSSetSamplers(startSlot, ssCount, SS);
			break;
		}
	}

	void ER_RHI_DX11::SetInputLayout(ER_RHI_InputLayout* aIL)
	{
		ER_RHI_DX11_InputLayout* aDX11_IL = static_cast<ER_RHI_DX11_InputLayout*>(aIL);
		assert(aDX11_IL);
		assert(aDX11_IL->mInputLayout);
		mDirect3DDeviceContext->IASetInputLayout(aDX11_IL->mInputLayout);
	}

	void ER_RHI_DX11::SetEmptyInputLayout()
	{
		mDirect3DDeviceContext->IASetInputLayout(nullptr);
	}

	void ER_RHI_DX11::SetIndexBuffer(ER_RHI_GPUBuffer* aBuffer, UINT offset /*= 0*/)
	{
		assert(aBuffer);
		ID3D11Buffer* buf = static_cast<ID3D11Buffer*>(aBuffer->GetBuffer());
		assert(buf);
		mDirect3DDeviceContext->IASetIndexBuffer(buf, GetFormat(aBuffer->GetFormatRhi()), offset);
	}

	void ER_RHI_DX11::SetVertexBuffers(const std::vector<ER_RHI_GPUBuffer*>& aVertexBuffers)
	{
		assert(aVertexBuffers.size() > 0 && aVertexBuffers.size() <= ER_RHI_MAX_BOUND_VERTEX_BUFFERS);
		if (aVertexBuffers.size() == 1)
		{
			UINT stride = aVertexBuffers[0]->GetStride();
			UINT offset = 0;
			assert(aVertexBuffers[0]);
			ID3D11Buffer* bufferPointers[1] = { static_cast<ID3D11Buffer*>(aVertexBuffers[0]->GetBuffer()) };
			assert(bufferPointers[0]);
			mDirect3DDeviceContext->IASetVertexBuffers(0, 1, bufferPointers, &stride, &offset);
		}
		else //+ instance buffer
		{
			UINT strides[2] = { aVertexBuffers[0]->GetStride(), aVertexBuffers[1]->GetStride() };
			UINT offsets[2] = { 0, 0 };

			assert(aVertexBuffers[0]);
			assert(aVertexBuffers[1]);
			ID3D11Buffer* bufferPointers[2] = { static_cast<ID3D11Buffer*>(aVertexBuffers[0]->GetBuffer()), static_cast<ID3D11Buffer*>(aVertexBuffers[1]->GetBuffer()) };
			assert(bufferPointers[0]);
			assert(bufferPointers[1]);
			mDirect3DDeviceContext->IASetVertexBuffers(0, 2, bufferPointers, strides, offsets);
		}
	}

	void ER_RHI_DX11::SetTopologyType(ER_RHI_PRIMITIVE_TYPE aType)
	{
		mDirect3DDeviceContext->IASetPrimitiveTopology(GetTopologyType(aType));
	}

	ER_RHI_PRIMITIVE_TYPE ER_RHI_DX11::GetCurrentTopologyType()
	{
		D3D11_PRIMITIVE_TOPOLOGY currentTopology;
		mDirect3DDeviceContext->IAGetPrimitiveTopology(&currentTopology);
		return GetTopologyType(currentTopology);
	}

	void ER_RHI_DX11::UnbindRenderTargets()
	{
		ID3D11RenderTargetView* nullRTVs[1] = { NULL };
		mDirect3DDeviceContext->OMSetRenderTargets(1, nullRTVs, nullptr);
	}

	void ER_RHI_DX11::UnbindResourcesFromShader(ER_RHI_SHADER_TYPE aShaderType, bool unbindShader)
	{
		auto context = GetContext();

		ID3D11ShaderResourceView* nullSRV[DX11_MAX_BOUND_SHADER_RESOURCE_VIEWS] = { NULL };
		ID3D11Buffer* nullCBs[DX11_MAX_BOUND_CONSTANT_BUFFERS] = { NULL };
		ID3D11SamplerState* nullSSs[DX11_MAX_BOUND_SAMPLERS] = { NULL };
		ID3D11UnorderedAccessView* nullUAV[DX11_MAX_BOUND_UNORDERED_ACCESS_VIEWS] = { NULL };

		switch (aShaderType)
		{
		case ER_RHI_SHADER_TYPE::ER_VERTEX:
		{
			if (unbindShader)
				context->VSSetShader(NULL, NULL, NULL);
			context->VSSetShaderResources(0, DX11_MAX_BOUND_SHADER_RESOURCE_VIEWS, nullSRV);
			context->VSSetConstantBuffers(0, DX11_MAX_BOUND_CONSTANT_BUFFERS, nullCBs);
		}
		break;
		case ER_RHI_SHADER_TYPE::ER_GEOMETRY:
		{
			if (unbindShader)
				context->GSSetShader(NULL, NULL, NULL);
			context->GSSetShaderResources(0, DX11_MAX_BOUND_SHADER_RESOURCE_VIEWS, nullSRV);
			context->GSSetConstantBuffers(0, DX11_MAX_BOUND_CONSTANT_BUFFERS, nullCBs);
			context->GSSetSamplers(0, DX11_MAX_BOUND_SAMPLERS, nullSSs);
		}
		break;
		case ER_RHI_SHADER_TYPE::ER_TESSELLATION_HULL:
		{
			if (unbindShader)
				context->HSSetShader(NULL, NULL, NULL);
			context->HSSetShaderResources(0, DX11_MAX_BOUND_SHADER_RESOURCE_VIEWS, nullSRV);
			context->HSSetConstantBuffers(0, DX11_MAX_BOUND_CONSTANT_BUFFERS, nullCBs);
			context->HSSetSamplers(0, DX11_MAX_BOUND_SAMPLERS, nullSSs);
		}
		break;
		case ER_RHI_SHADER_TYPE::ER_TESSELLATION_DOMAIN:
		{
			if (unbindShader)
				context->DSSetShader(NULL, NULL, NULL);
			context->DSSetShaderResources(0, DX11_MAX_BOUND_SHADER_RESOURCE_VIEWS, nullSRV);
			context->DSSetConstantBuffers(0, DX11_MAX_BOUND_CONSTANT_BUFFERS, nullCBs);
			context->DSSetSamplers(0, DX11_MAX_BOUND_SAMPLERS, nullSSs);
		}
		break;
		case ER_RHI_SHADER_TYPE::ER_PIXEL:
		{
			if (unbindShader)
				context->PSSetShader(NULL, NULL, NULL);
			context->PSSetShaderResources(0, DX11_MAX_BOUND_SHADER_RESOURCE_VIEWS, nullSRV);
			context->PSSetConstantBuffers(0, DX11_MAX_BOUND_CONSTANT_BUFFERS, nullCBs);
			context->PSSetSamplers(0, DX11_MAX_BOUND_SAMPLERS, nullSSs);
		}
		break;
		case ER_RHI_SHADER_TYPE::ER_COMPUTE:
		{
			if (unbindShader)
				context->CSSetShader(NULL, NULL, NULL);
			context->CSSetShaderResources(0, DX11_MAX_BOUND_SHADER_RESOURCE_VIEWS, nullSRV);
			context->CSSetConstantBuffers(0, DX11_MAX_BOUND_CONSTANT_BUFFERS, nullCBs);
			context->CSSetSamplers(0, DX11_MAX_BOUND_SAMPLERS, nullSSs);
			context->CSSetUnorderedAccessViews(0, DX11_MAX_BOUND_UNORDERED_ACCESS_VIEWS, nullUAV, 0);
		}
		break;
		}
	}

	void ER_RHI_DX11::UpdateBuffer(ER_RHI_GPUBuffer* aBuffer, void* aData, int dataSize)
	{
		assert(aBuffer->GetSize() >= dataSize);

		ER_RHI_DX11_GPUBuffer* buffer = static_cast<ER_RHI_DX11_GPUBuffer*>(aBuffer);
		assert(buffer);

		D3D11_MAPPED_SUBRESOURCE mappedResource;
		ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
		buffer->Map(this, D3D11_MAP_WRITE_DISCARD, &mappedResource);
		memcpy(mappedResource.pData, aData, dataSize);
		buffer->Unmap(this);
	}

	void ER_RHI_DX11::InitImGui()
	{
		ImGui_ImplDX11_Init(mDirect3DDevice, mDirect3DDeviceContext);
	}

	void ER_RHI_DX11::StartNewImGuiFrame()
	{
		ImGui_ImplDX11_NewFrame();
	}

	void ER_RHI_DX11::RenderDrawDataImGui(int cmdListIndex)
	{
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}

	void ER_RHI_DX11::ShutdownImGui()
	{
		ImGui_ImplDX11_Shutdown();
	}

	void ER_RHI_DX11::BeginEventTag(const std::string& aName, bool isComputeQueue /*= false*/)
	{
		mUserDefinedAnnotation->BeginEvent(ER_Utility::ToWideString(aName).c_str());
	}

	void ER_RHI_DX11::EndEventTag(bool isComputeQueue /*= false*/)
	{
		mUserDefinedAnnotation->EndEvent();
	}	

	DXGI_FORMAT ER_RHI_DX11::GetFormat(ER_RHI_FORMAT aFormat)
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
		case ER_RHI_FORMAT::ER_FORMAT_R8G8_TYPELESS:
			return DXGI_FORMAT_R8G8_TYPELESS;
		case ER_RHI_FORMAT::ER_FORMAT_R8G8_UNORM:
			return DXGI_FORMAT_R8G8_UNORM;
		case ER_RHI_FORMAT::ER_FORMAT_R8G8_UINT:
			return DXGI_FORMAT_R8G8_UINT;
		case ER_RHI_FORMAT::ER_FORMAT_R16_TYPELESS:
			return DXGI_FORMAT_R16_TYPELESS;
		case ER_RHI_FORMAT::ER_FORMAT_D16_UNORM:
			return DXGI_FORMAT_D16_UNORM;
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

	D3D11_PRIMITIVE_TOPOLOGY ER_RHI_DX11::GetTopologyType(ER_RHI_PRIMITIVE_TYPE aType)
	{
		switch (aType)
		{
		case ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_POINTLIST:
			return D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
		case ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_LINELIST:
			return D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
		case ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
			return D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		case ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
			return D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		case ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_CONTROL_POINT_PATCHLIST:
			return D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST;
		default:
		{
			assert(0);
			return D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
		}
		}
	}

	ER_RHI_PRIMITIVE_TYPE ER_RHI_DX11::GetTopologyType(D3D11_PRIMITIVE_TOPOLOGY aType)
	{
		switch (aType)
		{
		case D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_POINTLIST:
			return ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_POINTLIST;
		case D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_LINELIST:
			return ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_LINELIST;
		case D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
			return ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		case D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
			return ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		case D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST:
			return ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_CONTROL_POINT_PATCHLIST;
		default:
		{
			assert(0);
			return ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		}
		}
	}

	void ER_RHI_DX11::CreateSamplerStates()
	{
		float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		float black[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		auto direct3DDevice = mDirect3DDevice;

		// trilinear samplers
		D3D11_SAMPLER_DESC samplerStateDesc;
		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerStateDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerStateDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerStateDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerStateDesc.MipLODBias = 0;
		samplerStateDesc.MaxAnisotropy = 1;
		samplerStateDesc.MinLOD = -1000.0f;
		samplerStateDesc.MaxLOD = 1000.0f;
		HRESULT hr = direct3DDevice->CreateSamplerState(&samplerStateDesc, &TrilinearWrapSS);
		if (FAILED(hr))
			throw ER_CoreException("ER_RHI_DX11: Could not create TrilinearWrapSS", hr);
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, TrilinearWrapSS));

		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerStateDesc.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR;
		samplerStateDesc.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR;
		samplerStateDesc.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
		hr = direct3DDevice->CreateSamplerState(&samplerStateDesc, &TrilinearMirrorSS);
		if (FAILED(hr))
			throw ER_CoreException("ER_RHI_DX11: Could not create TrilinearMirrorSS", hr);
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_TRILINEAR_MIRROR, TrilinearMirrorSS));

		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerStateDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerStateDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerStateDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		hr = direct3DDevice->CreateSamplerState(&samplerStateDesc, &TrilinearClampSS);
		if (FAILED(hr))
			throw ER_CoreException("ER_RHI_DX11: Could not create TrilinearClampSS", hr);
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_TRILINEAR_CLAMP, TrilinearClampSS));

		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerStateDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		samplerStateDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		samplerStateDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		memcpy(samplerStateDesc.BorderColor, reinterpret_cast<FLOAT*>(&black), sizeof(FLOAT) * 4);
		hr = direct3DDevice->CreateSamplerState(&samplerStateDesc, &TrilinearBorderSS);
		if (FAILED(hr))
			throw ER_CoreException("ER_RHI_DX11: Could not create TrilinearBorderSS", hr);
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_TRILINEAR_BORDER, TrilinearBorderSS));

		// bilinear samplers
		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		samplerStateDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerStateDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerStateDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		hr = direct3DDevice->CreateSamplerState(&samplerStateDesc, &BilinearWrapSS);
		if (FAILED(hr))
			throw ER_CoreException("ER_RHI_DX11: Could not create BilinearWrapSS", hr);
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_BILINEAR_WRAP, BilinearWrapSS));

		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		samplerStateDesc.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR;
		samplerStateDesc.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR;
		samplerStateDesc.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
		hr = direct3DDevice->CreateSamplerState(&samplerStateDesc, &BilinearMirrorSS);
		if (FAILED(hr))
			throw ER_CoreException("ER_RHI_DX11: Could not create BilinearMirrorSS", hr);
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_BILINEAR_MIRROR, BilinearMirrorSS));

		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		samplerStateDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerStateDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerStateDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		hr = direct3DDevice->CreateSamplerState(&samplerStateDesc, &BilinearClampSS);
		if (FAILED(hr))
			throw ER_CoreException("ER_RHI_DX11: Could not create BilinearClampSS", hr);
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_BILINEAR_CLAMP, BilinearClampSS));

		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		samplerStateDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		samplerStateDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		samplerStateDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		memcpy(samplerStateDesc.BorderColor, reinterpret_cast<FLOAT*>(&black), sizeof(FLOAT) * 4);
		hr = direct3DDevice->CreateSamplerState(&samplerStateDesc, &BilinearBorderSS);
		if (FAILED(hr))
			throw ER_CoreException("ER_RHI_DX11: Could not create BilinerBorderSS", hr);
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_BILINEAR_BORDER, BilinearBorderSS));

		// anisotropic samplers
		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		samplerStateDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerStateDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerStateDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		hr = direct3DDevice->CreateSamplerState(&samplerStateDesc, &AnisotropicWrapSS);
		if (FAILED(hr))
			throw ER_CoreException("ER_RHI_DX11: Could not create AnisotropicWrapSS", hr);
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_ANISOTROPIC_WRAP, AnisotropicWrapSS));

		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		samplerStateDesc.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR;
		samplerStateDesc.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR;
		samplerStateDesc.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
		hr = direct3DDevice->CreateSamplerState(&samplerStateDesc, &AnisotropicMirrorSS);
		if (FAILED(hr))
			throw ER_CoreException("ER_RHI_DX11: Could not create AnisotropicMirrorSS", hr);
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_ANISOTROPIC_MIRROR, AnisotropicMirrorSS));

		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		samplerStateDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerStateDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerStateDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		hr = direct3DDevice->CreateSamplerState(&samplerStateDesc, &AnisotropicClampSS);
		if (FAILED(hr))
			throw ER_CoreException("ER_RHI_DX11: Could not create AnisotropicClampSS", hr);
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_ANISOTROPIC_CLAMP, AnisotropicClampSS));

		ZeroMemory(&samplerStateDesc, sizeof(samplerStateDesc));
		samplerStateDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		samplerStateDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		samplerStateDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		samplerStateDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		memcpy(samplerStateDesc.BorderColor, reinterpret_cast<FLOAT*>(&black), sizeof(FLOAT) * 4);
		hr = direct3DDevice->CreateSamplerState(&samplerStateDesc, &AnisotropicBorderSS);
		if (FAILED(hr))
			throw ER_CoreException("ER_RHI_DX11: Could not create AnisotropicBorderSS", hr);
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_ANISOTROPIC_BORDER, AnisotropicBorderSS));

		// shadow sampler state
		samplerStateDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		samplerStateDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		samplerStateDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		samplerStateDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		memcpy(samplerStateDesc.BorderColor, reinterpret_cast<FLOAT*>(&white), sizeof(FLOAT) * 4);
		samplerStateDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
		if (FAILED(direct3DDevice->CreateSamplerState(&samplerStateDesc, &ShadowSS)))
			throw ER_CoreException("ER_RHI_DX11: Could not create ShadowSS!");
		mSamplerStates.insert(std::make_pair(ER_RHI_SAMPLER_STATE::ER_SHADOW_SS, ShadowSS));
	}

	void ER_RHI_DX11::CreateBlendStates()
	{
		D3D11_BLEND_DESC blendStateDescription;
		ZeroMemory(&blendStateDescription, sizeof(D3D11_BLEND_DESC));

		blendStateDescription.AlphaToCoverageEnable = TRUE;
		blendStateDescription.RenderTarget[0].BlendEnable = TRUE;
		blendStateDescription.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blendStateDescription.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blendStateDescription.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendStateDescription.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendStateDescription.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		blendStateDescription.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendStateDescription.RenderTarget[0].RenderTargetWriteMask = 0x0f;
		if (FAILED(mDirect3DDevice->CreateBlendState(&blendStateDescription, &mAlphaToCoverageState)))
			throw ER_CoreException("ER_RHI_DX11: ID3D11Device::CreateBlendState() failed while create alpha-to-coverage blend state.");
		mBlendStates.insert(std::make_pair(ER_RHI_BLEND_STATE::ER_ALPHA_TO_COVERAGE, mAlphaToCoverageState));

		blendStateDescription.RenderTarget[0].BlendEnable = FALSE;
		blendStateDescription.AlphaToCoverageEnable = FALSE;
		if (FAILED(mDirect3DDevice->CreateBlendState(&blendStateDescription, &mNoBlendState)))
			throw ER_CoreException("ER_RHI_DX11: ID3D11Device::CreateBlendState() failed while create no blend state.");
		mBlendStates.insert(std::make_pair(ER_RHI_BLEND_STATE::ER_NO_BLEND, mNoBlendState));
	}

	void ER_RHI_DX11::CreateRasterizerStates()
	{
		D3D11_RASTERIZER_DESC rasterizerStateDesc;
		ZeroMemory(&rasterizerStateDesc, sizeof(rasterizerStateDesc));
		rasterizerStateDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerStateDesc.CullMode = D3D11_CULL_BACK;
		rasterizerStateDesc.DepthClipEnable = true;
		HRESULT hr = mDirect3DDevice->CreateRasterizerState(&rasterizerStateDesc, &BackCullingRS);
		if (FAILED(hr))
			throw ER_CoreException("ER_RHI_DX11: CreateRasterizerStates() failed when creating BackCullingRS.", hr);
		mRasterizerStates.insert(std::make_pair(ER_RHI_RASTERIZER_STATE::ER_BACK_CULLING, BackCullingRS));

		ZeroMemory(&rasterizerStateDesc, sizeof(rasterizerStateDesc));
		rasterizerStateDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerStateDesc.CullMode = D3D11_CULL_BACK;
		rasterizerStateDesc.FrontCounterClockwise = true;
		rasterizerStateDesc.DepthClipEnable = true;
		hr = mDirect3DDevice->CreateRasterizerState(&rasterizerStateDesc, &FrontCullingRS);
		if (FAILED(hr))
			throw ER_CoreException("ER_RHI_DX11: CreateRasterizerStates() failed when creating FrontCullingRS.", hr);
		mRasterizerStates.insert(std::make_pair(ER_RHI_RASTERIZER_STATE::ER_FRONT_CULLING, FrontCullingRS));

		ZeroMemory(&rasterizerStateDesc, sizeof(rasterizerStateDesc));
		rasterizerStateDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerStateDesc.CullMode = D3D11_CULL_NONE;
		rasterizerStateDesc.DepthClipEnable = true;
		hr = mDirect3DDevice->CreateRasterizerState(&rasterizerStateDesc, &NoCullingRS);
		if (FAILED(hr))
			throw ER_CoreException("ER_RHI_DX11: CreateRasterizerStates() failed when creating NoCullingRS.", hr);
		mRasterizerStates.insert(std::make_pair(ER_RHI_RASTERIZER_STATE::ER_NO_CULLING, NoCullingRS));

		ZeroMemory(&rasterizerStateDesc, sizeof(rasterizerStateDesc));
		rasterizerStateDesc.FillMode = D3D11_FILL_WIREFRAME;
		rasterizerStateDesc.CullMode = D3D11_CULL_NONE;
		rasterizerStateDesc.DepthClipEnable = true;
		hr = mDirect3DDevice->CreateRasterizerState(&rasterizerStateDesc, &WireframeRS);
		if (FAILED(hr))
			throw ER_CoreException("ER_RHI_DX11: CreateRasterizerStates() failed when creating WireframeRS.", hr);
		mRasterizerStates.insert(std::make_pair(ER_RHI_RASTERIZER_STATE::ER_WIREFRAME, WireframeRS));

		ZeroMemory(&rasterizerStateDesc, sizeof(rasterizerStateDesc));
		rasterizerStateDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerStateDesc.CullMode = D3D11_CULL_NONE;
		rasterizerStateDesc.DepthClipEnable = false;
		rasterizerStateDesc.ScissorEnable = true;
		hr = mDirect3DDevice->CreateRasterizerState(&rasterizerStateDesc, &NoCullingNoDepthEnabledScissorRS);
		if (FAILED(hr))
			throw ER_CoreException("ER_RHI_DX11: CreateRasterizerStates() failed when creating NoCullingNoDepthEnabledScissorRS.", hr);
		mRasterizerStates.insert(std::make_pair(ER_RHI_RASTERIZER_STATE::ER_NO_CULLING_NO_DEPTH_SCISSOR_ENABLED, NoCullingNoDepthEnabledScissorRS));

		ZeroMemory(&rasterizerStateDesc, sizeof(rasterizerStateDesc));
		rasterizerStateDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerStateDesc.CullMode = D3D11_CULL_BACK;
		rasterizerStateDesc.DepthClipEnable = false;
		rasterizerStateDesc.DepthBias = 0; //0.05f
		rasterizerStateDesc.SlopeScaledDepthBias = 3.0f;
		rasterizerStateDesc.FrontCounterClockwise = false;
		hr = mDirect3DDevice->CreateRasterizerState(&rasterizerStateDesc, &ShadowRS);
		if (FAILED(hr))
			throw ER_CoreException("ER_RHI_DX11: CreateRasterizerStates() failed when creating ShadowRS.", hr);
		mRasterizerStates.insert(std::make_pair(ER_RHI_RASTERIZER_STATE::ER_SHADOW_RS, ShadowRS));
	}

	void ER_RHI_DX11::CreateDepthStencilStates()
	{
		//TODO
		D3D11_DEPTH_STENCIL_DESC depthStencilStateDesc;
		depthStencilStateDesc.DepthEnable = TRUE;
		depthStencilStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_NEVER;
		depthStencilStateDesc.StencilEnable = FALSE;
		HRESULT hr = mDirect3DDevice->CreateDepthStencilState(&depthStencilStateDesc, &DepthOnlyReadComparisonNeverDS);
		if (FAILED(hr))
			throw ER_CoreException("CreateDepthStencilState() failed when creating DepthOnlyReadComparisonNeverDS.", hr);
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_READ_COMPARISON_NEVER, DepthOnlyReadComparisonNeverDS));

		// read only 
		depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_LESS;
		hr = mDirect3DDevice->CreateDepthStencilState(&depthStencilStateDesc, &DepthOnlyReadComparisonLessDS);
		if (FAILED(hr))
			throw ER_CoreException("CreateDepthStencilState() failed when creating DepthOnlyReadComparisonLessDS.", hr);
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_READ_COMPARISON_LESS, DepthOnlyReadComparisonLessDS));

		depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_EQUAL;
		hr = mDirect3DDevice->CreateDepthStencilState(&depthStencilStateDesc, &DepthOnlyReadComparisonEqualDS);
		if (FAILED(hr))
			throw ER_CoreException("CreateDepthStencilState() failed when creating DepthOnlyReadComparisonEqualDS.", hr);
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_READ_COMPARISON_EQUAL, DepthOnlyReadComparisonEqualDS));

		depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		hr = mDirect3DDevice->CreateDepthStencilState(&depthStencilStateDesc, &DepthOnlyReadComparisonLessEqualDS);
		if (FAILED(hr))
			throw ER_CoreException("CreateDepthStencilState() failed when creating DepthOnlyReadComparisonLessEqualDS.", hr);
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_READ_COMPARISON_LESS_EQUAL, DepthOnlyReadComparisonLessEqualDS));

		depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_GREATER;
		hr = mDirect3DDevice->CreateDepthStencilState(&depthStencilStateDesc, &DepthOnlyReadComparisonGreaterDS);
		if (FAILED(hr))
			throw ER_CoreException("CreateDepthStencilState() failed when creating DepthOnlyReadComparisonGreaterDS.", hr);
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_READ_COMPARISON_GREATER, DepthOnlyReadComparisonGreaterDS));

		depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_NOT_EQUAL;
		hr = mDirect3DDevice->CreateDepthStencilState(&depthStencilStateDesc, &DepthOnlyReadComparisonNotEqualDS);
		if (FAILED(hr))
			throw ER_CoreException("CreateDepthStencilState() failed when creating DepthOnlyReadComparisonNotEqualDS.", hr);
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_READ_COMPARISON_NOT_EQUAL, DepthOnlyReadComparisonNotEqualDS));

		depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
		hr = mDirect3DDevice->CreateDepthStencilState(&depthStencilStateDesc, &DepthOnlyReadComparisonGreaterEqualDS);
		if (FAILED(hr))
			throw ER_CoreException("CreateDepthStencilState() failed when creating DepthOnlyReadComparisonGreaterEqualDS.", hr);
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_READ_COMPARISON_GREATER_EQUAL, DepthOnlyReadComparisonGreaterEqualDS));

		depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		hr = mDirect3DDevice->CreateDepthStencilState(&depthStencilStateDesc, &DepthOnlyReadComparisonAlwaysDS);
		if (FAILED(hr))
			throw ER_CoreException("CreateDepthStencilState() failed when creating DepthOnlyReadComparisonAlwaysDS.", hr);
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_READ_COMPARISON_ALWAYS, DepthOnlyReadComparisonAlwaysDS));

		//write
		depthStencilStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_LESS;
		hr = mDirect3DDevice->CreateDepthStencilState(&depthStencilStateDesc, &DepthOnlyWriteComparisonLessDS);
		if (FAILED(hr))
			throw ER_CoreException("CreateDepthStencilState() failed when creating DepthOnlyWriteComparisonLessDS.", hr);
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_WRITE_COMPARISON_LESS, DepthOnlyWriteComparisonLessDS));

		depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_EQUAL;
		hr = mDirect3DDevice->CreateDepthStencilState(&depthStencilStateDesc, &DepthOnlyWriteComparisonEqualDS);
		if (FAILED(hr))
			throw ER_CoreException("CreateDepthStencilState() failed when creating DepthOnlyWriteComparisonEqualDS.", hr);
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_WRITE_COMPARISON_EQUAL, DepthOnlyWriteComparisonEqualDS));

		depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		hr = mDirect3DDevice->CreateDepthStencilState(&depthStencilStateDesc, &DepthOnlyWriteComparisonLessEqualDS);
		if (FAILED(hr))
			throw ER_CoreException("CreateDepthStencilState() failed when creating DepthOnlyWriteComparisonLessEqualDS.", hr);
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_WRITE_COMPARISON_LESS_EQUAL, DepthOnlyWriteComparisonLessEqualDS));

		depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_GREATER;
		hr = mDirect3DDevice->CreateDepthStencilState(&depthStencilStateDesc, &DepthOnlyWriteComparisonGreaterDS);
		if (FAILED(hr))
			throw ER_CoreException("CreateDepthStencilState() failed when creating DepthOnlyWriteComparisonGreaterDS.", hr);
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_WRITE_COMPARISON_GREATER, DepthOnlyWriteComparisonGreaterDS));

		depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_NOT_EQUAL;
		hr = mDirect3DDevice->CreateDepthStencilState(&depthStencilStateDesc, &DepthOnlyWriteComparisonNotEqualDS);
		if (FAILED(hr))
			throw ER_CoreException("CreateDepthStencilState() failed when creating DepthOnlyWriteComparisonNotEqualDS.", hr);
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_WRITE_COMPARISON_NOT_EQUAL, DepthOnlyWriteComparisonNotEqualDS));

		depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
		hr = mDirect3DDevice->CreateDepthStencilState(&depthStencilStateDesc, &DepthOnlyWriteComparisonGreaterEqualDS);
		if (FAILED(hr))
			throw ER_CoreException("CreateDepthStencilState() failed when creating DepthOnlyWriteComparisonGreaterEqualDS.", hr);
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_WRITE_COMPARISON_GREATER_EQUAL, DepthOnlyWriteComparisonGreaterEqualDS));

		depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		hr = mDirect3DDevice->CreateDepthStencilState(&depthStencilStateDesc, &DepthOnlyWriteComparisonAlwaysDS);
		if (FAILED(hr))
			throw ER_CoreException("CreateDepthStencilState() failed when creating DepthOnlyWriteComparisonAlwaysDS.", hr);
		mDepthStates.insert(std::make_pair(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_WRITE_COMPARISON_ALWAYS, DepthOnlyWriteComparisonAlwaysDS));
	}

}