#include "..\Library\CustomRenderTarget.h"

CustomRenderTarget* CustomRenderTarget::Create(ID3D11Device * device, UINT width, UINT height, UINT samples, DXGI_FORMAT format)
{
	ID3D11Texture2D * tex = nullptr;
	ID3D11ShaderResourceView * srv = nullptr;
	ID3D11RenderTargetView * rtv = nullptr;
	ID3D11UnorderedAccessView * uav = nullptr;

	CD3D11_TEXTURE2D_DESC texDesc;
	texDesc.ArraySize = 1;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	if (samples == 1)
		texDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	texDesc.CPUAccessFlags = 0;
	texDesc.Format = format;
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.MipLevels = 1;
	texDesc.MiscFlags = 0;
	if (samples > 1)
	{
		texDesc.SampleDesc.Count = samples;
		texDesc.SampleDesc.Quality = 0;//static_cast<UINT>(D3D11_STANDARD_MULTISAMPLE_PATTERN);
	}
	else
	{
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
	}
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	device->CreateTexture2D(&texDesc, nullptr, &tex);
	if (tex == nullptr)
	{
		return nullptr;
	}

	CD3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = texDesc.Format;
	if (samples > 1)
	{
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
	}
	else
	{
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
	}
	device->CreateShaderResourceView(tex, &srvDesc, &srv);
	if (srv == nullptr)
	{
		tex->Release();
		return nullptr;
	}

	CD3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.Format = texDesc.Format;
	if (samples > 1)
	{
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
	}
	else
	{
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;
	}
	device->CreateRenderTargetView(tex, &rtvDesc, &rtv);
	if (rtv == nullptr)
	{
		tex->Release();
		srv->Release();
		return nullptr;
	}

	if (samples == 1)
	{
		CD3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
		uavDesc.Format = texDesc.Format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;
		device->CreateUnorderedAccessView(tex, &uavDesc, &uav);
		if (uav == nullptr)
		{
			tex->Release();
			srv->Release();
			rtv->Release();
			return nullptr;
		}
	}

	return new CustomRenderTarget(tex, srv, rtv, uav);
}

CustomRenderTarget::CustomRenderTarget(ID3D11Resource * resource, ID3D11ShaderResourceView * srv, ID3D11RenderTargetView * rtv, ID3D11UnorderedAccessView * uav)
	
{
	mRTV = rtv;
	mSRV = srv;
}

CustomRenderTarget::~CustomRenderTarget()
{
	ReleaseObject(mRTV);
	ReleaseObject(mSRV);
}