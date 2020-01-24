#pragma once
#include "IBLCubemapFace.h"

namespace Library 
{
	class IBLCubemap
	{
	public:

		IBLCubemap(ID3D11Device* device, int mipLevels, int size);
		~IBLCubemap();

		ID3D11ShaderResourceView** GetShaderResourceViewAddress()
		{
			return this->mShaderResourceView.GetAddressOf();
		}

		IBLCubemapFace* mSurfaces[6];

	private:

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mShaderResourceView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> mTexture;

	};
}