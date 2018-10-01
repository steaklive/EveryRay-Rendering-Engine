#pragma once
#include "Common.h"

namespace Library
{
	class IBLCubemapFace
	{
	public:

		IBLCubemapFace(ID3D11Device* device, ID3D11Texture2D* texture, DXGI_FORMAT format, int size, int arrayIndex, int mipCount);
		~IBLCubemapFace();

		ID3D11RenderTargetView*	mRenderTargets[7];
	};
}