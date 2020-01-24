#include "stdafx.h"

#include <stack>
#include "RenderTarget.h"
#include "Game.h"

namespace Library
{
	RTTI_DEFINITIONS(RenderTarget)

	std::stack<RenderTarget::RenderTargetData> RenderTarget::sRenderTargetStack;

	RenderTarget::RenderTarget()
	{
	}

	RenderTarget::~RenderTarget()
	{
	}

	void RenderTarget::Begin(ID3D11DeviceContext* deviceContext, UINT viewCount, ID3D11RenderTargetView** renderTargetViews, ID3D11DepthStencilView* depthStencilView, const D3D11_VIEWPORT& viewport)
	{
		sRenderTargetStack.push(RenderTargetData(viewCount, renderTargetViews, depthStencilView, viewport));
		deviceContext->OMSetRenderTargets(viewCount, renderTargetViews, depthStencilView);
		deviceContext->RSSetViewports(1, &viewport);
	}

	void RenderTarget::End(ID3D11DeviceContext* deviceContext)
	{
		sRenderTargetStack.pop();

		RenderTargetData renderTargetData = sRenderTargetStack.top();
		deviceContext->OMSetRenderTargets(renderTargetData.ViewCount, renderTargetData.RenderTargetViews, renderTargetData.DepthStencilView);
		deviceContext->RSSetViewports(1, &renderTargetData.Viewport);
	}
}