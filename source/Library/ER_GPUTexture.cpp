#include "ER_GPUTexture.h"

ER_GPUTexture::ER_GPUTexture(ID3D11Device* device, UINT width, UINT height, UINT samples, DXGI_FORMAT format, UINT bindFlags,
	int mips, int depth, int arraySize, bool isCubemap, int cubemapArraySize) 
{
	ID3D11Texture2D* tex2D = nullptr;
	ID3D11Texture3D* tex3D = nullptr;
	ID3D11ShaderResourceView* srv = nullptr;
	ID3D11RenderTargetView** rtv = nullptr;
	ID3D11UnorderedAccessView** uav = nullptr;

	//todo move to init list
	mArraySize = arraySize;
	mMipLevels = mips;
	mBindFlags = bindFlags;
	mDepth = depth;
	mWidth = width;
	mHeight = height;
	mIsCubemap = isCubemap;

	if (depth > 0) {
		CD3D11_TEXTURE3D_DESC texDesc;
		texDesc.BindFlags = bindFlags;
		texDesc.CPUAccessFlags = 0;
		texDesc.Format = format;
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.Depth = depth;
		texDesc.MipLevels = mMipLevels;
		if (mMipLevels > 1 && (D3D11_BIND_RENDER_TARGET & bindFlags) != 0)
			texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
		else
			texDesc.MiscFlags = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		device->CreateTexture3D(&texDesc, NULL, &tex3D);
	}
	else {
		CD3D11_TEXTURE2D_DESC texDesc;
		texDesc.ArraySize = (mIsCubemap && cubemapArraySize > 0) ? 6 * cubemapArraySize : arraySize;
		texDesc.BindFlags = bindFlags;	
		texDesc.MiscFlags = 0;
		if (mIsCubemap)
			texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
		if (mMipLevels > 1 && (D3D11_BIND_RENDER_TARGET & bindFlags) != 0)
			texDesc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
		texDesc.CPUAccessFlags = 0;
		texDesc.Format = format;
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = mMipLevels;

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

		device->CreateTexture2D(&texDesc, NULL, &tex2D);
	}

	if (bindFlags & D3D11_BIND_RENDER_TARGET)
	{
		D3D11_RENDER_TARGET_VIEW_DESC rDesc;
		rDesc.Format = format;
		if (arraySize == 1)
		{
			rDesc.ViewDimension = (depth > 0) ? D3D11_RTV_DIMENSION_TEXTURE3D : ((samples > 1) ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D);
			rtv = (ID3D11RenderTargetView**)malloc(sizeof(ID3D11RenderTargetView*) * mips);
			int currentDepth = depth;
			for (int i = 0; i < mips; i++)
			{
				if (depth > 0) {
					rDesc.Texture3D.MipSlice = i;
					rDesc.Texture3D.FirstWSlice = 0;
					rDesc.Texture3D.WSize = currentDepth;
					device->CreateRenderTargetView(tex3D, &rDesc, &rtv[i]);
					currentDepth >>= 1;
				}
				else {
					rDesc.Texture2D.MipSlice = i;
					device->CreateRenderTargetView(tex2D, &rDesc, &rtv[i]);
				}
			}
		}
		else
		{
			rDesc.ViewDimension = (samples > 1) ? D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			rtv = (ID3D11RenderTargetView**)malloc(sizeof(ID3D11RenderTargetView*) * arraySize * mips);
			for (int i = 0; i < arraySize; i++)
			{
				for (int j = 0; j < mips; j++)
				{
					if (samples > 1)
					{
						rDesc.Texture2DMSArray.FirstArraySlice = i;
						rDesc.Texture2DMSArray.ArraySize = 1;
					}
					else
					{
						rDesc.Texture2DArray.MipSlice = j;
						rDesc.Texture2DArray.FirstArraySlice = i;
						rDesc.Texture2DArray.ArraySize = 1;
					}
					device->CreateRenderTargetView(tex2D, &rDesc, &rtv[i * mips + j]);
				}
			}
		}

	}
	if (bindFlags & D3D11_BIND_UNORDERED_ACCESS)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.Flags = 0;
		int depthElements = (depth > 0) ? depth : 1;
		uavDesc.Buffer.NumElements = width * height * depthElements;
		uavDesc.ViewDimension = depth > 0 ? D3D11_UAV_DIMENSION_TEXTURE3D : D3D11_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Format = format;
		uav = (ID3D11UnorderedAccessView**)malloc(sizeof(ID3D11UnorderedAccessView*) * mips);

		int currentDepth = depth;
		for (int i = 0; i < mips; i++)
		{
			if (currentDepth > 0) {
				uavDesc.Texture3D.MipSlice = i;
				uavDesc.Texture3D.FirstWSlice = 0;
				uavDesc.Texture3D.WSize = currentDepth; //TODO change for proper mip support
				device->CreateUnorderedAccessView(tex3D, &uavDesc, &(uav[i]));
				currentDepth >>= 1;
			}
			else {
				uavDesc.Texture2D.MipSlice = i;
				device->CreateUnorderedAccessView(tex2D, &uavDesc, &(uav[i]));
			}
		}

		//TODO add support for cubemap
	}
	if (bindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC sDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC();
		sDesc.Format = format;
		if (arraySize > 1)
		{
			if (mIsCubemap)
			{
				if (cubemapArraySize > 0)
				{
					sDesc.TextureCubeArray.MipLevels = mips;
					sDesc.TextureCubeArray.MostDetailedMip = 0;
					sDesc.TextureCubeArray.NumCubes = cubemapArraySize;
					sDesc.TextureCubeArray.First2DArrayFace = 0;
					sDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
				}
				else
				{
					sDesc.TextureCube.MipLevels = mips;
					sDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
				}
				device->CreateShaderResourceView(tex2D, &sDesc, &srv);
			}
			else {
				if (samples > 1)
				{
					sDesc.Texture2DMSArray.FirstArraySlice = 0;
					sDesc.Texture2DMSArray.ArraySize = arraySize;
					sDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
				}
				else
				{
					sDesc.Texture2DArray.MipLevels = mips;
					sDesc.Texture2DArray.ArraySize = arraySize;
					sDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
				}
				device->CreateShaderResourceView(tex2D, &sDesc, &srv);
			}
		}
		else {
			if (depth > 0) {
				sDesc.Texture3D.MipLevels = mips;
				sDesc.Texture3D.MostDetailedMip = 0;
				sDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
				device->CreateShaderResourceView(tex3D, &sDesc, &srv);
			}
			else {
				sDesc.Texture2D.MipLevels = mips;
				sDesc.Texture2D.MostDetailedMip = 0;
				sDesc.ViewDimension = (samples > 1) ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
				device->CreateShaderResourceView(tex2D, &sDesc, &srv);
			}
		}
	}

	mRTVs = rtv;
	mSRV = srv;
	mUAVs = uav;
	mTexture2D = tex2D;
	mTexture3D = tex3D;
}

ER_GPUTexture::~ER_GPUTexture()
{
	ReleaseObject(mSRV);

	if (mBindFlags & D3D11_BIND_RENDER_TARGET)
	{
		for (UINT i = 0; i < mArraySize * mMipLevels; i++)
		{
			ReleaseObject(mRTVs[i]);
		}
	}
	if (mBindFlags & D3D11_BIND_UNORDERED_ACCESS)
	{
		for (UINT i = 0; i < mMipLevels; i++)
		{
			ReleaseObject(mUAVs[i]);
		}
	}

	ReleaseObject(mTexture2D);
	ReleaseObject(mTexture3D);

	mMipLevels = 0;
	mBindFlags = 0;
	mArraySize = 0;
}