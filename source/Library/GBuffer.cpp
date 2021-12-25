#include "GBuffer.h"
#include "Game.h"
#include "GameException.h"
#include "DeferredMaterial.h"
#include "RenderingObject.h"
#include "Utility.h"

namespace Library {

	GBuffer::GBuffer(Game& game, Camera& camera, int width, int height):
		DrawableGameComponent(game, camera)
	{
		mWidth = width;
		mHeight = height;
	}

	GBuffer::~GBuffer()
	{
		DeleteObject(mAlbedoBuffer);
		DeleteObject(mNormalBuffer);
		DeleteObject(mPositionsBuffer);
		DeleteObject(mExtraBuffer);
		DeleteObject(mDepthBuffer);
		ReleaseObject(mRS);
	}

	void GBuffer::Initialize()
	{
		mAlbedoBuffer = new CustomRenderTarget(mGame->Direct3DDevice(), mWidth, mHeight, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
		mNormalBuffer = new CustomRenderTarget(mGame->Direct3DDevice(), mWidth, mHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
		mPositionsBuffer = new CustomRenderTarget(mGame->Direct3DDevice(), mWidth, mHeight, 1, DXGI_FORMAT_R32G32B32A32_FLOAT);
		mExtraBuffer = new CustomRenderTarget(mGame->Direct3DDevice(), mWidth, mHeight, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
		
		mDepthBuffer = DepthTarget::Create(mGame->Direct3DDevice(), mWidth, mHeight, 1, DXGI_FORMAT_D24_UNORM_S8_UINT);

		D3D11_RASTERIZER_DESC rasterizerStateDesc;
		ZeroMemory(&rasterizerStateDesc, sizeof(rasterizerStateDesc));
		rasterizerStateDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerStateDesc.CullMode = D3D11_CULL_NONE;
		rasterizerStateDesc.DepthClipEnable = true;
		HRESULT hr = mGame->Direct3DDevice()->CreateRasterizerState(&rasterizerStateDesc, &mRS);
		if (FAILED(hr))
		{
			throw GameException("ID3D11Device::CreateRasterizerState() in GBuffer failed.", hr);
		}
	}

	void GBuffer::Update(const GameTime& time)
	{
	}

	void GBuffer::Start()
	{
		float color[4] = { 0,0,0,0 };

		ID3D11RenderTargetView* rtvs[] = { mAlbedoBuffer->getRTV(), mNormalBuffer->getRTV(), mPositionsBuffer->getRTV(), mExtraBuffer->getRTV() };
		mGame->Direct3DDeviceContext()->OMSetRenderTargets(4, rtvs, mDepthBuffer->getDSV() /*mGame->DepthStencilView()*/);

		mGame->Direct3DDeviceContext()->ClearRenderTargetView(rtvs[0], color);
		mGame->Direct3DDeviceContext()->ClearRenderTargetView(rtvs[1], color);
		mGame->Direct3DDeviceContext()->ClearRenderTargetView(rtvs[2], color);
		mGame->Direct3DDeviceContext()->ClearRenderTargetView(rtvs[3], color);
		mGame->Direct3DDeviceContext()->ClearDepthStencilView(mDepthBuffer->getDSV(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		mGame->Direct3DDeviceContext()->RSSetState(mRS);

		// Set the viewport.
		//mGame->Direct3DDeviceContext()->RSSetViewports(1, &m_viewport);

	}

	void GBuffer::End()
	{
		mGame->ResetRenderTargets();
	}


}
