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

	Terrain::Terrain(std::string path, Game& pGame, Camera& camera, bool isWireframe) :
		GameComponent(pGame),
		mCamera(camera), 
		mIsWireframe(isWireframe),
		mHeightMaps(0, nullptr)
	{
		if (!(mNumTiles && !(mNumTiles & (mNumTiles - 1))))
			throw GameException("Number of tiles defined is not a power of 2!");

		Effect* effect = new Effect(pGame);
		effect->CompileFromFile(Utility::GetFilePath(L"content\\effects\\BasicEffect.fx"));

		mMaterial = new BasicMaterial();
		mMaterial->Initialize(effect);

		int numTilesSqrt = sqrt(mNumTiles);
		// Create the float array to hold the height map data.
		for (int i = 0; i < numTilesSqrt; i++)
		{
			for (int j = 0; j < numTilesSqrt; j++)
			{
				mHeightMaps.push_back(new HeightMap(mWidth, mHeight));

				int index = i * numTilesSqrt + j;
				std::string filePath = path;
				filePath += "_x" + std::to_string(i) + "_y" + std::to_string(j) + ".r16";
				
				LoadRawHeightmapTile(i, j, filePath);
				GenerateTileMesh(index);
			}
		}

	}

	Terrain::~Terrain()
	{
		DeleteObject(mMaterial);
		DeletePointerCollection(mHeightMaps);
	}

	void Terrain::Draw()
	{
		for (int i = 0 ; i < mHeightMaps.size(); i++)
			Draw(i);
	}

	void Terrain::Draw(int tileIndex)
	{
		ID3D11DeviceContext* context = GetGame()->Direct3DDeviceContext();
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		
		Pass* pass = mMaterial->CurrentTechnique()->Passes().at(0);
		ID3D11InputLayout* inputLayout = mMaterial->InputLayouts().at(pass);
		context->IASetInputLayout(inputLayout);

		UINT stride = sizeof(VertexPositionColor);
		UINT offset = 0;
		context->IASetVertexBuffers(0, 1, &(mHeightMaps[tileIndex]->mVertexBuffer), &stride, &offset);
		context->IASetIndexBuffer(mHeightMaps[tileIndex]->mIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

		XMMATRIX wvp = mHeightMaps[tileIndex]->mWorldMatrix * mCamera.ViewMatrix() * mCamera.ProjectionMatrix();
		mMaterial->WorldViewProjection() << wvp;

		pass->Apply(0, context);

		if (mIsWireframe)
		{
			context->RSSetState(RasterizerStates::Wireframe);
			context->DrawIndexed(mHeightMaps[tileIndex]->mIndexCount, 0, 0);
			context->RSSetState(nullptr);
		}
		else
			context->DrawIndexed(mHeightMaps[tileIndex]->mIndexCount, 0, 0);
	}

	void Terrain::GenerateTileMesh(int tileIndex)
	{
		assert(tileIndex < mHeightMaps.size());

		ID3D11Device* device = GetGame()->Direct3DDevice();

		mHeightMaps[tileIndex]->mVertexCount = (mWidth - 1) * (mHeight - 1) * 12;
		int size = sizeof(VertexPositionColor) * mHeightMaps[tileIndex]->mVertexCount;

		VertexPositionColor* vertices = new VertexPositionColor[mHeightMaps[tileIndex]->mVertexCount];

		unsigned long* indices;
		mHeightMaps[tileIndex]->mIndexCount = mHeightMaps[tileIndex]->mVertexCount;
		indices = new unsigned long[mHeightMaps[tileIndex]->mIndexCount];

		XMFLOAT4 vertexColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

		int index = 0;
		int index1, index2, index3, index4;
		// Load the vertex and index array with the terrain data.
		for (int j = 0; j < ((int)mHeight - 1); j++)
		{
			for (int i = 0; i < ((int)mWidth - 1); i++)
			{
				index1 = (mHeight * j) + i;          // Bottom left.
				index2 = (mHeight * j) + (i + 1);      // Bottom right.
				index3 = (mHeight * (j + 1)) + i;      // Upper left.
				index4 = (mHeight * (j + 1)) + (i + 1);  // Upper right.

				// Upper left.
				vertices[index].Position = XMFLOAT4(mHeightMaps[tileIndex]->mData[index3].x, mHeightMaps[tileIndex]->mData[index3].y, mHeightMaps[tileIndex]->mData[index3].z, 1.0f);
				vertices[index].Color = vertexColor;
				indices[index] = index;
				index++;

				// Upper right.
				vertices[index].Position = XMFLOAT4(mHeightMaps[tileIndex]->mData[index4].x, mHeightMaps[tileIndex]->mData[index4].y, mHeightMaps[tileIndex]->mData[index4].z, 1.0f);
				vertices[index].Color = vertexColor;
				indices[index] = index;
				index++;

				// Upper right.
				vertices[index].Position = XMFLOAT4(mHeightMaps[tileIndex]->mData[index4].x, mHeightMaps[tileIndex]->mData[index4].y, mHeightMaps[tileIndex]->mData[index4].z, 1.0f);
				vertices[index].Color = vertexColor;
				indices[index] = index;
				index++;

				// Bottom left.
				vertices[index].Position = XMFLOAT4(mHeightMaps[tileIndex]->mData[index1].x, mHeightMaps[tileIndex]->mData[index1].y, mHeightMaps[tileIndex]->mData[index1].z, 1.0f);
				vertices[index].Color = vertexColor;
				indices[index] = index;
				index++;

				// Bottom left.
				vertices[index].Position = XMFLOAT4(mHeightMaps[tileIndex]->mData[index1].x, mHeightMaps[tileIndex]->mData[index1].y, mHeightMaps[tileIndex]->mData[index1].z, 1.0f);
				vertices[index].Color = vertexColor;
				indices[index] = index;
				index++;

				// Upper left.
				vertices[index].Position = XMFLOAT4(mHeightMaps[tileIndex]->mData[index3].x, mHeightMaps[tileIndex]->mData[index3].y, mHeightMaps[tileIndex]->mData[index3].z, 1.0f);
				vertices[index].Color = vertexColor;
				indices[index] = index;
				index++;

				// Bottom left.
				vertices[index].Position = XMFLOAT4(mHeightMaps[tileIndex]->mData[index1].x, mHeightMaps[tileIndex]->mData[index1].y, mHeightMaps[tileIndex]->mData[index1].z, 1.0f);
				vertices[index].Color = vertexColor;
				indices[index] = index;
				index++;

				// Upper right.
				vertices[index].Position = XMFLOAT4(mHeightMaps[tileIndex]->mData[index4].x, mHeightMaps[tileIndex]->mData[index4].y, mHeightMaps[tileIndex]->mData[index4].z, 1.0f);
				vertices[index].Color = vertexColor;
				indices[index] = index;
				index++;

				// Upper right.
				vertices[index].Position = XMFLOAT4(mHeightMaps[tileIndex]->mData[index4].x, mHeightMaps[tileIndex]->mData[index4].y, mHeightMaps[tileIndex]->mData[index4].z, 1.0f);
				vertices[index].Color = vertexColor;
				indices[index] = index;
				index++;

				// Bottom right.
				vertices[index].Position = XMFLOAT4(mHeightMaps[tileIndex]->mData[index2].x, mHeightMaps[tileIndex]->mData[index2].y, mHeightMaps[tileIndex]->mData[index2].z, 1.0f);
				vertices[index].Color = vertexColor;
				indices[index] = index;
				index++;

				// Bottom right.
				vertices[index].Position = XMFLOAT4(mHeightMaps[tileIndex]->mData[index2].x, mHeightMaps[tileIndex]->mData[index2].y, mHeightMaps[tileIndex]->mData[index2].z, 1.0f);
				vertices[index].Color = vertexColor;
				indices[index] = index;
				index++;

				// Bottom left.
				vertices[index].Position = XMFLOAT4(mHeightMaps[tileIndex]->mData[index1].x, mHeightMaps[tileIndex]->mData[index1].y, mHeightMaps[tileIndex]->mData[index1].z, 1.0f);
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
		if (FAILED(hr = device->CreateBuffer(&vertexBufferDesc, &vertexSubResourceData, &(mHeightMaps[tileIndex]->mVertexBuffer))))
			throw GameException("ID3D11Device::CreateBuffer() failed while generating vertex buffer of terrain mesh");

		delete[] vertices;
		vertices = NULL;

		D3D11_BUFFER_DESC indexBufferDesc;
		indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		indexBufferDesc.ByteWidth = sizeof(unsigned long) * mHeightMaps[tileIndex]->mIndexCount;
		indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		indexBufferDesc.CPUAccessFlags = 0;
		indexBufferDesc.MiscFlags = 0;
		indexBufferDesc.StructureByteStride = 0;

		// Give the subresource structure a pointer to the index data.
		D3D11_SUBRESOURCE_DATA indexSubResourceData;
		ZeroMemory(&indexSubResourceData, sizeof(indexSubResourceData));
		indexSubResourceData.pSysMem = indices;

		HRESULT hr2;
		if (FAILED(hr2 = device->CreateBuffer(&indexBufferDesc, &indexSubResourceData, &(mHeightMaps[tileIndex]->mIndexBuffer))))
			throw GameException("ID3D11Device::CreateBuffer() failed while generating index buffer of terrain mesh");

		delete[] indices;
		indices = NULL;
	}

	void Terrain::LoadRawHeightmapTile(int tileIndexX, int tileIndexY, std::string path)
	{
		assert(tileIndex < mHeightMaps.size());

		int error, i, j, index;
		FILE* filePtr;
		unsigned long long imageSize, count;
		unsigned short* rawImage;

		// Open the 16 bit raw height map file for reading in binary.
		error = fopen_s(&filePtr, path.c_str(), "rb");
		if (error != 0)
			throw GameException("Can not open the terrain's heightmap RAW!");

		// Calculate the size of the raw image data.
		imageSize = mWidth * mHeight;

		// Allocate memory for the raw image data.
		rawImage = new unsigned short[imageSize];

		// Read in the raw image data.
		count = fread(rawImage, sizeof(unsigned short), imageSize, filePtr);
		if (count != imageSize)
			throw GameException("Can not read the terrain's heightmap RAW file!");

		// Close the file.
		error = fclose(filePtr);
		if (error != 0)
			throw GameException("Can not close the terrain's heightmap RAW file!");

		// Copy the image data into the height map array.
		for (j = 0; j < (int)mHeight; j++)
		{
			for (i = 0; i < (int)mWidth; i++)
			{
				index = (mWidth * j) + i;

				// Store the height at this point in the height map array.
				mHeightMaps[tileIndexX *  sqrt(mNumTiles) + tileIndexY]->mData[index].x = (float)(i + (int)mWidth * (tileIndexX - 1));
				mHeightMaps[tileIndexX *  sqrt(mNumTiles) + tileIndexY]->mData[index].y = (float)rawImage[index] / mHeightScale;
				mHeightMaps[tileIndexX *  sqrt(mNumTiles) + tileIndexY]->mData[index].z = (float)(j - (int)mHeight * tileIndexY);

			}
		}

		// Release image data.
		delete[] rawImage;
		rawImage = 0;
	}

	HeightMap::HeightMap(int width, int height)
	{
		mData = new MapData[width * height];
	}

	HeightMap::~HeightMap()
	{		
		ReleaseObject(mVertexBuffer);
		ReleaseObject(mIndexBuffer);
		DeleteObjects(mData);
	}

}