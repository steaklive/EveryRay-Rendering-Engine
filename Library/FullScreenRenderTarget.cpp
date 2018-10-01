#include "stdafx.h"

#include "FullScreenRenderTarget.h"
#include "Game.h"
#include "GameException.h"

namespace Library
{
	FullScreenRenderTarget::FullScreenRenderTarget(Game& game)
		: mGame(&game), mRenderTargetView(nullptr), mDepthStencilView(nullptr), mOutputTexture(nullptr)
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

		if (FAILED(hr = game.Direct3DDevice()->CreateShaderResourceView(fullScreenTexture, nullptr, &mOutputTexture)))
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
		depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthStencilDesc.SampleDesc.Count = 1;
		depthStencilDesc.SampleDesc.Quality = 0;
		depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;

		ID3D11Texture2D* depthStencilBuffer = nullptr;
		if (FAILED(hr = game.Direct3DDevice()->CreateTexture2D(&depthStencilDesc, nullptr, &depthStencilBuffer)))
		{
			throw GameException("IDXGIDevice::CreateTexture2D() failed.", hr);
		}

		if (FAILED(hr = game.Direct3DDevice()->CreateDepthStencilView(depthStencilBuffer, nullptr, &mDepthStencilView)))
		{
			ReleaseObject(depthStencilBuffer);
			throw GameException("IDXGIDevice::CreateDepthStencilView() failed.", hr);
		}

		ReleaseObject(depthStencilBuffer);
	}

	FullScreenRenderTarget::~FullScreenRenderTarget()
	{
		ReleaseObject(mOutputTexture);
		ReleaseObject(mDepthStencilView);
		ReleaseObject(mRenderTargetView);
	}

	ID3D11ShaderResourceView* FullScreenRenderTarget::OutputTexture() const
	{
		return mOutputTexture;
	}

	ID3D11RenderTargetView* FullScreenRenderTarget::RenderTargetView() const
	{
		return mRenderTargetView;
	}

	ID3D11DepthStencilView* FullScreenRenderTarget::DepthStencilView() const
	{
		return mDepthStencilView;
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