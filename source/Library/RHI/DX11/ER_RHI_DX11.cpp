#include "ER_RHI_DX11.h"
#include "..\..\ER_CoreException.h"

#define DX11_MAX_BOUND_RENDER_TARGETS_VIEWS 8
#define DX11_MAX_BOUND_SHADER_RESOURCE_VIEWS 64 
#define DX11_MAX_BOUND_UNORDERED_ACCESS_VIEWS 16 

namespace Library
{

	ER_RHI_DX11::~ER_RHI_DX11()
	{
		ReleaseObject(mMainRenderTargetView);
		ReleaseObject(mMainDepthStencilView);
		ReleaseObject(mSwapChain);
		ReleaseObject(mDepthStencilBuffer);

		if (mDirect3DDeviceContext)
			mDirect3DDeviceContext->ClearState();

		ReleaseObject(mDirect3DDeviceContext);
		ReleaseObject(mDirect3DDevice);
	}

	bool ER_RHI_DX11::Initialize(UINT width, UINT height, bool isFullscreen)
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
		{
			throw ER_CoreException("D3D11CreateDevice() failed", hr);
		}

		if (FAILED(hr = direct3DDevice->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&mDirect3DDevice))))
		{
			throw ER_CoreException("ID3D11Device::QueryInterface() failed", hr);
		}

		if (FAILED(hr = direct3DDeviceContext->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&mDirect3DDeviceContext))))
		{
			throw ER_CoreException("ID3D11Device::QueryInterface() failed", hr);
		}

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
			throw ER_CoreException("ID3D11Device::QueryInterface() failed", hr);
		}

		IDXGIAdapter* dxgiAdapter = nullptr;
		if (FAILED(hr = dxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&dxgiAdapter))))
		{
			ReleaseObject(dxgiDevice);
			throw ER_CoreException("IDXGIDevice::GetParent() failed retrieving adapter.", hr);
		}

		IDXGIFactory2* dxgiFactory = nullptr;
		if (FAILED(hr = dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory))))
		{
			ReleaseObject(dxgiDevice);
			ReleaseObject(dxgiAdapter);
			throw ER_CoreException("IDXGIAdapter::GetParent() failed retrieving factory.", hr);
		}

		DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullScreenDesc;
		ZeroMemory(&fullScreenDesc, sizeof(fullScreenDesc));
		fullScreenDesc.RefreshRate.Numerator = DefaultFrameRate;
		fullScreenDesc.RefreshRate.Denominator = 1;
		fullScreenDesc.Windowed = !isFullscreen;

		if (FAILED(hr = dxgiFactory->CreateSwapChainForHwnd(dxgiDevice, mWindowHandle, &swapChainDesc, &fullScreenDesc, nullptr, &mSwapChain)))
		{
			ReleaseObject(dxgiDevice);
			ReleaseObject(dxgiAdapter);
			ReleaseObject(dxgiFactory);
			throw ER_CoreException("IDXGIDevice::CreateSwapChainForHwnd() failed.", hr);
		}

		ReleaseObject(dxgiDevice);
		ReleaseObject(dxgiAdapter);
		ReleaseObject(dxgiFactory);

		ID3D11Texture2D* backBuffer;
		if (FAILED(hr = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer))))
		{
			throw ER_CoreException("IDXGISwapChain::GetBuffer() failed.", hr);
		}

		backBuffer->GetDesc(&mBackBufferDesc);


		if (FAILED(hr = mDirect3DDevice->CreateRenderTargetView(backBuffer, nullptr, &mMainRenderTargetView)))
		{
			ReleaseObject(backBuffer);
			throw ER_CoreException("IDXGIDevice::CreateRenderTargetView() failed.", hr);
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
				throw ER_CoreException("IDXGIDevice::CreateTexture2D() failed.", hr);
			}

			if (FAILED(hr = mDirect3DDevice->CreateDepthStencilView(mDepthStencilBuffer, nullptr, &mMainDepthStencilView)))
			{
				throw ER_CoreException("IDXGIDevice::CreateDepthStencilView() failed.", hr);
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

	void ER_RHI_DX11::ClearRenderTarget(ER_GPUTexture* aRenderTarget, float colors[4])
	{
		assert(aRenderTarget);
		mDirect3DDeviceContext->ClearRenderTargetView(aRenderTarget->GetRTV(), colors);
	}

	void ER_RHI_DX11::ClearDepthStencilTarget(ER_GPUTexture* aDepthTarget, float depth, UINT stencil /*= 0*/)
	{
		assert(aDepthTarget);
		mDirect3DDeviceContext->ClearDepthStencilView(aDepthTarget->GetDSV(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, stencil);
	}

	void ER_RHI_DX11::ClearUAV(ER_GPUTexture* aRenderTarget, float colors[4])
	{
		assert(aRenderTarget);
		mDirect3DDeviceContext->ClearUnorderedAccessViewFloat(aRenderTarget->GetUAV(), colors);
	}

	void ER_RHI_DX11::CopyBuffer(ER_GPUBuffer* aDestBuffer, ER_GPUBuffer* aSrcBuffer)
	{
		assert(aDestBuffer);
		assert(aSrcBuffer);

		mDirect3DDeviceContext->CopyResource(aDestBuffer->GetBuffer(), aSrcBuffer->GetBuffer());
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

	void ER_RHI_DX11::GenerateMips(ER_GPUTexture* aTexture)
	{
		assert(aTexture);
		mDirect3DDeviceContext->GenerateMips(aTexture->GetSRV());
	}

	void ER_RHI_DX11::PresentGraphics()
	{
		HRESULT hr = mSwapChain->Present(0, 0);
		if (FAILED(hr))
			throw ER_CoreException("IDXGISwapChain::Present() failed.", hr);
	}

	void ER_RHI_DX11::SetRenderTargets(const std::vector<ER_GPUTexture*>& aRenderTargets, ER_GPUTexture* aDepthTarget /*= nullptr*/, ER_GPUTexture* aUAV /*= nullptr*/)
	{
		if (!aUAV)
		{
			assert(aRenderTargets.size() > 0);
			assert(aRenderTargets.size() <= DX11_MAX_BOUND_RENDER_TARGETS_VIEWS);

			ID3D11RenderTargetView* RTVs[DX11_MAX_BOUND_RENDER_TARGETS_VIEWS] = {};
			UINT rtCount = static_cast<UINT>(aRenderTargets.size());
			for (UINT i = 0; i < rtCount; i++)
			{
				assert(aRenderTargets[i]);
				RTVs[i] = aRenderTargets[i]->GetRTV();
			}

			mDirect3DDeviceContext->OMSetRenderTargets(rtCount, RTVs, aDepthTarget ? aDepthTarget->GetDSV() : NULL);
		}
		else
		{
			ID3D11RenderTargetView* nullRTVs[1] = { NULL };
			ID3D11UnorderedAccessView* UAVs[1] = { aUAV->GetUAV() };
			mDirect3DDeviceContext->OMSetRenderTargetsAndUnorderedAccessViews(0, nullRTVs, aDepthTarget ? aDepthTarget->GetDSV() : NULL, 0, 1, UAVs, NULL);
		}
	}

	void ER_RHI_DX11::SetDepthTarget(ER_GPUTexture* aDepthTarget)
	{
		ID3D11RenderTargetView* nullRTVs[1] = { NULL };

		assert(aDepthTarget);
		mDirect3DDeviceContext->OMSetRenderTargets(1, nullRTVs, aDepthTarget->GetDSV());
	}

	void ER_RHI_DX11::InitImGui()
	{
		ImGui_ImplDX11_Init(mDirect3DDevice, mDirect3DDeviceContext);
	}

	void ER_RHI_DX11::StartNewImGuiFrame()
	{
		ImGui_ImplDX11_NewFrame();
	}

	void ER_RHI_DX11::ShutdownImGui()
	{
		ImGui_ImplDX11_Shutdown();
	}

}