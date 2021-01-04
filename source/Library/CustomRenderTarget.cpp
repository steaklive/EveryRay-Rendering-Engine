#include "..\Library\CustomRenderTarget.h"

CustomRenderTarget::CustomRenderTarget(ID3D11Device * device, UINT width, UINT height, UINT samples, DXGI_FORMAT format, UINT bindFlags, int mips)
{
	ID3D11Texture2D* tex = nullptr;
	ID3D11ShaderResourceView* srv = nullptr;
	ID3D11RenderTargetView** rtv = nullptr;
	ID3D11UnorderedAccessView** uav = nullptr;

	mMipLevels = mips;
	mBindFlags = bindFlags;

	CD3D11_TEXTURE2D_DESC texDesc;
	texDesc.ArraySize = 1;
	texDesc.BindFlags = bindFlags;
	if (samples == 1)
		texDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	texDesc.CPUAccessFlags = 0;
	texDesc.Format = format;
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.MipLevels = mMipLevels;
	if (mMipLevels > 1 && (D3D11_BIND_RENDER_TARGET & bindFlags) != 0)
		texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
	else
		texDesc.MiscFlags = 0;
	if (samples > 1)
	{
		texDesc.SampleDesc.Count = samples;
		UINT qualityLevels = 0;
		device->CheckMultisampleQualityLevels(format, samples, &qualityLevels);
		texDesc.SampleDesc.Quality = qualityLevels - 1;
	}
	else
	{
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
	}
	texDesc.Usage = D3D11_USAGE_DEFAULT;

	device->CreateTexture2D(&texDesc, NULL, &tex);

	if (bindFlags & D3D11_BIND_RENDER_TARGET)
	{
		D3D11_RENDER_TARGET_VIEW_DESC rDesc;
		rDesc.Format = format;
		rDesc.ViewDimension = (samples > 1 ) ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
		rtv = (ID3D11RenderTargetView**)malloc(sizeof(ID3D11RenderTargetView*) * mips);
		for (int i = 0; i < mips; i++)
		{
			rDesc.Texture2D.MipSlice = i;
			device->CreateRenderTargetView(tex, &rDesc, &rtv[i]);
		}

	}

	if (bindFlags & D3D11_BIND_UNORDERED_ACCESS)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.Flags = 0;
		uavDesc.Buffer.NumElements = width * height;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Format = format;
		uav = (ID3D11UnorderedAccessView**)malloc(sizeof(ID3D11UnorderedAccessView*) * mips);

		for (int i = 0; i < mips; i++)
		{
			uavDesc.Texture2D.MipSlice = i;
			device->CreateUnorderedAccessView(tex, &uavDesc, &(uav[i]));
		}
	}
	if (bindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC sDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC();
		sDesc.Format = format;
		sDesc.Texture2D.MipLevels = mips;
		sDesc.Texture2D.MostDetailedMip = 0;
		sDesc.ViewDimension = (samples > 1) ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
		device->CreateShaderResourceView(tex, &sDesc, &srv);
	}

	mRTVs = rtv;
	mSRV = srv;
	mUAVs = uav;
	mTexture2D = tex;
}

CustomRenderTarget::~CustomRenderTarget()
{
	ReleaseObject(mSRV);

	if (mBindFlags & D3D11_BIND_RENDER_TARGET)
	{
		for (int i = 0; i < mMipLevels; i++)
		{
			ReleaseObject(mRTVs[i]);
		}
	}
	if (mBindFlags & D3D11_BIND_UNORDERED_ACCESS)
	{
		for (int i = 0; i < mMipLevels; i++)
		{
			ReleaseObject(mUAVs[i]);
		}
	}

	ReleaseObject(mTexture2D);

	mMipLevels = 0;
	mBindFlags = 0;
}