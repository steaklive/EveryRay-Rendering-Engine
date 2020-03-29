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
	}

	void GBuffer::Initialize()
	{
		mAlbedoBuffer = CustomRenderTarget::Create(mGame->Direct3DDevice(), mWidth, mHeight, 4, DXGI_FORMAT_R32G32B32A32_FLOAT);
		mNormalBuffer = CustomRenderTarget::Create(mGame->Direct3DDevice(), mWidth, mHeight, 4, DXGI_FORMAT_R16G16B16A16_FLOAT);
	
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

		ID3D11RenderTargetView* rtvs[] = { mAlbedoBuffer->getRTV(), mNormalBuffer->getRTV() };
		mGame->Direct3DDeviceContext()->OMSetRenderTargets(2, rtvs, mGame->DepthStencilView());

		mGame->Direct3DDeviceContext()->ClearRenderTargetView(rtvs[0], color);
		mGame->Direct3DDeviceContext()->ClearRenderTargetView(rtvs[1], color);

		mGame->Direct3DDeviceContext()->RSSetState(mRS);

		// Set the viewport.
		//mGame->Direct3DDeviceContext()->RSSetViewports(1, &m_viewport);

	}

	void GBuffer::End()
	{
		mGame->ResetRenderTargets();
	}


}
