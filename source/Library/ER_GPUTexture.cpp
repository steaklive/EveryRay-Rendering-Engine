#include "ER_GPUTexture.h"
#include "ER_CoreException.h"
#include "ER_Utility.h"

ER_GPUTexture::ER_GPUTexture(ID3D11Device* device, UINT width, UINT height, UINT samples, DXGI_FORMAT format, UINT bindFlags,
	int mips, int depth, int arraySize, bool isCubemap, int cubemapArraySize) 
{
	ID3D11Texture2D* tex2D = nullptr;
	ID3D11Texture3D* tex3D = nullptr;
	ID3D11ShaderResourceView* srv = nullptr;
	ID3D11RenderTargetView** rtv = nullptr;
	ID3D11UnorderedAccessView** uav = nullptr;
	mIsLoadedFromFile = false;

	//todo move to init list
	mArraySize = arraySize;
	mMipLevels = mips;
	mBindFlags = bindFlags;
	mDepth = depth;
	mWidth = width;
	mHeight = height;
	mIsCubemap = isCubemap;
	mIsDepthStencil = bindFlags & D3D11_BIND_DEPTH_STENCIL;

	DXGI_FORMAT depthstencil_tex_format;
	DXGI_FORMAT depthstencil_srv_format;
	DXGI_FORMAT depthstencil_dsv_format;
	if (mIsDepthStencil)
	{
		switch (format)
		{
		case DXGI_FORMAT_D32_FLOAT:
			depthstencil_tex_format = DXGI_FORMAT_R32_TYPELESS;
			depthstencil_srv_format = DXGI_FORMAT_R32_FLOAT;
			depthstencil_dsv_format = DXGI_FORMAT_D32_FLOAT;
			break;
		case DXGI_FORMAT_D16_UNORM:
			depthstencil_tex_format = DXGI_FORMAT_R16_TYPELESS;
			depthstencil_srv_format = DXGI_FORMAT_R16_UNORM;
			depthstencil_dsv_format = DXGI_FORMAT_D16_UNORM;
			break;
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		default:
			depthstencil_tex_format = DXGI_FORMAT_R24G8_TYPELESS;
			depthstencil_srv_format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			depthstencil_dsv_format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			break;
		}
	}

	if (depth > 0) {
		CD3D11_TEXTURE3D_DESC texDesc;
		texDesc.BindFlags = bindFlags;
		texDesc.CPUAccessFlags = 0;
		texDesc.Format = mIsDepthStencil ? depthstencil_tex_format : format;
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.Depth = depth;
		texDesc.MipLevels = mMipLevels;
		if (mMipLevels > 1 && (D3D11_BIND_RENDER_TARGET & bindFlags) != 0 && !mIsDepthStencil)
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
		if (mMipLevels > 1 && (D3D11_BIND_RENDER_TARGET & bindFlags) != 0 && !mIsDepthStencil)
			texDesc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
		texDesc.CPUAccessFlags = 0;
		texDesc.Format = mIsDepthStencil ? depthstencil_tex_format : format;
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
		assert(!mIsDepthStencil);

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
	if (bindFlags & D3D11_BIND_DEPTH_STENCIL)
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		dsvDesc.Flags = 0;
		dsvDesc.Format = depthstencil_dsv_format;
		if (depth == 1)
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
				dsvDesc.Texture2DMSArray.ArraySize = depth;
			}
			else
			{
				dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
				dsvDesc.Texture2DArray.MipSlice = 0;
				dsvDesc.Texture2DArray.FirstArraySlice = 0;
				dsvDesc.Texture2DArray.ArraySize = depth;
			}
		}

		device->CreateDepthStencilView(tex2D, &dsvDesc, &mDSV);
		dsvDesc.Flags |= D3D11_DSV_READ_ONLY_DEPTH;
		if (depthstencil_dsv_format == DXGI_FORMAT_D24_UNORM_S8_UINT)
			dsvDesc.Flags |= D3D11_DSV_READ_ONLY_STENCIL;
		device->CreateDepthStencilView(tex2D, &dsvDesc, &mDSV_ReadOnly);
	}
	if (bindFlags & D3D11_BIND_UNORDERED_ACCESS)
	{
		assert(!mIsDepthStencil);

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
		sDesc.Format = mIsDepthStencil ? depthstencil_srv_format : format;
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

ER_GPUTexture::ER_GPUTexture(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& aPath, bool isFullPath)
{
	std::string originalPath = aPath;

	mIsLoadedFromFile = true;
	const char* postfixDDS = ".dds";
	const char* postfixDDS_Capital = ".DDS";
	bool isDDS = (originalPath.substr(originalPath.length() - 4) == std::string(postfixDDS)) || (originalPath.substr(originalPath.length() - 4) == std::string(postfixDDS_Capital));
	
	ID3D11Resource* resourceTex = NULL;

	if (isFullPath)
	{
		if (isDDS)
		{
			if (FAILED(DirectX::CreateDDSTextureFromFile(device, context, Library::ER_Utility::ToWideString(aPath).c_str(), &resourceTex, &mSRV)))
			{
				std::string msg = "Failed to load DDS texture from disk: ";
				msg += aPath;
				throw Library::ER_CoreException(msg.c_str());
			}
		}
		else
		{
			if (FAILED(DirectX::CreateWICTextureFromFile(device, context, Library::ER_Utility::ToWideString(aPath).c_str(), &resourceTex, &mSRV)))
			{
				std::string msg = "Failed to load WIC texture from disk: ";
				msg += aPath;
				throw Library::ER_CoreException(msg.c_str());
			}
		}
	}
	else
	{
		if (isDDS)
		{
			if (FAILED(DirectX::CreateDDSTextureFromFile(device, context, Library::ER_Utility::GetFilePath(Library::ER_Utility::ToWideString(aPath)).c_str(), &resourceTex, &mSRV)))
			{
				std::string msg = "Failed to load DDS texture from disk: ";
				msg += aPath;
				throw Library::ER_CoreException(msg.c_str());
			}
		}
		else
		{
			if (FAILED(DirectX::CreateWICTextureFromFile(device, context, Library::ER_Utility::GetFilePath(Library::ER_Utility::ToWideString(aPath)).c_str(), &resourceTex, &mSRV)))
			{
				std::string msg = "Failed to load WIC texture from disk: ";
				msg += aPath;
				throw Library::ER_CoreException(msg.c_str());
			}
		}
	}

	if (FAILED(resourceTex->QueryInterface(IID_ID3D11Texture2D, (void**)&mTexture2D))) {
		throw Library::ER_CoreException("Could not cast loaded texture resource to Texture2D. Maybe wrong dimension?");
	}
}
ER_GPUTexture::ER_GPUTexture(ID3D11Device* device, ID3D11DeviceContext* context, const std::wstring& aPath, bool isFullPath)
{
	mIsLoadedFromFile = true;

	std::wstring originalPath = aPath;
	const wchar_t* postfixDDS = L".dds";
	const wchar_t* postfixDDS_Capital = L".DDS";
	bool isDDS = (originalPath.substr(originalPath.length() - 4) == std::wstring(postfixDDS)) || (originalPath.substr(originalPath.length() - 4) == std::wstring(postfixDDS_Capital));
	
	ID3D11Resource* resourceTex = NULL;

	if (isFullPath)
	{
		if (isDDS)
		{
			if (FAILED(DirectX::CreateDDSTextureFromFile(device, context, aPath.c_str(), &resourceTex, &mSRV)))
			{
				std::string msg = "Failed to load DDS texture from disk: ";
				//msg += aPath;
				throw Library::ER_CoreException(msg.c_str());
			}
		}
		else
		{
			if (FAILED(DirectX::CreateWICTextureFromFile(device, context, aPath.c_str(), &resourceTex, &mSRV)))
			{
				std::string msg = "Failed to load WIC texture from disk: ";
				//msg += aPath;
				throw Library::ER_CoreException(msg.c_str());
			}
		}
	}
	else
	{
		if (isDDS)
		{
			if (FAILED(DirectX::CreateDDSTextureFromFile(device, context, Library::ER_Utility::GetFilePath(aPath).c_str(), &resourceTex, &mSRV)))
			{
				std::string msg = "Failed to load DDS texture from disk: ";
				//msg += aPath;
				throw Library::ER_CoreException(msg.c_str());
			}
		}
		else
		{
			if (FAILED(DirectX::CreateWICTextureFromFile(device, context, Library::ER_Utility::GetFilePath(aPath).c_str(), &resourceTex, &mSRV)))
			{
				std::string msg = "Failed to load WIC texture from disk: ";
				//msg += aPath;
				throw Library::ER_CoreException(msg.c_str());
			}
		}
	}

	if (FAILED(resourceTex->QueryInterface(IID_ID3D11Texture2D, (void**)&mTexture2D))) {
		throw Library::ER_CoreException("Could not cast loaded texture resource to Texture2D. Maybe wrong dimension?");
	}
}

ER_GPUTexture::~ER_GPUTexture()
{
	ReleaseObject(mSRV);
	ReleaseObject(mTexture2D);
	ReleaseObject(mTexture3D);

	if (mIsLoadedFromFile)
		return;

	if (mIsDepthStencil)
	{
		ReleaseObject(mDSV);
		ReleaseObject(mDSV_ReadOnly);
	}

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

	mMipLevels = 0;
	mBindFlags = 0;
	mArraySize = 0;
}