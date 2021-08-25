#pragma once
#include "Common.h"

class CustomRenderTarget 
{
public:
	CustomRenderTarget(ID3D11Device * device, UINT width, UINT height, UINT samples, DXGI_FORMAT format, UINT bindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, int mip = 1, UINT depth = -1);
	~CustomRenderTarget();

	ID3D11RenderTargetView* getRTV() { return mRTVs[0]; }
	ID3D11RenderTargetView** getRTVs() { return mRTVs; }
	ID3D11ShaderResourceView* getSRV() { return mSRV; }
	ID3D11Texture2D* getTexture2D() { return mTexture2D; }
	ID3D11Texture3D* getTexture3D() { return mTexture3D; }
	ID3D11UnorderedAccessView* getUAV() { return mUAVs[0]; }
	ID3D11UnorderedAccessView** getUAVs() { return mUAVs; }
	int getMips() { return mMipLevels; }

private:
	ID3D11RenderTargetView** mRTVs = nullptr;
	ID3D11UnorderedAccessView** mUAVs = nullptr;
	ID3D11ShaderResourceView* mSRV = nullptr; 
	ID3D11Texture2D* mTexture2D = nullptr;
	ID3D11Texture3D* mTexture3D = nullptr;
	bool mLegacyCode;

	UINT mBindFlags;
	int mMipLevels;
};
