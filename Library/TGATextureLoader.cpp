#include "stdafx.h"

#include "TGATextureLoader.h"


TGATextureLoader::TGATextureLoader()
{
	mTGAData = 0;
	mTexture2D = nullptr;
}

TGATextureLoader::TGATextureLoader(const TGATextureLoader& other)
{
}

TGATextureLoader::~TGATextureLoader()
{
}

bool TGATextureLoader::Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext, const wchar_t* filename, ID3D11ShaderResourceView** resource)
{
	int height, width;
	unsigned int rowPitch;
	D3D11_TEXTURE2D_DESC textureDesc;
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;

	// Load the targa image data into memory.
	bool result = LoadTGA(filename, height, width);
	if (!result)
	{
		return false;
	}

	// Setup the description of the texture.
	textureDesc.Height = height;
	textureDesc.Width = width;
	textureDesc.MipLevels = 0;
	textureDesc.ArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

	// Create the empty texture.
	HRESULT hResult;
	hResult = device->CreateTexture2D(&textureDesc, NULL, &mTexture2D);
	if (FAILED(hResult))
	{
		return false;
	}

	// Set the row pitch of the targa image data.
	rowPitch = (width * 4) * sizeof(unsigned char);

	// Copy the targa image data into the texture.
	deviceContext->UpdateSubresource(mTexture2D, 0, NULL, mTGAData, rowPitch, 0);

	// Setup the shader resource view description.
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;

	// Create the shader resource view for the texture.
	hResult = device->CreateShaderResourceView(mTexture2D, &srvDesc, resource);
	if (FAILED(hResult))
	{
		return false;
	}

	// Generate mipmaps for this texture.
	//deviceContext->GenerateMips(*resource);

	// Release the targa image data now that the image data has been loaded into the texture.
	delete[] mTGAData;
	mTGAData = 0;

	return true;
}


void TGATextureLoader::Shutdown()
{
	// Release the texture.
	if (mTexture2D)
	{
		mTexture2D->Release();
		mTexture2D = nullptr;
	}

	// Release the targa data.
	if (mTGAData)
	{
		delete[] mTGAData;
		mTGAData = 0;
	}

	return;
}

bool TGATextureLoader::LoadTGA(const wchar_t* filename, int& height, int& width)
{
	int error, bpp, imageSize, index, i, j, k;
	FILE* filePtr;
	unsigned int count;
	TGAHeader targaFileHeader;
	unsigned char* targaImage;


	// Open the targa file for reading in binary.
	error = _wfopen_s(&filePtr, filename, L"rb");
	if (error != 0)
	{
		return false;
	}

	// Read in the file header.
	count = (unsigned int)fread(&targaFileHeader, sizeof(TGAHeader), 1, filePtr);
	if (count != 1)
	{
		return false;
	}

	// Get the important information from the header.
	height = (int)targaFileHeader.height;
	width = (int)targaFileHeader.width;
	bpp = (int)targaFileHeader.bpp;

	// Check that it is 32 bit and not 24 bit.
	if (bpp != 32)
	{
		return false;
	}

	// Calculate the size of the 32 bit image data.
	imageSize = width * height * 4;

	// Allocate memory for the targa image data.
	targaImage = new unsigned char[imageSize];
	if (!targaImage)
	{
		return false;
	}

	// Read in the targa image data.
	count = (unsigned int)fread(targaImage, 1, imageSize, filePtr);
	if (count != imageSize)
	{
		return false;
	}

	// Close the file.
	error = fclose(filePtr);
	if (error != 0)
	{
		return false;
	}

	// Allocate memory for the targa destination data.
	mTGAData = new unsigned char[imageSize];
	if (!mTGAData)
	{
		return false;
	}

	// Initialize the index into the targa destination data array.
	index = 0;

	// Initialize the index into the targa image data.
	k = (width * height * 4) - (width * 4);

	// Now copy the targa image data into the targa destination array in the correct order since the targa format is stored upside down.
	for (j = 0; j < height; j++)
	{
		for (i = 0; i < width; i++)
		{
			mTGAData[index + 0] = targaImage[k + 2];  // Red.
			mTGAData[index + 1] = targaImage[k + 1];  // Green.
			mTGAData[index + 2] = targaImage[k + 0];  // Blue
			mTGAData[index + 3] = targaImage[k + 3];  // Alpha

			// Increment the indexes into the targa data.
			k += 4;
			index += 4;
		}

		// Set the targa image data index back to the preceding row at the beginning of the column since its reading it in upside down.
		k -= (width * 8);
	}

	// Release the targa image data now that it was copied into the destination array.
	delete[] targaImage;
	targaImage = 0;

	return true;
}