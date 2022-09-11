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
		mFormat = aRHIDX12->GetFormat(format);

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
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
		if (bindFlags & ER_BIND_RENDER_TARGET)
			flags |= D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		if (bindFlags & ER_BIND_DEPTH_STENCIL)
			flags |= D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		if (bindFlags & ER_BIND_UNORDERED_ACCESS)
			flags |= D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		XMFLOAT4 clearColor = { 0, 0, 0, 1 };
		D3D12_CLEAR_VALUE optimizedClearValue = {};
		optimizedClearValue.Format = mIsDepthStencil ? depthstencil_dsv_format : aRHIDX12->GetFormat(format);
		optimizedClearValue.Color[0] = clearColor.x;
		optimizedClearValue.Color[1] = clearColor.y;
		optimizedClearValue.Color[2] = clearColor.z;
		optimizedClearValue.Color[3] = clearColor.w;

		D3D12_RESOURCE_DESC textureDesc = {};
		if (depth > 0)
			textureDesc.DepthOrArraySize = depth;
		else
			textureDesc.DepthOrArraySize = (mIsCubemap && cubemapArraySize > 0) ? 6 * cubemapArraySize : arraySize;
		textureDesc.MipLevels = mMipLevels;
		textureDesc.Format = mIsDepthStencil ? depthstencil_tex_format : aRHIDX12->GetFormat(format);
		textureDesc.Width = width;
		textureDesc.Height = height;
		textureDesc.Flags = flags;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = depth > 0 ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		bool isRTorDT = (bindFlags & ER_RHI_BIND_FLAG::ER_BIND_DEPTH_STENCIL || bindFlags & ER_RHI_BIND_FLAG::ER_BIND_RENDER_TARGET);

		if (FAILED(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &textureDesc, aRHIDX12->GetState(mCurrentResourceState), isRTorDT ? &optimizedClearValue : nullptr, IID_PPV_ARGS(&mResource))))
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
						rDesc.Texture2D.PlaneSlice = 0;
					}
					device->CreateRenderTargetView(mResource.Get(), &rDesc, mRTVHandles[i].GetCPUHandle());
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
							rDesc.Texture2DArray.PlaneSlice = 0;
						}
						mRTVHandles[i * mip + j] = descriptorHeapManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
						device->CreateRenderTargetView(mResource.Get(), &rDesc, mRTVHandles[i * mip + j].GetCPUHandle());
					}
				}
			}

		}
		if (bindFlags & ER_BIND_DEPTH_STENCIL)
		{
			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
			dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
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
			device->CreateDepthStencilView(mResource.Get(), &dsvDesc, mDSVHandle.GetCPUHandle());

			dsvDesc.Flags |= D3D12_DSV_FLAG_READ_ONLY_DEPTH;
			if (depthstencil_dsv_format == DXGI_FORMAT_D24_UNORM_S8_UINT)
				dsvDesc.Flags |= D3D12_DSV_FLAG_READ_ONLY_STENCIL;
			mDSVReadOnlyHandle = descriptorHeapManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
			device->CreateDepthStencilView(mResource.Get(), &dsvDesc, mDSVReadOnlyHandle.GetCPUHandle());
		}
		if (bindFlags & ER_BIND_UNORDERED_ACCESS)
		{
			assert(!mIsDepthStencil);

			mUAVHandles.resize(mip);

			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
			uavDesc.Buffer.FirstElement = 0;
			uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
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
				device->CreateUnorderedAccessView(mResource.Get(), nullptr, &uavDesc, mUAVHandles[i].GetCPUHandle());
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
						sDesc.Texture2DArray.PlaneSlice = 0;
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
					sDesc.Texture2D.PlaneSlice = 0;
					sDesc.ViewDimension = (samples > 1) ? D3D12_SRV_DIMENSION_TEXTURE2DMS : D3D12_SRV_DIMENSION_TEXTURE2D;
				}
			}
			device->CreateShaderResourceView(mResource.Get(), &sDesc, mSRVHandle.GetCPUHandle());
		}
	}
	void ER_RHI_DX12_GPUTexture::CreateGPUTextureResource(ER_RHI* aRHI, const std::string& aPath, bool isFullPath /*= false*/, bool is3D)
	{
		CreateGPUTextureResource(aRHI, EveryRay_Core::ER_Utility::ToWideString(aPath), isFullPath, is3D);
	}
	void ER_RHI_DX12_GPUTexture::CreateGPUTextureResource(ER_RHI* aRHI, const std::wstring& aPath, bool isFullPath /*= false*/, bool is3D)
	{
		assert(aRHI);
		ER_RHI_DX12* aRHIDX12 = static_cast<ER_RHI_DX12*>(aRHI);
		ID3D12Device* device = aRHIDX12->GetDevice();
		assert(device);

		ER_RHI_DX12_GPUDescriptorHeapManager* descriptorHeapManager = aRHIDX12->GetDescriptorHeapManager();
		assert(descriptorHeapManager);

		mIsLoadedFromFile = true;
		mCurrentResourceState = ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_COPY_DEST;

		std::wstring originalPath = aPath;
		const wchar_t* postfixDDS = L".dds";
		const wchar_t* postfixDDS_Capital = L".DDS";
		bool isDDS = (originalPath.substr(originalPath.length() - 4) == std::wstring(postfixDDS)) || (originalPath.substr(originalPath.length() - 4) == std::wstring(postfixDDS_Capital));

		auto outputLog = [](const std::wstring& pathT) {
			std::wstring msg = L"[ER Logger][ER_RHI_DX12_GPUTexture] Failed to load texture from disk: " + pathT + L". Loading fallback texture instead. \n";
			ER_OUTPUT_LOG(msg.c_str());
		};

		if (isDDS)
		{
			std::unique_ptr<uint8_t[]> ddsData;
			std::vector<D3D12_SUBRESOURCE_DATA> subresources;
			bool isCubemap = false;
			if (FAILED(DirectX::LoadDDSTextureFromFile(device, isFullPath ? aPath.c_str() : EveryRay_Core::ER_Utility::GetFilePath(aPath).c_str(), &mResource, ddsData, subresources, 0, nullptr, &isCubemap)))
			{
				outputLog(isFullPath ? aPath.c_str() : EveryRay_Core::ER_Utility::GetFilePath(aPath).c_str());
				LoadFallbackTexture(aRHI);

				return;
			}

			// Create the GPU upload buffer and update subresources
			const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mResource.Get(), 0, 1);
			if (FAILED(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mResourceUpload))))
				throw ER_CoreException("ER_RHI_DX12: Could not create a committed resource for the GPU texture resource (upload)");

			{
				int cmdIndex = aRHIDX12->GetCurrentGraphicsCommandListIndex();
				auto commandList = aRHIDX12->GetGraphicsCommandList(cmdIndex);
				UpdateSubresources(commandList, mResource.Get(), mResourceUpload.Get(), 0, 0, static_cast<UINT>(subresources.size()), subresources.data());

				auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(mResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				commandList->ResourceBarrier(1, &barrier);

				mCurrentResourceState = ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			}

			mSRVHandle = descriptorHeapManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			D3D12_RESOURCE_DESC desc = mResource->GetDesc();
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = desc.Format;
			if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
			{
				if (desc.DepthOrArraySize > 1)
				{
					srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
					srvDesc.Texture2DArray.MipLevels = desc.MipLevels;
					srvDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
				}
				else
				{
					srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
					srvDesc.Texture2D.MipLevels = desc.MipLevels;
				}
			}
			else if (isCubemap)
			{
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
				srvDesc.TextureCube.MipLevels = desc.MipLevels;
			}
			else if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
			{
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
				srvDesc.Texture3D.MipLevels = desc.MipLevels;
			}
			device->CreateShaderResourceView(mResource.Get(), &srvDesc, mSRVHandle.GetCPUHandle());
		}
		else
		{
			std::unique_ptr<uint8_t[]> decodedData;
			D3D12_SUBRESOURCE_DATA subresource;

			if (FAILED(DirectX::LoadWICTextureFromFile(device, isFullPath ? aPath.c_str() : EveryRay_Core::ER_Utility::GetFilePath(aPath).c_str(), &mResource, decodedData, subresource)))
			{
				outputLog(isFullPath ? aPath.c_str() : EveryRay_Core::ER_Utility::GetFilePath(aPath).c_str());
				LoadFallbackTexture(aRHI);

				return;
			}
			// Create the GPU upload buffer and update subresources
			const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mResource.Get(), 0, 1);
			if (FAILED(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mResourceUpload))))
				throw ER_CoreException("ER_RHI_DX12: Could not create a committed resource for the GPU texture resource (upload)");

			{
				int cmdIndex = aRHIDX12->GetCurrentGraphicsCommandListIndex();
				auto commandList = aRHIDX12->GetGraphicsCommandList(cmdIndex);
				UpdateSubresources(commandList, mResource.Get(), mResourceUpload.Get(), 0, 0, 1, &subresource);

				auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(mResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				commandList->ResourceBarrier(1, &barrier);

				mCurrentResourceState = ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			}

			mSRVHandle = descriptorHeapManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			D3D12_RESOURCE_DESC desc = mResource->GetDesc();
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = desc.Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			device->CreateShaderResourceView(mResource.Get(), &srvDesc, mSRVHandle.GetCPUHandle());
		}
	}

	void ER_RHI_DX12_GPUTexture::LoadFallbackTexture(ER_RHI* aRHI)
	{
		assert(aRHI);
		ER_RHI_DX12* aRHIDX12 = static_cast<ER_RHI_DX12*>(aRHI);
		ID3D12Device* device = aRHIDX12->GetDevice();
		assert(device);
		ER_RHI_DX12_GPUDescriptorHeapManager* descriptorHeapManager = aRHIDX12->GetDescriptorHeapManager();
		assert(descriptorHeapManager);

		std::unique_ptr<uint8_t[]> decodedData;
		D3D12_SUBRESOURCE_DATA subresource;
		DirectX::LoadWICTextureFromFile(device, EveryRay_Core::ER_Utility::GetFilePath(L"content\\textures\\uvChecker.jpg").c_str(), &mResource, decodedData, subresource);

		// Create the GPU upload buffer and update subresources
		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mResource.Get(), 0, 1);
		if (FAILED(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mResourceUpload))))
			throw ER_CoreException("ER_RHI_DX12: Could not create a committed resource for the GPU texture resource (upload)");

		{
			int cmdIndex = aRHIDX12->GetCurrentGraphicsCommandListIndex();
			auto commandList = aRHIDX12->GetGraphicsCommandList(cmdIndex);
			UpdateSubresources(commandList, mResource.Get(), mResourceUpload.Get(), 0, 0, 1, &subresource);

			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(mResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			commandList->ResourceBarrier(1, &barrier);

			mCurrentResourceState = ER_RHI_RESOURCE_STATE::ER_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		}

		mSRVHandle = descriptorHeapManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		D3D12_RESOURCE_DESC desc = mResource->GetDesc();
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = desc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(mResource.Get(), &srvDesc, mSRVHandle.GetCPUHandle());
	}
}
