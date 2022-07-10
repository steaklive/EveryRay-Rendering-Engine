#include "ER_RHI_DX11.h"
#include "..\..\ER_CoreException.h"

#define DX11_MAX_BOUND_RENDER_TARGETS_VIEWS 8
#define DX11_MAX_BOUND_SHADER_RESOURCE_VIEWS 64 
#define DX11_MAX_BOUND_UNORDERED_ACCESS_VIEWS 16 
#define DX11_MAX_BOUND_CONSTANT_BUFFERS 8 
#define DX11_MAX_BOUND_SAMPLERS 8 

namespace Library
{

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
			throw ER_CoreException("ER_RHI_DX11: D3D11CreateDevice() failed", hr);
		}

		if (FAILED(hr = direct3DDevice->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&mDirect3DDevice))))
		{
			throw ER_CoreException("ER_RHI_DX11: ID3D11Device::QueryInterface() failed", hr);
		}

		if (FAILED(hr = direct3DDeviceContext->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&mDirect3DDeviceContext))))
		{
			throw ER_CoreException("ER_RHI_DX11: ID3D11Device::QueryInterface() failed", hr);
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

		if (FAILED(hr = dxgiFactory->CreateSwapChainForHwnd(dxgiDevice, mWindowHandle, &swapChainDesc, &fullScreenDesc, nullptr, &mSwapChain)))
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
			throw ER_CoreException("ER_RHI_DX11: IDXGISwapChain::Present() failed.", hr);
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

	void ER_RHI_DX11::SetShaderResources(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_GPUTexture*>& aSRVs, UINT startSlot /*= 0*/)
	{
		assert(aSRVs.size() > 0);
		assert(aSRVs.size() <= DX11_MAX_BOUND_SHADER_RESOURCE_VIEWS);

		ID3D11ShaderResourceView* SRs[DX11_MAX_BOUND_SHADER_RESOURCE_VIEWS] = {};
		UINT srCount = static_cast<UINT>(aSRVs.size());
		for (UINT i = 0; i < srCount; i++)
		{
			assert(aSRVs[i]);
			SRs[i] = aSRVs[i]->GetSRV();
		}

		switch (aShaderType)
		{
		case ER_RHI_SHADER_TYPE::ER_VERTEX:
			mDirect3DDeviceContext->VSSetShaderResources(startSlot, srCount, SRs);
			break;
		case ER_RHI_SHADER_TYPE::ER_GEOMETRY:
			mDirect3DDeviceContext->GSSetShaderResources(startSlot, srCount, SRs);
			break;
		case ER_RHI_SHADER_TYPE::ER_TESSELLATION:
			mDirect3DDeviceContext->HSSetShaderResources(startSlot, srCount, SRs);
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

	void ER_RHI_DX11::SetUnorderedAccessResources(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_GPUTexture*>& aUAVs, UINT startSlot /*= 0*/)
	{
		assert(aUAVs.size() > 0);
		assert(aUAVs.size() <= DX11_MAX_BOUND_UNORDERED_ACCESS_VIEWS);

		ID3D11UnorderedAccessView* UAVs[DX11_MAX_BOUND_UNORDERED_ACCESS_VIEWS] = {};
		UINT uavCount = static_cast<UINT>(aUAVs.size());
		for (UINT i = 0; i < uavCount; i++)
		{
			assert(aUAVs[i]);
			UAVs[i] = aUAVs[i]->GetUAV();
		}

		switch (aShaderType)
		{
		case ER_RHI_SHADER_TYPE::ER_VERTEX:
			throw ER_CoreException("ER_RHI_DX11: Binding UAV to this shader stage is not possible."); //TODO possible with 11_3 i think?
			break;
		case ER_RHI_SHADER_TYPE::ER_GEOMETRY:
			throw ER_CoreException("ER_RHI_DX11: Binding UAV to this shader stage is not possible.");
			break;
		case ER_RHI_SHADER_TYPE::ER_TESSELLATION:
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

	void ER_RHI_DX11::SetConstantBuffers(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_GPUBuffer*>& aCBs, UINT startSlot /*= 0*/)
	{
		assert(aCBs.size() > 0);
		assert(aCBs.size() <= DX11_MAX_BOUND_CONSTANT_BUFFERS);

		ID3D11Buffer* CBs[DX11_MAX_BOUND_CONSTANT_BUFFERS] = {};
		UINT cbsCount = static_cast<UINT>(aCBs.size());
		for (UINT i = 0; i < cbsCount; i++)
		{
			assert(aCBs[i]);
			CBs[i] = aCBs[i]->GetBuffer();
		}

		switch (aShaderType)
		{
		case ER_RHI_SHADER_TYPE::ER_VERTEX:
			mDirect3DDeviceContext->VSSetConstantBuffers(startSlot, cbsCount, CBs);
			break;
		case ER_RHI_SHADER_TYPE::ER_GEOMETRY:
			mDirect3DDeviceContext->GSSetConstantBuffers(startSlot, cbsCount, CBs);
			break;
		case ER_RHI_SHADER_TYPE::ER_TESSELLATION:
			mDirect3DDeviceContext->HSSetConstantBuffers(startSlot, cbsCount, CBs);
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

	void ER_RHI_DX11::SetSamplers(ER_RHI_SHADER_TYPE aShaderType, const std::vector<ER_RHI_SAMPLER_STATE>& aSamplers, UINT startSlot /*= 0*/)
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
		case ER_RHI_SHADER_TYPE::ER_TESSELLATION:
			mDirect3DDeviceContext->HSSetSamplers(startSlot, ssCount, SS);
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

	void ER_RHI_DX11::SetIndexBuffer(ER_GPUBuffer* aBuffer, UINT offset /*= 0*/)
	{
		assert(aBuffer);
		mDirect3DDeviceContext->IASetIndexBuffer(aBuffer->GetBuffer(), aBuffer->GetFormat(), offset);
	}

	void ER_RHI_DX11::SetVertexBuffers(const std::vector<ER_GPUBuffer*>& aVertexBuffers)
	{
		assert(aVertexBuffers.size() > 0 && aVertexBuffers.size() < ER_RHI_MAX_BOUND_VERTEX_BUFFERS);
		if (aVertexBuffers.size() == 1)
		{
			UINT stride = aVertexBuffers[0]->GetStride();
			UINT offset = 0;
			assert(aVertexBuffers[0]);
			ID3D11Buffer* bufferPointers[1] = { aVertexBuffers[0]->GetBuffer() };
			mDirect3DDeviceContext->IASetVertexBuffers(0, 1, bufferPointers, &stride, &offset);
		}
		else //+ instance buffer
		{
			UINT strides[2] = { aVertexBuffers[0]->GetStride(), aVertexBuffers[1]->GetStride() };
			UINT offsets[2] = { 0, 0 };

			assert(aVertexBuffers[0]);
			assert(aVertexBuffers[1]);
			ID3D11Buffer* bufferPointers[2] = { aVertexBuffers[0]->GetBuffer(), aVertexBuffers[1]->GetBuffer() };
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
}