#pragma once
#include "Common.h"

class CustomRenderTarget 
{
public:
	static CustomRenderTarget* Create(ID3D11Device * device, UINT width, UINT height, UINT samples, DXGI_FORMAT format, int mip = 1);
	static CustomRenderTarget* Create(ID3D11Device * device, UINT width, UINT height, UINT samples, DXGI_FORMAT format, UINT bindFlags, int mip = 1);
	CustomRenderTarget(ID3D11Texture2D* resource, ID3D11ShaderResourceView * srv, ID3D11RenderTargetView** rtvs, ID3D11UnorderedAccessView** uav);
	CustomRenderTarget(ID3D11Texture2D* resource, ID3D11ShaderResourceView * srv, ID3D11RenderTargetView* rtv, ID3D11UnorderedAccessView* uav);
	~CustomRenderTarget();

	ID3D11RenderTargetView* getRTV() { return mRTVs[0]; }
	ID3D11RenderTargetView** getRTVs() { return mRTVs; }
	ID3D11ShaderResourceView* getSRV() { return mSRV; }
	ID3D11Texture2D* getTexture2D() { return mTexture2D; }
	ID3D11UnorderedAccessView** getUAV() { return mUAVs; }
	int getMips() { return mMipLevels; }
protected:
	CustomRenderTarget() {};
	//some annoying legacy code decisions....
	ID3D11RenderTargetView** mRTVs = nullptr;
	ID3D11RenderTargetView* mRTV = nullptr;
	ID3D11UnorderedAccessView** mUAVs = nullptr;
	ID3D11ShaderResourceView* mSRV = nullptr; 
	ID3D11Texture2D* mTexture2D = nullptr;

	static UINT mBindFlags;
	static int mMipLevels;
};
