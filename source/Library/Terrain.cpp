#include "stdafx.h"
#include <stdio.h>

#include "Terrain.h"
#include "GameException.h"
#include "Model.h"
#include "Mesh.h"
#include "Game.h"
#include "MatrixHelper.h"
#include "Utility.h"
#include "VertexDeclarations.h"
#include "BasicMaterial.h"
#include "RasterizerStates.h"

namespace Library
{

	Terrain::Terrain(std::string path, Game& pGame, Camera& camera, float cellScale, bool isWireframe) :
		GameComponent(pGame),
		mCamera(camera), 
		/*mSize(size),*/
		mCellScale(cellScale), 
		mWorldMatrix(XMMatrixIdentity()),
		mIsWireframe(isWireframe)
	{
		Effect* effect = new Effect(pGame);
		effect->CompileFromFile(Utility::GetFilePath(L"content\\effects\\BasicEffect.fx"));

		mMaterial = new BasicMaterial();
		mMaterial->Initialize(effect);

		LoadTextures(path);
		GenerateMesh();
	}

	Terrain::~Terrain()
	{
		DeleteObject(mMaterial);
		ReleaseObject(mVertexBuffer);
		ReleaseObject(mIndexBuffer);
		DeleteObjects(mHeightMap);
	}

	void Terrain::Draw()
	{
		ID3D11DeviceContext* context = GetGame()->Direct3DDeviceContext();
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		
		Pass* pass = mMaterial->CurrentTechnique()->Passes().at(0);
		ID3D11InputLayout* inputLayout = mMaterial->InputLayouts().at(pass);
		context->IASetInputLayout(inputLayout);

		UINT stride = sizeof(VertexPositionColor);
		UINT offset = 0;
		context->IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &offset);
		context->IASetIndexBuffer(mIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

		XMMATRIX wvp = mWorldMatrix * mCamera.ViewMatrix() * mCamera.ProjectionMatrix();
		mMaterial->WorldViewProjection() << wvp;

		pass->Apply(0, context);

		if (mIsWireframe)
		{
			context->RSSetState(RasterizerStates::Wireframe);
			context->DrawIndexed(mIndexCount, 0, 0);
			context->RSSetState(nullptr);
		}
		else
			context->DrawIndexed(mIndexCount, 0, 0);
	}

	void Terrain::GenerateMesh()
	{
		InitializeGrid();
	}

	void Terrain::InitializeGrid()
	{
		ID3D11Device* device = GetGame()->Direct3DDevice();

		mVertexCount = (mWidth - 1) * (mHeight - 1) * 12;
		int size = sizeof(VertexPositionColor) * mVertexCount;

		VertexPositionColor* vertices = new VertexPositionColor[mVertexCount];

		unsigned long* indices;
		mIndexCount = mVertexCount;
		indices = new unsigned long[mIndexCount];

		XMFLOAT4 vertexColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		//float adjustedScale = mCellScale;// *0.1f;
		//float maxPosition = mSize * adjustedScale / 2;

		//for (unsigned int i = 0, j = 0; i < mSize + 1; i++, j = 4 * i)
		//{
		//	float position = maxPosition - (i * adjustedScale);
		//
		//	// Vertical line
		//	vertices[j] = VertexPositionColor(XMFLOAT4(position, 0.0f, maxPosition, 1.0f), vertexColor);
		//	vertices[j + 1] = VertexPositionColor(XMFLOAT4(position, 0.0f, -maxPosition, 1.0f), vertexColor);
		//
		//	// Horizontal line
		//	vertices[j + 2] = VertexPositionColor(XMFLOAT4(maxPosition, 0.0f, position, 1.0f), vertexColor);
		//	vertices[j + 3] = VertexPositionColor(XMFLOAT4(-maxPosition, 0.0f, position, 1.0f), vertexColor);
		//}

		int index = 0;
		int index1, index2, index3, index4;
		// Load the vertex and index array with the terrain data.
		for (int j = 0; j < (mHeight - 1); j++)
		{
			for (int i = 0; i < (mWidth - 1); i++)
			{
				index1 = (mHeight * j) + i;          // Bottom left.
				index2 = (mHeight * j) + (i + 1);      // Bottom right.
				index3 = (mHeight * (j + 1)) + i;      // Upper left.
				index4 = (mHeight * (j + 1)) + (i + 1);  // Upper right.

				// Upper left.
				vertices[index].Position = XMFLOAT4(mHeightMap[index3].x, mHeightMap[index3].y, mHeightMap[index3].z, 1.0f);
				vertices[index].Color = vertexColor;
				indices[index] = index;
				index++;

				// Upper right.
				vertices[index].Position = XMFLOAT4(mHeightMap[index4].x, mHeightMap[index4].y, mHeightMap[index4].z, 1.0f);
				vertices[index].Color = vertexColor;
				indices[index] = index;
				index++;

				// Upper right.
				vertices[index].Position = XMFLOAT4(mHeightMap[index4].x, mHeightMap[index4].y, mHeightMap[index4].z, 1.0f);
				vertices[index].Color = vertexColor;
				indices[index] = index;
				index++;

				// Bottom left.
				vertices[index].Position = XMFLOAT4(mHeightMap[index1].x, mHeightMap[index1].y, mHeightMap[index1].z, 1.0f);
				vertices[index].Color = vertexColor;
				indices[index] = index;
				index++;

				// Bottom left.
				vertices[index].Position = XMFLOAT4(mHeightMap[index1].x, mHeightMap[index1].y, mHeightMap[index1].z, 1.0f);
				vertices[index].Color = vertexColor;
				indices[index] = index;
				index++;

				// Upper left.
				vertices[index].Position = XMFLOAT4(mHeightMap[index3].x, mHeightMap[index3].y, mHeightMap[index3].z, 1.0f);
				vertices[index].Color = vertexColor;
				indices[index] = index;
				index++;

				// Bottom left.
				vertices[index].Position = XMFLOAT4(mHeightMap[index1].x, mHeightMap[index1].y, mHeightMap[index1].z, 1.0f);
				vertices[index].Color = vertexColor;
				indices[index] = index;
				index++;

				// Upper right.
				vertices[index].Position = XMFLOAT4(mHeightMap[index4].x, mHeightMap[index4].y, mHeightMap[index4].z, 1.0f);
				vertices[index].Color = vertexColor;
				indices[index] = index;
				index++;

				// Upper right.
				vertices[index].Position = XMFLOAT4(mHeightMap[index4].x, mHeightMap[index4].y, mHeightMap[index4].z, 1.0f);
				vertices[index].Color = vertexColor;
				indices[index] = index;
				index++;

				// Bottom right.
				vertices[index].Position = XMFLOAT4(mHeightMap[index2].x, mHeightMap[index2].y, mHeightMap[index2].z, 1.0f);
				vertices[index].Color = vertexColor;
				indices[index] = index;
				index++;

				// Bottom right.
				vertices[index].Position = XMFLOAT4(mHeightMap[index2].x, mHeightMap[index2].y, mHeightMap[index2].z, 1.0f);
				vertices[index].Color = vertexColor;
				indices[index] = index;
				index++;

				// Bottom left.
				vertices[index].Position = XMFLOAT4(mHeightMap[index1].x, mHeightMap[index1].y, mHeightMap[index1].z, 1.0f);
				vertices[index].Color = vertexColor;
				indices[index] = index;
				index++;
			}
		}

		D3D11_BUFFER_DESC vertexBufferDesc;
		ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
		vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		vertexBufferDesc.ByteWidth = size;
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA vertexSubResourceData;
		ZeroMemory(&vertexSubResourceData, sizeof(vertexSubResourceData));
		vertexSubResourceData.pSysMem = vertices;

		HRESULT hr;
		if (FAILED(hr = device->CreateBuffer(&vertexBufferDesc, &vertexSubResourceData, &mVertexBuffer)))
			throw GameException("ID3D11Device::CreateBuffer() failed while generating vertex buffer of terrain mesh");

		delete[] vertices;
		vertices = NULL;

		D3D11_BUFFER_DESC indexBufferDesc;
		indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		indexBufferDesc.ByteWidth = sizeof(unsigned long) * mIndexCount;
		indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		indexBufferDesc.CPUAccessFlags = 0;
		indexBufferDesc.MiscFlags = 0;
		indexBufferDesc.StructureByteStride = 0;

		// Give the subresource structure a pointer to the index data.
		D3D11_SUBRESOURCE_DATA indexSubResourceData;
		ZeroMemory(&indexSubResourceData, sizeof(indexSubResourceData));
		indexSubResourceData.pSysMem = indices;

		HRESULT hr2;
		if (FAILED(hr2 = device->CreateBuffer(&indexBufferDesc, &indexSubResourceData, &mIndexBuffer)))
			throw GameException("ID3D11Device::CreateBuffer() failed while generating index buffer of terrain mesh");

		delete[] indices;
		indices = NULL;
	}

	void Terrain::LoadTextures(std::string path)
	{
		FILE* filePtr;
		int error;
		unsigned int count;
		BITMAPFILEHEADER bitmapFileHeader;
		BITMAPINFOHEADER bitmapInfoHeader;
		int imageSize, i, j, k, index;
		unsigned char* bitmapImage;
		unsigned char height;

		// Open the height map file in binary.
		error = fopen_s(&filePtr, path.c_str(), "rb");
		if (error != 0)
			throw GameException("Can not open the terrain's heightmap bmp!");

		// Read in the file header.
		count = fread(&bitmapFileHeader, sizeof(BITMAPFILEHEADER), 1, filePtr);
		if (count != 1)
			throw GameException("Can not read the terrain's heightmap bmp header!");

		// Read in the bitmap info header.
		count = fread(&bitmapInfoHeader, sizeof(BITMAPINFOHEADER), 1, filePtr);
		if (count != 1)
			throw GameException("Can not read the terrain's heightmap bmp info header!");

		// Save the dimensions of the terrain.
		mWidth = bitmapInfoHeader.biWidth;
		mHeight = bitmapInfoHeader.biHeight;

		// Calculate the size of the bitmap image data.
		imageSize = mWidth * mHeight * 3;

		// Allocate memory for the bitmap image data.
		bitmapImage = new unsigned char[imageSize];
		if (!bitmapImage)
			throw GameException("Can not allocate data for the terrain's heightmap bmp!");

		// Move to the beginning of the bitmap data.
		fseek(filePtr, bitmapFileHeader.bfOffBits, SEEK_SET);

		// Read in the bitmap image data.
		count = fread(bitmapImage, 1, imageSize, filePtr);
		if (count != imageSize)
			throw GameException("Can not read the loaded data from the terrain's heightmap bmp!");

		// Close the file.
		error = fclose(filePtr);
		if (error != 0)
			throw GameException("Can not close the terrain's heightmap bmp!");

		// Create the structure to hold the height map data.
		mHeightMap = new HeightMap[mWidth * mHeight];

		k = 0;
		for (j = 0; j < mHeight; j++)
		{
			for (i = 0; i < mWidth; i++)
			{
				height = bitmapImage[k];

				index = (mHeight * j) + i;

				mHeightMap[index].x = (float)i;
				mHeightMap[index].y = (float)height;
				mHeightMap[index].z = (float)j;

				k += 3;
			}
		}

		// Release the bitmap image data.
		delete[] bitmapImage;
		bitmapImage = NULL;
	}

}