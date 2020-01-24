#pragma once
#include "Common.h"

class TGATextureLoader
{
private:
	struct TGAHeader
	{
		unsigned char data1[12];
		unsigned short width;
		unsigned short height;
		unsigned char bpp;
		unsigned char data2;
	};

public:
	TGATextureLoader();
	TGATextureLoader(const TGATextureLoader&);
	~TGATextureLoader();

	bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, const wchar_t* filename, ID3D11ShaderResourceView** resoure);
	void Shutdown();

	ID3D11ShaderResourceView* GetTexture();

private:
	bool LoadTGA(const wchar_t* filename, int& height, int& width);

private:
	unsigned char* mTGAData;
	ID3D11Texture2D* mTexture2D;
};