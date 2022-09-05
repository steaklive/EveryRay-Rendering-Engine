#include "ER_RHI_DX12_GPUTexture.h"
#include "..\..\ER_Utility.h"
#include "..\..\ER_CoreException.h"

namespace EveryRay_Core
{
	ER_RHI_DX12_GPUTexture::ER_RHI_DX12_GPUTexture()
	{

	}

	ER_RHI_DX12_GPUTexture::~ER_RHI_DX12_GPUTexture()
	{
		ReleaseObject(mResource);

		if (mIsLoadedFromFile)
			return;

		mMipLevels = 0;
		mBindFlags = 0;
		mArraySize = 0;
	}

	void ER_RHI_DX12_GPUTexture::CreateGPUTextureResource(ER_RHI* aRHI, UINT width, UINT height, UINT samples, ER_RHI_FORMAT format, ER_RHI_BIND_FLAG bindFlags /*= ER_BIND_SHADER_RESOURCE | ER_BIND_RENDER_TARGET*/, int mip /*= 1*/, int depth /*= -1*/, int arraySize /*= 1*/, bool isCubemap /*= false*/, int cubemapArraySize /*= -1*/)
	{
		assert(aRHI);
		ER_RHI_DX12* aRHIDX12 = static_cast<ER_RHI_DX12*>(aRHI);
		ID3D12Device* device = aRHIDX12->GetDevice();
		assert(device);

		ER_RHI_DX12_GPUDescriptorHeapManager* descriptorHeapManager = aRHIDX12->GetDescriptorHeapManager();
		assert(descriptorHeapManager);

		mIsLoadedFromFile = false;

		//todo move to init list
		mArraySize = arraySize;
		mMipLevels = mip;
		mBindFlags = bindFlags;
		mDepth = depth;
		mWidth = width;
		mHeight = height;
		mIsCubemap = isCubemap;
		mIsDepthStencil = bindFlags & ER_BIND_DEPTH_STENCIL;

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
		D3D12_RESOURCE_FLAGS flags;
		if (bindFlags & ER_BIND_RENDER_TARGET)
			flags |= D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		if (bindFlags & ER_BIND_DEPTH_STENCIL)
			flags |= D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		if (bindFlags & ER_BIND_UNORDERED_ACCESS)
			flags |= D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		XMFLOAT4 clearColor = { 0, 0, 0, 1 };
		D3D12_CLEAR_VALUE optimizedClearValue = {};
		optimizedClearValue.Format = mIsDepthStencil ? depthstencil_tex_format : aRHIDX12->GetFormat(format);
		optimizedClearValue.Color[0] = clearColor.x;
		optimizedClearValue.Color[1] = clearColor.y;
		optimizedClearValue.Color[2] = clearColor.z;
		optimizedClearValue.Color[3] = clearColor.w;

		D3D12_RESOURCE_DESC textureDesc = {};
		if (depth == 0)
			textureDesc.DepthOrArraySize = (mIsCubemap && cubemapArraySize > 0) ? 6 * cubemapArraySize : arraySize;
		else
			textureDesc.DepthOrArraySize = depth;
		textureDesc.MipLevels = mMipLevels;
		textureDesc.Format = mIsDepthStencil ? depthstencil_tex_format : aRHIDX12->GetFormat(format);
		textureDesc.Width = width;
		textureDesc.Height = height;
		textureDesc.Flags = flags;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = depth > 0 ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		if (FAILED(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &textureDesc, mCurrentResourceState, &optimizedClearValue, &mResource)))
			throw ER_CoreException("ER_RHI_DX12: Could not create a committed resource for the GPU texture");

		if (bindFlags & ER_BIND_RENDER_TARGET)
		{
			mRTVHandles.resize(mip * arraySize);
			assert(!mIsDepthStencil);

			D3D12_RENDER_TARGET_VIEW_DESC rDesc;
			rDesc.Format = aRHIDX12->GetFormat(format);
			if (arraySize == 1)
			{
				rDesc.ViewDimension = (depth > 0) ? D3D12_RTV_DIMENSION_TEXTURE3D : ((samples > 1) ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D);
				int currentDepth = depth;
				for (int i = 0; i < mip; i++)
				{
					mRTVHandles[i] = descriptorHeapManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
					if (depth > 0) {
						rDesc.Texture3D.MipSlice = i;
						rDesc.Texture3D.FirstWSlice = 0;
						rDesc.Texture3D.WSize = currentDepth;
						currentDepth >>= 1;
					}
					else {
						rDesc.Texture2D.MipSlice = i;
					}
					device->CreateRenderTargetView(mResource, &rDesc, mRTVHandles[i].GetCPUHandle());
				}
			}
			else
			{
				rDesc.ViewDimension = (samples > 1) ? D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
				for (int i = 0; i < arraySize; i++)
				{
					for (int j = 0; j < mip; j++)
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
						mRTVHandles[i * mip + j] = descriptorHeapManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
						device->CreateRenderTargetView(mResource, &rDesc, mRTVHandles[i * mip + j].GetCPUHandle());
					}
				}
			}

		}
		if (bindFlags & ER_BIND_DEPTH_STENCIL)
		{
			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
			dsvDesc.Flags = 0;
			dsvDesc.Format = depthstencil_dsv_format;
			if (depth == 1)
			{
				if (samples > 1)
				{
					dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
				}
				else
				{
					dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
					dsvDesc.Texture2D.MipSlice = 0;
				}
			}
			else
			{
				if (samples > 1)
				{
					dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
					dsvDesc.Texture2DMSArray.FirstArraySlice = 0;
					dsvDesc.Texture2DMSArray.ArraySize = depth;
				}
				else
				{
					dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
					dsvDesc.Texture2DArray.MipSlice = 0;
					dsvDesc.Texture2DArray.FirstArraySlice = 0;
					dsvDesc.Texture2DArray.ArraySize = depth;
				}
			}

			mDSVHandle = descriptorHeapManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
			device->CreateDepthStencilView(mResource, &dsvDesc, mDSVHandle.GetCPUHandle());

			dsvDesc.Flags |= D3D12_DSV_FLAG_READ_ONLY_DEPTH;
			if (depthstencil_dsv_format == DXGI_FORMAT_D24_UNORM_S8_UINT)
				dsvDesc.Flags |= D3D12_DSV_FLAG_READ_ONLY_STENCIL;
			device->CreateDepthStencilView(mResource, &dsvDesc, mDSVReadOnlyHandle.GetCPUHandle());
		}
		if (bindFlags & ER_BIND_UNORDERED_ACCESS)
		{
			assert(!mIsDepthStencil);

			mUAVHandles.resize(mip);

			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
			uavDesc.Buffer.FirstElement = 0;
			uavDesc.Buffer.Flags = 0;
			int depthElements = (depth > 0) ? depth : 1;
			uavDesc.Buffer.NumElements = width * height * depthElements;
			uavDesc.ViewDimension = depth > 0 ? D3D12_UAV_DIMENSION_TEXTURE3D : D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Format = aRHIDX12->GetFormat(format);

			int currentDepth = depth;
			for (int i = 0; i < mip; i++)
			{
				mUAVHandles[i] = descriptorHeapManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				if (currentDepth > 0) {
					uavDesc.Texture3D.MipSlice = i;
					uavDesc.Texture3D.FirstWSlice = 0;
					uavDesc.Texture3D.WSize = currentDepth; //TODO change for proper mip support
					currentDepth >>= 1;
				}
				else {
					uavDesc.Texture2D.MipSlice = i;
				}
				device->CreateUnorderedAccessView(mResource, nullptr, &uavDesc, mUAVHandles[i].GetCPUHandle());
			}

			//TODO add support for cubemap
		}
		if (bindFlags & ER_BIND_SHADER_RESOURCE)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC sDesc = {};
			sDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			sDesc.Format = mIsDepthStencil ? depthstencil_srv_format : aRHIDX12->GetFormat(format);
			mSRVHandle = descriptorHeapManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			if (arraySize > 1)
			{
				if (mIsCubemap)
				{
					if (cubemapArraySize > 0)
					{
						sDesc.TextureCubeArray.MipLevels = mip;
						sDesc.TextureCubeArray.MostDetailedMip = 0;
						sDesc.TextureCubeArray.NumCubes = cubemapArraySize;
						sDesc.TextureCubeArray.First2DArrayFace = 0;
						sDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
					}
					else
					{
						sDesc.TextureCube.MipLevels = mip;
						sDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
					}
				}
				else {
					if (samples > 1)
					{
						sDesc.Texture2DMSArray.FirstArraySlice = 0;
						sDesc.Texture2DMSArray.ArraySize = arraySize;
						sDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
					}
					else
					{
						sDesc.Texture2DArray.MipLevels = mip;
						sDesc.Texture2DArray.ArraySize = arraySize;
						sDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
					}
				}
			}
			else {
				if (depth > 0) {
					sDesc.Texture3D.MipLevels = mip;
					sDesc.Texture3D.MostDetailedMip = 0;
					sDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
				}
				else {
					sDesc.Texture2D.MipLevels = mip;
					sDesc.Texture2D.MostDetailedMip = 0;
					sDesc.ViewDimension = (samples > 1) ? D3D12_SRV_DIMENSION_TEXTURE2DMS : D3D12_SRV_DIMENSION_TEXTURE2D;
				}
			}
			device->CreateShaderResourceView(mResource, &sDesc, mSRVHandle.GetCPUHandle());
		}
	}
	void ER_RHI_DX12_GPUTexture::CreateGPUTextureResource(ER_RHI* aRHI, const std::string& aPath, bool isFullPath /*= false*/, bool is3D)
	{
		CreateGPUTextureResource(aRHI, EveryRay_Core::ER_Utility::ToWideString(aPath), isFullPath, is3D);
	}
	void ER_RHI_DX12_GPUTexture::CreateGPUTextureResource(ER_RHI* aRHI, const std::wstring& aPath, bool isFullPath /*= false*/, bool is3D)
	{
		assert(aRHI);
		ER_RHI_DX11* aRHIDX11 = static_cast<ER_RHI_DX11*>(aRHI);
		ID3D11Device* device = aRHIDX11->GetDevice();
		assert(device);
		ID3D11DeviceContext1* context = aRHIDX11->GetContext();
		assert(context);

		mIsLoadedFromFile = true;

		std::wstring originalPath = aPath;
		const wchar_t* postfixDDS = L".dds";
		const wchar_t* postfixDDS_Capital = L".DDS";
		bool isDDS = (originalPath.substr(originalPath.length() - 4) == std::wstring(postfixDDS)) || (originalPath.substr(originalPath.length() - 4) == std::wstring(postfixDDS_Capital));

		ID3D11Resource* resourceTex = NULL;

		auto outputLog = [](const std::wstring& pathT) {
			std::wstring msg = L"[ER Logger][ER_RHI_DX11_GPUTexture] Failed to load texture from disk: " + pathT + L". Loading fallback texture instead. \n";
			ER_OUTPUT_LOG(msg.c_str());
		};

		if (isDDS)
		{
			if (FAILED(DirectX::CreateDDSTextureFromFile(device, context, isFullPath ? aPath.c_str() : EveryRay_Core::ER_Utility::GetFilePath(aPath).c_str(), &resourceTex, &mSRV)))
			{
				outputLog(isFullPath ? aPath.c_str() : EveryRay_Core::ER_Utility::GetFilePath(aPath).c_str());
				LoadFallbackTexture(aRHI, &resourceTex, &mSRV);
			}
		}
		else
		{
			if (FAILED(DirectX::CreateWICTextureFromFile(device, context, isFullPath ? aPath.c_str() : EveryRay_Core::ER_Utility::GetFilePath(aPath).c_str(), &resourceTex, &mSRV)))
			{
				outputLog(isFullPath ? aPath.c_str() : EveryRay_Core::ER_Utility::GetFilePath(aPath).c_str());
				LoadFallbackTexture(aRHI, &resourceTex, &mSRV);
			}
		}

		if (!is3D)
		{
			if (FAILED(resourceTex->QueryInterface(IID_ID3D11Texture2D, (void**)&mTexture2D))) {
				throw EveryRay_Core::ER_CoreException("ER_RHI_DX11: Could not cast loaded texture resource to Texture2D. Maybe wrong dimension?");
			}
		}
		else
		{
			if (FAILED(resourceTex->QueryInterface(IID_ID3D11Texture3D, (void**)&mTexture3D))) {
				throw EveryRay_Core::ER_CoreException("ER_RHI_DX11: Could not cast loaded texture resource to Texture3D. Maybe wrong dimension?");
			}
		}

		resourceTex->Release();
	}

	void ER_RHI_DX12_GPUTexture::LoadFallbackTexture(ER_RHI* aRHI, ID3D11Resource** texture, ID3D11ShaderResourceView** textureView)
	{
		assert(aRHI);
		ER_RHI_DX12* aRHIDX12 = static_cast<ER_RHI_DX12*>(aRHI);
		ID3D12Device* device = aRHIDX12->GetDevice();
		assert(device);
		ID3D11DeviceContext1* context = aRHIDX11->GetContext();
		assert(context);

		DirectX::CreateWICTextureFromFile(device, context, EveryRay_Core::ER_Utility::GetFilePath(L"content\\textures\\uvChecker.jpg").c_str(), texture, textureView);
	}
}
