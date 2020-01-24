#include "stdafx.h"

#include "FullScreenRenderTarget.h"
#include "Game.h"
#include "GameException.h"

namespace Library
{
	FullScreenRenderTarget::FullScreenRenderTarget(Game& game)
		: mGame(&game), mRenderTargetView(nullptr), mDepthStencilView(nullptr), mOutputColorTexture(nullptr), mOutputDepthTexture(nullptr)
	{
		D3D11_TEXTURE2D_DESC fullScreenTextureDesc;
		ZeroMemory(&fullScreenTextureDesc, sizeof(fullScreenTextureDesc));
		fullScreenTextureDesc.Width = game.ScreenWidth();
		fullScreenTextureDesc.Height = game.ScreenHeight();
		fullScreenTextureDesc.MipLevels = 1;
		fullScreenTextureDesc.ArraySize = 1;
		fullScreenTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		fullScreenTextureDesc.SampleDesc.Count = 1;
		fullScreenTextureDesc.SampleDesc.Quality = 0;
		fullScreenTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		fullScreenTextureDesc.Usage = D3D11_USAGE_DEFAULT;

		HRESULT hr;
		ID3D11Texture2D* fullScreenTexture = nullptr;
		if (FAILED(hr = game.Direct3DDevice()->CreateTexture2D(&fullScreenTextureDesc, nullptr, &fullScreenTexture)))
		{
			throw GameException("IDXGIDevice::CreateTexture2D() failed.", hr);
		}

		if (FAILED(hr = game.Direct3DDevice()->CreateShaderResourceView(fullScreenTexture, nullptr, &mOutputColorTexture)))
		{
			throw GameException("IDXGIDevice::CreateShaderResourceView() failed.", hr);
		}

		if (FAILED(hr = game.Direct3DDevice()->CreateRenderTargetView(fullScreenTexture, nullptr, &mRenderTargetView)))
		{
			ReleaseObject(fullScreenTexture);
			throw GameException("IDXGIDevice::CreateRenderTargetView() failed.", hr);
		}

		ReleaseObject(fullScreenTexture);

		D3D11_TEXTURE2D_DESC depthStencilDesc;
		ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));
		depthStencilDesc.Width = game.ScreenWidth();
		depthStencilDesc.Height = game.ScreenHeight();
		depthStencilDesc.MipLevels = 1;
		depthStencilDesc.ArraySize = 1;
		depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
		depthStencilDesc.SampleDesc.Count = 1;
		depthStencilDesc.SampleDesc.Quality = 0;
		depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		//depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;

		ID3D11Texture2D* depthStencilBuffer = nullptr;
		if (FAILED(hr = game.Direct3DDevice()->CreateTexture2D(&depthStencilDesc, nullptr, &depthStencilBuffer)))
		{
			throw GameException("IDXGIDevice::CreateTexture2D() failed.", hr);
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC resourceViewDesc;
		ZeroMemory(&resourceViewDesc, sizeof(resourceViewDesc));
		resourceViewDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		resourceViewDesc.ViewDimension = D3D_SRV_DIMENSION_TEXTURE2D;
		resourceViewDesc.Texture2D.MipLevels = 1;
		if (FAILED(hr = game.Direct3DDevice()->CreateShaderResourceView(depthStencilBuffer, &resourceViewDesc, &mOutputDepthTexture)))
		{
			throw GameException("IDXGIDevice::CreateShaderResourceView() failed.", hr);
		}

		D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
		ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));
		depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		depthStencilViewDesc.Texture2D.MipSlice = 0;
		if (FAILED(hr = game.Direct3DDevice()->CreateDepthStencilView(depthStencilBuffer, &depthStencilViewDesc, &mDepthStencilView)))
		{
			ReleaseObject(depthStencilBuffer);
			throw GameException("IDXGIDevice::CreateDepthStencilView() failed.", hr);
		}


		ReleaseObject(depthStencilBuffer);
	}

	//FullScreenRenderTarget::FullScreenRenderTarget(Game& game,
	//	ID3D11RenderTargetView* rtv, ID3D11DepthStencilView* dsv)
	//	: mGame(&game), mRenderTargetView(rtv), mDepthStencilView(dsv), mOutputColorTexture(nullptr), mOutputDepthTexture(nullptr)
	//{
	//	D3D11_TEXTURE2D_DESC fullScreenTextureDesc;
	//	ZeroMemory(&fullScreenTextureDesc, sizeof(fullScreenTextureDesc));
	//	fullScreenTextureDesc.Width = game.ScreenWidth();
	//	fullScreenTextureDesc.Height = game.ScreenHeight();
	//	fullScreenTextureDesc.MipLevels = 1;
	//	fullScreenTextureDesc.ArraySize = 1;
	//	fullScreenTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	//	fullScreenTextureDesc.SampleDesc.Count = 1;
	//	fullScreenTextureDesc.SampleDesc.Quality = 0;
	//	fullScreenTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	//	fullScreenTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	//
	//	HRESULT hr;
	//	ID3D11Texture2D* fullScreenTexture = nullptr;
	//	if (FAILED(hr = game.Direct3DDevice()->CreateTexture2D(&fullScreenTextureDesc, nullptr, &fullScreenTexture)))
	//	{
	//		throw GameException("IDXGIDevice::CreateTexture2D() failed.", hr);
	//	}
	//
	//	if (FAILED(hr = game.Direct3DDevice()->CreateShaderResourceView(fullScreenTexture, nullptr, &mOutputColorTexture)))
	//	{
	//		throw GameException("IDXGIDevice::CreateShaderResourceView() failed.", hr);
	//	}
	//
	//	if (FAILED(hr = game.Direct3DDevice()->CreateRenderTargetView(fullScreenTexture, nullptr, &mRenderTargetView)))
	//	{
	//		ReleaseObject(fullScreenTexture);
	//		throw GameException("IDXGIDevice::CreateRenderTargetView() failed.", hr);
	//	}
	//
	//	ReleaseObject(fullScreenTexture);
	//
	//	D3D11_TEXTURE2D_DESC depthStencilDesc;
	//	ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));
	//	depthStencilDesc.Width = game.ScreenWidth();
	//	depthStencilDesc.Height = game.ScreenHeight();
	//	depthStencilDesc.MipLevels = 1;
	//	depthStencilDesc.ArraySize = 1;
	//	depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	//	depthStencilDesc.SampleDesc.Count = 1;
	//	depthStencilDesc.SampleDesc.Quality = 0;
	//	depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	//	//depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
	//
	//	ID3D11Texture2D* depthStencilBuffer = nullptr;
	//	if (FAILED(hr = game.Direct3DDevice()->CreateTexture2D(&depthStencilDesc, nullptr, &depthStencilBuffer)))
	//	{
	//		throw GameException("IDXGIDevice::CreateTexture2D() failed.", hr);
	//	}
	//
	//	D3D11_SHADER_RESOURCE_VIEW_DESC resourceViewDesc;
	//	ZeroMemory(&resourceViewDesc, sizeof(resourceViewDesc));
	//	resourceViewDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	//	resourceViewDesc.ViewDimension = D3D_SRV_DIMENSION_TEXTURE2D;
	//	resourceViewDesc.Texture2D.MipLevels = 1;
	//	if (FAILED(hr = game.Direct3DDevice()->CreateShaderResourceView(depthStencilBuffer, &resourceViewDesc, &mOutputDepthTexture)))
	//	{
	//		throw GameException("IDXGIDevice::CreateShaderResourceView() failed.", hr);
	//	}
	//
	//	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
	//	ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));
	//	depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	//	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	//	depthStencilViewDesc.Texture2D.MipSlice = 0;
	//	if (FAILED(hr = game.Direct3DDevice()->CreateDepthStencilView(depthStencilBuffer, &depthStencilViewDesc, &mDepthStencilView)))
	//	{
	//		ReleaseObject(depthStencilBuffer);
	//		throw GameException("IDXGIDevice::CreateDepthStencilView() failed.", hr);
	//	}
	//
	//	ReleaseObject(depthStencilBuffer);
	//}

	FullScreenRenderTarget::~FullScreenRenderTarget()
	{
		ReleaseObject(mOutputColorTexture);
		ReleaseObject(mOutputDepthTexture);
		ReleaseObject(mDepthStencilView);
		ReleaseObject(mRenderTargetView);
	}

	ID3D11ShaderResourceView* FullScreenRenderTarget::OutputColorTexture() const
	{
		return mOutputColorTexture;
	}	
	ID3D11ShaderResourceView* FullScreenRenderTarget::OutputDepthTexture() const
	{
		return mOutputDepthTexture;
	}

	ID3D11RenderTargetView* FullScreenRenderTarget::RenderTargetView() const
	{
		return mRenderTargetView;
	}

	ID3D11DepthStencilView* FullScreenRenderTarget::DepthStencilView() const
	{
		return mDepthStencilView;
	}

	void FullScreenRenderTarget::SetRTV(ID3D11RenderTargetView* RTV)
	{
		mRenderTargetView = RTV;
	}

	void FullScreenRenderTarget::SetDSV(ID3D11DepthStencilView* DSV)
	{
		mDepthStencilView = DSV;
	}

	void FullScreenRenderTarget::SetSRV(ID3D11ShaderResourceView* SRV)
	{
		mOutputColorTexture = SRV;
	}

	void FullScreenRenderTarget::Begin()
	{
		mGame->Direct3DDeviceContext()->OMSetRenderTargets(1, &mRenderTargetView, mDepthStencilView);
	}

	void FullScreenRenderTarget::End()
	{
		mGame->ResetRenderTargets();
	}
}