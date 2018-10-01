#include "Common.h"
#include "IBLCubemapFace.h"

namespace Library
{
	IBLCubemapFace::IBLCubemapFace(ID3D11Device* device, ID3D11Texture2D* texture, DXGI_FORMAT format, int size, int arrayIndex, int mipCount)
	{
		for (int mipIndex = 0; mipIndex < mipCount; mipIndex++)
		{
			D3D11_RENDER_TARGET_VIEW_DESC texRenderTargetViewDesc;

			ZeroMemory(&texRenderTargetViewDesc, sizeof(D3D11_RENDER_TARGET_VIEW_DESC));

			texRenderTargetViewDesc.Format = format;
			texRenderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			texRenderTargetViewDesc.Texture2DArray.MipSlice = mipIndex;
			texRenderTargetViewDesc.Texture2DArray.ArraySize = 1;
			texRenderTargetViewDesc.Texture2DArray.FirstArraySlice = arrayIndex;

			device->CreateRenderTargetView(texture,	&texRenderTargetViewDesc, &mRenderTargets[mipIndex]);
		}
	}


	IBLCubemapFace::~IBLCubemapFace()
	{
		delete mRenderTargets;
	}
}