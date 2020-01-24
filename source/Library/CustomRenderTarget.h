#pragma once
#include "Common.h"

class CustomRenderTarget 
{
public:
	static CustomRenderTarget* Create(ID3D11Device * device, UINT width, UINT height, UINT samples, DXGI_FORMAT format);
	CustomRenderTarget(ID3D11Resource * resource, ID3D11ShaderResourceView * srv, ID3D11RenderTargetView * rtv, ID3D11UnorderedAccessView * uav);
	~CustomRenderTarget();

	ID3D11RenderTargetView * getRTV() { return mRTV; }
	ID3D11ShaderResourceView* getSRV() { return mSRV; }
protected:
	CustomRenderTarget() {};
	ID3D11RenderTargetView* mRTV;
	ID3D11ShaderResourceView* mSRV;

};
