#pragma once
#include "Common.h"

class ER_GPUTexture 
{
public:
	ER_GPUTexture(ID3D11Device* device, UINT width, UINT height, UINT samples, DXGI_FORMAT format, UINT bindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
		int mip = 1, int depth = -1, int arraySize = 1, bool isCubemap = false, int cubemapArraySize = -1);
	ER_GPUTexture(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& aPath, bool isFullPath = false);
	ER_GPUTexture(ID3D11Device* device, ID3D11DeviceContext* context, const std::wstring& aPath, bool isFullPath = false);
	~ER_GPUTexture();

	ID3D11RenderTargetView* GetRTV() { return mRTVs[0]; }
	ID3D11RenderTargetView** GetRTVs() { return mRTVs; }

	ID3D11ShaderResourceView* GetSRV() { return mSRV; }
	void SetSRV(ID3D11ShaderResourceView* SRV) { mSRV = SRV; }

	ID3D11Texture2D* GetTexture2D() { return mTexture2D; }
	void SetTexture2D(ID3D11Texture2D* t2d) { mTexture2D = t2d; }

	ID3D11Texture3D* GetTexture3D() { return mTexture3D; }
	void SetTexture3D(ID3D11Texture3D* t3d) { mTexture3D = t3d; }

	ID3D11UnorderedAccessView* GetUAV() { return mUAVs[0]; }
	ID3D11UnorderedAccessView** GetUAVs() { return mUAVs; }

	UINT GetMips() { return mMipLevels; }
	UINT GetWidth() { return mWidth; }
	UINT GetHeight() { return mHeight; }
	UINT GetDepth() { return mDepth; }

	bool IsLoadedFromFile() { return mIsLoadedFromFile; }

private:
	ID3D11RenderTargetView** mRTVs = nullptr;
	ID3D11UnorderedAccessView** mUAVs = nullptr;
	ID3D11ShaderResourceView* mSRV = nullptr; 
	ID3D11Texture2D* mTexture2D = nullptr;
	ID3D11Texture3D* mTexture3D = nullptr;

	UINT mMipLevels = 0;
	UINT mBindFlags = 0;
	UINT mWidth = 0;
	UINT mHeight = 0;
	UINT mDepth = 0;
	UINT mArraySize = 0;
	bool mIsCubemap = false;
	bool mIsLoadedFromFile = false;
};
