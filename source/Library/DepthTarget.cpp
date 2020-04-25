#include "..\Library\DepthTarget.h"

DepthTarget* DepthTarget::Create(ID3D11Device * device, UINT width, UINT height, UINT samples, DXGI_FORMAT format, uint32_t slices)
{
	ID3D11Texture2D * tex = nullptr;
	ID3D11ShaderResourceView * srv = nullptr;
	ID3D11DepthStencilView * dsv = nullptr;
	ID3D11DepthStencilView * dsv_ro = nullptr;

	DXGI_FORMAT tex_format;
	DXGI_FORMAT srv_format;
	DXGI_FORMAT dsv_format;
	
	switch (format)
	{
	case DXGI_FORMAT_D32_FLOAT:
		tex_format = DXGI_FORMAT_R32_TYPELESS;
		srv_format = DXGI_FORMAT_R32_FLOAT;
		dsv_format = DXGI_FORMAT_D32_FLOAT;
		break;

	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		tex_format = DXGI_FORMAT_R24G8_TYPELESS;
		srv_format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		dsv_format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		break;

	case DXGI_FORMAT_D16_UNORM:
		tex_format = DXGI_FORMAT_R16_TYPELESS;
		srv_format = DXGI_FORMAT_R16_UNORM;
		dsv_format = DXGI_FORMAT_D16_UNORM;
		break;

	default:
		return nullptr;
	}

	CD3D11_TEXTURE2D_DESC texDesc;
	texDesc.ArraySize = slices;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
	texDesc.CPUAccessFlags = 0;
	texDesc.Format = tex_format;
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.MipLevels = 1;
	texDesc.MiscFlags = 0;
	if (samples > 1)
	{
		texDesc.SampleDesc.Count = samples;

		UINT multiSampleLevel;
		device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, samples, &multiSampleLevel);

		texDesc.SampleDesc.Quality = multiSampleLevel - 1;
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
	srvDesc.Format = srv_format;
	if (slices == 1)
	{
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
	}
	else
	{
		if (samples > 1)
		{
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
			srvDesc.Texture2DMSArray.FirstArraySlice = 0;
			srvDesc.Texture2DMSArray.ArraySize = slices;
		}
		else
		{
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2DArray.MostDetailedMip = 0;
			srvDesc.Texture2DArray.MipLevels = 1;
			srvDesc.Texture2DArray.FirstArraySlice = 0;
			srvDesc.Texture2DArray.ArraySize = slices;
		}
	}
	device->CreateShaderResourceView(tex, &srvDesc, &srv);

	if (srv == nullptr)
	{
		tex->Release();
		return nullptr;
	}

	CD3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = 0;
	dsvDesc.Format = dsv_format;
	if (slices == 1)
	{
		if (samples > 1)
		{
			dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
		}
		else
		{
			dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Texture2D.MipSlice = 0;
		}
	}
	else
	{
		if (samples > 1)
		{
			dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
			dsvDesc.Texture2DMSArray.FirstArraySlice = 0;
			dsvDesc.Texture2DMSArray.ArraySize = slices;
		}
		else
		{
			dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
			dsvDesc.Texture2DArray.MipSlice = 0;
			dsvDesc.Texture2DArray.FirstArraySlice = 0;
			dsvDesc.Texture2DArray.ArraySize = slices;
		}
	}

	device->CreateDepthStencilView(tex, &dsvDesc, &dsv);
	dsvDesc.Flags |= D3D11_DSV_READ_ONLY_DEPTH;
	if (dsv_format == DXGI_FORMAT_D24_UNORM_S8_UINT)
		dsvDesc.Flags |= D3D11_DSV_READ_ONLY_STENCIL;
	device->CreateDepthStencilView(tex, &dsvDesc, &dsv_ro);
	if (dsv == nullptr || dsv_ro == nullptr)
	{
		tex->Release();
		srv->Release();
		return nullptr;
	}

	return new DepthTarget(tex, srv, dsv, dsv_ro, nullptr);
}

DepthTarget::DepthTarget(ID3D11Resource * resource, ID3D11ShaderResourceView * srv, ID3D11DepthStencilView * dsv, ID3D11DepthStencilView * readonly_dsv, ID3D11UnorderedAccessView * uav)
{
	mDSV = dsv;
	mReadonlyDSV = readonly_dsv;
	mSRV = srv;
}

DepthTarget::~DepthTarget()
{
	ReleaseObject(mDSV);
	ReleaseObject(mReadonlyDSV);
}