#include "stdafx.h"
#include <stdio.h>

#include "ER_Terrain.h"
#include "GameException.h"
#include "Model.h"
#include "Mesh.h"
#include "Game.h"
#include "MatrixHelper.h"
#include "Utility.h"
#include "VertexDeclarations.h"
#include "RasterizerStates.h"
#include "ER_ShadowMapper.h"
#include "ER_Scene.h"
#include "ShaderCompiler.h"
#include "DirectionalLight.h"
#include "ER_LightProbesManager.h"
#include "ER_LightProbe.h"
#include "ER_RenderableAABB.h"
#include "Camera.h"

#define USE_RAYCASTING_FOR_ON_TERRAIN_PLACEMENT 0
#define MAX_TERRAIN_TILE_COUNT 256

namespace Library
{
	ER_Terrain::ER_Terrain(Game& pGame, DirectionalLight& light) :
		GameComponent(pGame),
		mIsWireframe(false),
		mHeightMaps(0, nullptr),
		mDirectionalLight(light)
	{
		//shaders
		{
			ID3DBlob* blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Terrain\\Terrain.hlsl").c_str(), "VSMain", "vs_5_0", &blob)))
				throw GameException("Failed to load VSMain from shader: Terrain.hlsl!");
			if (FAILED(GetGame()->Direct3DDevice()->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mVS)))
				throw GameException("Failed to create vertex shader from Terrain.hlsl!");

			D3D11_INPUT_ELEMENT_DESC inputElementDescriptions[] =
			{
				{ "PATCH_INFO", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 }
			};

			HRESULT hr = GetGame()->Direct3DDevice()->CreateInputLayout(inputElementDescriptions, ARRAYSIZE(inputElementDescriptions), blob->GetBufferPointer(), blob->GetBufferSize(), &mInputLayout);
			if (FAILED(hr))
				throw GameException("CreateInputLayout() failed when creating terrain's vertex shader.", hr);
			blob->Release();

			blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Terrain\\Terrain.hlsl").c_str(), "HSMain", "hs_5_0", &blob)))
				throw GameException("Failed to load HSMain from shader: Terrain.hlsl!");
			if (FAILED(GetGame()->Direct3DDevice()->CreateHullShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mHS)))
				throw GameException("Failed to create hull shader from Terrain.hlsl!");
			blob->Release();

			blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Terrain\\Terrain.hlsl").c_str(), "DSMain", "ds_5_0", &blob)))
				throw GameException("Failed to load DSMain from shader: Terrain.hlsl!");
			if (FAILED(GetGame()->Direct3DDevice()->CreateDomainShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mDS)))
				throw GameException("Failed to create domain shader from Terrain.hlsl!");
			blob->Release();

			blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Terrain\\Terrain.hlsl").c_str(), "DSShadowMap", "ds_5_0", &blob)))
				throw GameException("Failed to load DSShadowMap from shader: Terrain.hlsl!");
			if (FAILED(GetGame()->Direct3DDevice()->CreateDomainShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mDS_ShadowMap)))
				throw GameException("Failed to create domain shader from Terrain.hlsl!");
			blob->Release();

			blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Terrain\\Terrain.hlsl").c_str(), "PSMain", "ps_5_0", &blob)))
				throw GameException("Failed to load PSMain from shader: Terrain.hlsl!");
			if (FAILED(GetGame()->Direct3DDevice()->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mPS)))
				throw GameException("Failed to create pixel shader from Terrain.hlsl!");
			blob->Release();

			blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Terrain\\Terrain.hlsl").c_str(), "PSShadowMap", "ps_5_0", &blob)))
				throw GameException("Failed to load PSShadowMap from shader: Terrain.hlsl!");
			if (FAILED(GetGame()->Direct3DDevice()->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mPS_ShadowMap)))
				throw GameException("Failed to create pixel shader from Terrain.hlsl!");
			blob->Release();

			blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Terrain\\PlaceObjectsOnTerrain.hlsl").c_str(), "CSMain", "cs_5_0", &blob)))
				throw GameException("Failed to load a shader: CSMain from PlaceObjectsOnTerrain.hlsl!");
			if (FAILED(GetGame()->Direct3DDevice()->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mPlaceOnTerrainCS)))
				throw GameException("Failed to create compute shader from PlaceObjectsOnTerrain.hlsl!");
			blob->Release();
		}

		mTerrainConstantBuffer.Initialize(GetGame()->Direct3DDevice());
		mPlaceOnTerrainConstantBuffer.Initialize(GetGame()->Direct3DDevice());
	}

	ER_Terrain::~ER_Terrain()
	{
		DeletePointerCollection(mHeightMaps);
		for (int i = 0; i < NUM_TEXTURE_SPLAT_CHANNELS; i++)
			DeleteObject(mSplatChannelTextures[i]);

		ReleaseObject(mVS);
		ReleaseObject(mHS);
		ReleaseObject(mDS);
		ReleaseObject(mDS_ShadowMap);
		ReleaseObject(mPS);
		ReleaseObject(mPS_ShadowMap);
		ReleaseObject(mPlaceOnTerrainCS);
		ReleaseObject(mInputLayout);
		DeleteObject(mTerrainTilesDataGPU);
		DeleteObject(mTerrainTilesHeightmapsArrayTexture);
		DeleteObject(mTerrainTilesSplatmapsArrayTexture);
		mTerrainConstantBuffer.Release();
		mPlaceOnTerrainConstantBuffer.Release();
	}

	void ER_Terrain::LoadTerrainData(ER_Scene* aScene)
	{
		if (!aScene->HasTerrain())
		{
			mEnabled = false;
			mLoaded = false;
			return;
		}
		else
			mLoaded = true;
		assert(!mLevelPath.empty());

		mTileResolution = aScene->GetTerrainTileResolution();
		if (mTileResolution == 0)
			throw GameException("Tile resolution (= heightmap texture tile resolution) of the terrain is 0!");

		mTileScale = aScene->GetTerrainTileScale();
		mWidth = mHeight = mTileResolution;

		mNumTiles = aScene->GetTerrainTilesCount();
		if (!(mNumTiles && !(mNumTiles & (mNumTiles - 1))))
			throw GameException("Number of tiles defined is not a power of 2!");

		if (mNumTiles > MAX_TERRAIN_TILE_COUNT)
			throw GameException("Number of tiles exceeds MAX_TERRAIN_TILE_COUNT!");

		for (int i = 0; i < mNumTiles; i++)
			mHeightMaps.push_back(new HeightMap(mWidth, mHeight));

		std::wstring path = mLevelPath + L"terrain\\";
		LoadTextures(path, 
			path + aScene->GetTerrainSplatLayerTextureName(0),
			path + aScene->GetTerrainSplatLayerTextureName(1),
			path + aScene->GetTerrainSplatLayerTextureName(2),
			path + aScene->GetTerrainSplatLayerTextureName(3)
		); //not thread-safe

		for (int i = 0; i < mNumTiles; i++)
			LoadTile(i, path); //not thread-safe

		int tileSize = mTileScale * mTileResolution;
		TerrainTileDataGPU* terrainTilesDataCPUBuffer = new TerrainTileDataGPU[mNumTiles];
		for (int tileIndex = 0; tileIndex < mNumTiles; tileIndex++)
		{
			terrainTilesDataCPUBuffer[tileIndex].UVoffsetTileSize = XMFLOAT4(mHeightMaps[tileIndex]->mTileUVOffset.x, mHeightMaps[tileIndex]->mTileUVOffset.y, tileSize, tileSize);
			terrainTilesDataCPUBuffer[tileIndex].AABBMinPoint = XMFLOAT4(mHeightMaps[tileIndex]->mAABB.first.x, mHeightMaps[tileIndex]->mAABB.first.y, mHeightMaps[tileIndex]->mAABB.first.z, 1.0);
			terrainTilesDataCPUBuffer[tileIndex].AABBMaxPoint = XMFLOAT4(mHeightMaps[tileIndex]->mAABB.second.x, mHeightMaps[tileIndex]->mAABB.second.y, mHeightMaps[tileIndex]->mAABB.second.z, 1.0);
		}
		mTerrainTilesDataGPU = new ER_GPUBuffer(GetGame()->Direct3DDevice(), terrainTilesDataCPUBuffer, mNumTiles, sizeof(TerrainTileDataGPU),
			D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED);
		DeleteObjects(terrainTilesDataCPUBuffer);

		auto context = GetGame()->Direct3DDeviceContext();
		mTerrainTilesHeightmapsArrayTexture = new ER_GPUTexture(GetGame()->Direct3DDevice(), mTileResolution, mTileResolution, 1, DXGI_FORMAT_R16_UNORM,
			D3D11_BIND_SHADER_RESOURCE, 1, -1, mNumTiles);
		mTerrainTilesSplatmapsArrayTexture = new ER_GPUTexture(GetGame()->Direct3DDevice(), mTileResolution, mTileResolution, 1, DXGI_FORMAT_R16G16B16A16_UNORM,
			D3D11_BIND_SHADER_RESOURCE, 1, -1, mNumTiles);
		for (int tileIndex = 0; tileIndex < mNumTiles; tileIndex++)
		{
			context->CopySubresourceRegion(mTerrainTilesHeightmapsArrayTexture->GetTexture2D(),
				D3D11CalcSubresource(0, tileIndex, 1), 0, 0, 0,
				mHeightMaps[tileIndex]->mHeightTexture->GetTexture2D(), 
				D3D11CalcSubresource(0, 0, 1), NULL);
			context->CopySubresourceRegion(mTerrainTilesSplatmapsArrayTexture->GetTexture2D(),
				D3D11CalcSubresource(0, tileIndex, 1), 0, 0, 0,
				mHeightMaps[tileIndex]->mSplatTexture->GetTexture2D(),
				D3D11CalcSubresource(0, 0, 1), NULL);
		}
	}

	void ER_Terrain::LoadTextures(const std::wstring& aTexturesPath, const std::wstring& splatLayer0Path, const std::wstring& splatLayer1Path, const std::wstring& splatLayer2Path, const std::wstring& splatLayer3Path)
	{
		if (!splatLayer0Path.empty())
			mSplatChannelTextures[0] = new ER_GPUTexture(GetGame()->Direct3DDevice(), GetGame()->Direct3DDeviceContext(), splatLayer0Path, true);
		if (!splatLayer1Path.empty())
			mSplatChannelTextures[1] = new ER_GPUTexture(GetGame()->Direct3DDevice(), GetGame()->Direct3DDeviceContext(), splatLayer1Path, true);
		if (!splatLayer2Path.empty())
			mSplatChannelTextures[2] = new ER_GPUTexture(GetGame()->Direct3DDevice(), GetGame()->Direct3DDeviceContext(), splatLayer2Path, true);
		if (!splatLayer3Path.empty())
			mSplatChannelTextures[3] = new ER_GPUTexture(GetGame()->Direct3DDevice(), GetGame()->Direct3DDeviceContext(), splatLayer3Path, true);

		int numTilesSqrt = sqrt(mNumTiles);

		for (int i = 0; i < numTilesSqrt; i++)
		{
			for (int j = 0; j < numTilesSqrt; j++)
			{
				int index = i * numTilesSqrt + j;
				
				std::wstring filePathSplatmap = aTexturesPath;
				filePathSplatmap += L"terrainSplat_x" + std::to_wstring(i) + L"_y" + std::to_wstring(j) + L".png";
				LoadSplatmapPerTileGPU(i, j, filePathSplatmap); //unfortunately, not thread safe

				std::wstring filePathHeightmap = aTexturesPath;
				filePathHeightmap += L"terrainHeight_x" + std::to_wstring(i) + L"_y" + std::to_wstring(j) + L".png";
				LoadHeightmapPerTileGPU(i, j, filePathHeightmap); //unfortunately, not thread safe
			}
		}
	}
	
	void ER_Terrain::LoadTile(int threadIndex, const std::wstring& aTexturesPath)
	{
		int numTilesSqrt = sqrt(mNumTiles);

		int tileX = threadIndex / numTilesSqrt;
		int tileY = threadIndex - numTilesSqrt * tileX;

		int index = tileX * numTilesSqrt + tileY;
		std::wstring filePathHeightmap = aTexturesPath;
		filePathHeightmap += L"terrainHeight_x" + std::to_wstring(tileX) + L"_y" + std::to_wstring(tileY) + L".r16";

		CreateTerrainTileDataCPU(tileX, tileY, filePathHeightmap);
		CreateTerrainTileDataGPU(tileX, tileY);
	}

	void ER_Terrain::LoadSplatmapPerTileGPU(int tileIndexX, int tileIndexY, const std::wstring& path)
	{
		int tileIndex = tileIndexX * sqrt(mNumTiles) + tileIndexY;
		if (tileIndex >= mHeightMaps.size())
			return;

		mHeightMaps[tileIndex]->mSplatTexture = new ER_GPUTexture(GetGame()->Direct3DDevice(), GetGame()->Direct3DDeviceContext(), path, true);
	}

	void ER_Terrain::LoadHeightmapPerTileGPU(int tileIndexX, int tileIndexY, const std::wstring& path)
	{
		int tileIndex = tileIndexX * sqrt(mNumTiles) + tileIndexY;
		if (tileIndex >= mHeightMaps.size())
			return;

		mHeightMaps[tileIndex]->mHeightTexture = new ER_GPUTexture(GetGame()->Direct3DDevice(), GetGame()->Direct3DDeviceContext(), path, true);
	}

	// Create pre-tessellated patch data which is used for terrain rendering (using GPU tessellation pipeline)
	void ER_Terrain::CreateTerrainTileDataGPU(int tileIndexX, int tileIndexY)
	{
		int tileIndex = tileIndexX * sqrt(mNumTiles) + tileIndexY;
		assert(tileIndex < mHeightMaps.size());

		int terrainTileSize = mTileResolution * mTileScale;

		// creating terrain vertex buffer for patches
		float* patches_rawdata = new float[NUM_TERRAIN_PATCHES_PER_TILE * NUM_TERRAIN_PATCHES_PER_TILE * 4];
		for (int i = 0; i < NUM_TERRAIN_PATCHES_PER_TILE; i++)
			for (int j = 0; j < NUM_TERRAIN_PATCHES_PER_TILE; j++)
			{
				patches_rawdata[(i + j * NUM_TERRAIN_PATCHES_PER_TILE) * 4 + 0] = i * terrainTileSize / NUM_TERRAIN_PATCHES_PER_TILE;
				patches_rawdata[(i + j * NUM_TERRAIN_PATCHES_PER_TILE) * 4 + 1] = j * terrainTileSize / NUM_TERRAIN_PATCHES_PER_TILE;
				patches_rawdata[(i + j * NUM_TERRAIN_PATCHES_PER_TILE) * 4 + 2] = terrainTileSize / NUM_TERRAIN_PATCHES_PER_TILE;
				patches_rawdata[(i + j * NUM_TERRAIN_PATCHES_PER_TILE) * 4 + 3] = terrainTileSize / NUM_TERRAIN_PATCHES_PER_TILE;
			}

		D3D11_BUFFER_DESC buf_desc;
		memset(&buf_desc, 0, sizeof(buf_desc));

		buf_desc.ByteWidth = NUM_TERRAIN_PATCHES_PER_TILE * NUM_TERRAIN_PATCHES_PER_TILE * 4 * sizeof(float);
		buf_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		buf_desc.Usage = D3D11_USAGE_DEFAULT;

		D3D11_SUBRESOURCE_DATA subresource_data;
		subresource_data.pSysMem = patches_rawdata;
		subresource_data.SysMemPitch = 0;
		subresource_data.SysMemSlicePitch = 0;

		if (FAILED(GetGame()->Direct3DDevice()->CreateBuffer(&buf_desc, &subresource_data, &(mHeightMaps[tileIndex]->mVertexBufferTS))))
			throw GameException("ID3D11Device::CreateBuffer() failed while generating vertex buffer of GPU terrain mesh patch (pre-tessellation)");

		free(patches_rawdata);

		mHeightMaps[tileIndex]->mWorldMatrixTS = XMMatrixTranslation(terrainTileSize * (tileIndexX - 1), 0.0f, terrainTileSize * -tileIndexY);
		mHeightMaps[tileIndex]->mTileUVOffset = XMFLOAT2(terrainTileSize - tileIndexX * terrainTileSize, tileIndexY * terrainTileSize);
	}

	// Create CPU tile data which is used for terrain debugging, collisions, placement of ER_RenderingObject(s) (no GPU tessellation pipeline)
	void ER_Terrain::CreateTerrainTileDataCPU(int tileIndexX, int tileIndexY, const std::wstring& aPath)
	{
		int tileIndex = tileIndexX * sqrt(mNumTiles) + tileIndexY;
		assert(tileIndex < mHeightMaps.size());
		auto device = GetGame()->Direct3DDevice();

		int error, i, j, index;
		FILE* filePtr;
		unsigned long long imageSize, count;
		unsigned short* rawImage;

		// Load raw heightmap file
		{
			// Open the 16 bit raw height map file for reading in binary.
			error = _wfopen_s(&filePtr, aPath.c_str(), L"rb");
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
		}

		// Copy the image data into the height map array.
		{
			int tileSize = mTileResolution * mTileScale;
			for (j = 0; j < static_cast<int>(mHeight); j++)
			{
				for (i = 0; i < static_cast<int>(mWidth); i++)
				{
					index = (mWidth * j) + i;

					// Store the height at this point in the height map array.
					mHeightMaps[tileIndex]->mData[index].x = static_cast<float>(i * mTileScale + tileSize * (tileIndexX - 1));
					mHeightMaps[tileIndex]->mData[index].y = static_cast<float>(rawImage[index]) / 200.0f;//TODO mTerrainNonTessellatedHeightScale;
					mHeightMaps[tileIndex]->mData[index].z = static_cast<float>(j * mTileScale - tileSize * tileIndexY);

					if (tileIndex > 0) //a way to fix the seams between tiles...
					{
						mHeightMaps[tileIndex]->mData[index].x -= static_cast<float>(tileIndexX) /** scale*/;
						mHeightMaps[tileIndex]->mData[index].z += static_cast<float>(tileIndexY) /** scale*/;
					}

				}
			}

			// Release image data.
			delete[] rawImage;
			rawImage = 0;
		}

		// Generate CPU mesh (and its GPU vertex/index buffers) + calculate AABB of the tile
		{
			mHeightMaps[tileIndex]->mVertexCountNonTS = (mWidth - 1) * (mHeight - 1) * 6;
			DebugTerrainVertexInput* vertices = new DebugTerrainVertexInput[mHeightMaps[tileIndex]->mVertexCountNonTS];

			mHeightMaps[tileIndex]->mIndexCountNonTS = mHeightMaps[tileIndex]->mVertexCountNonTS;
			unsigned long* indices = new unsigned long[mHeightMaps[tileIndex]->mIndexCountNonTS];

			int index = 0;
			int index1, index2, index3, index4;
			// Load the vertex and index array with the terrain data.
			for (int j = 0; j < ((int)mHeight - 1); j++)
			{
				for (int i = 0; i < ((int)mWidth - 1); i++)
				{
					index1 = (mHeight * j) + i;					// Bottom left.	
					index2 = (mHeight * j) + (i + 1);			// Bottom right.
					index3 = (mHeight * (j + 1)) + i;			// Upper left.	
					index4 = (mHeight * (j + 1)) + (i + 1);		// Upper right.	

					// Upper left.
					vertices[index].Position = XMFLOAT4(mHeightMaps[tileIndex]->mData[index3].x, mHeightMaps[tileIndex]->mData[index3].y, mHeightMaps[tileIndex]->mData[index3].z, 1.0f);
					indices[index] = index;
					index++;

					// Upper right.
					vertices[index].Position = XMFLOAT4(mHeightMaps[tileIndex]->mData[index4].x, mHeightMaps[tileIndex]->mData[index4].y, mHeightMaps[tileIndex]->mData[index4].z, 1.0f);
					indices[index] = index;
					index++;

					// Bottom left.
					vertices[index].Position = XMFLOAT4(mHeightMaps[tileIndex]->mData[index1].x, mHeightMaps[tileIndex]->mData[index1].y, mHeightMaps[tileIndex]->mData[index1].z, 1.0f);
					indices[index] = index;
					index++;

					// Bottom left.
					vertices[index].Position = XMFLOAT4(mHeightMaps[tileIndex]->mData[index1].x, mHeightMaps[tileIndex]->mData[index1].y, mHeightMaps[tileIndex]->mData[index1].z, 1.0f);
					indices[index] = index;
					index++;

					// Upper right.
					vertices[index].Position = XMFLOAT4(mHeightMaps[tileIndex]->mData[index4].x, mHeightMaps[tileIndex]->mData[index4].y, mHeightMaps[tileIndex]->mData[index4].z, 1.0f);
					indices[index] = index;
					index++;

					// Bottom right.
					vertices[index].Position = XMFLOAT4(mHeightMaps[tileIndex]->mData[index2].x, mHeightMaps[tileIndex]->mData[index2].y, mHeightMaps[tileIndex]->mData[index2].z, 1.0f);
					indices[index] = index;
					index++;
				}
			}

			D3D11_BUFFER_DESC vertexBufferDesc;
			ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
			vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
			vertexBufferDesc.ByteWidth = sizeof(DebugTerrainVertexInput) * mHeightMaps[tileIndex]->mVertexCountNonTS;
			vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

			D3D11_SUBRESOURCE_DATA vertexSubResourceData;
			ZeroMemory(&vertexSubResourceData, sizeof(vertexSubResourceData));
			vertexSubResourceData.pSysMem = vertices;

			HRESULT hr;
			if (FAILED(hr = device->CreateBuffer(&vertexBufferDesc, &vertexSubResourceData, &(mHeightMaps[tileIndex]->mVertexBufferNonTS))))
				throw GameException("ID3D11Device::CreateBuffer() failed while generating vertex buffer of CPU terrain mesh tile");

			XMFLOAT3 minVertex = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
			XMFLOAT3 maxVertex = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

			// copy vertices to tile data + calculate AABB
			for (int i = 0; i < mHeightMaps[tileIndex]->mVertexCountNonTS; i++)
			{
				mHeightMaps[tileIndex]->mVertexList[i].x = vertices[i].Position.x;
				mHeightMaps[tileIndex]->mVertexList[i].y = vertices[i].Position.y;
				mHeightMaps[tileIndex]->mVertexList[i].z = vertices[i].Position.z;

				//Get the smallest vertex 
				minVertex.x = std::min(minVertex.x, vertices[i].Position.x);    // Find smallest x value in model
				minVertex.y = std::min(minVertex.y, vertices[i].Position.y);    // Find smallest y value in model
				minVertex.z = std::min(minVertex.z, vertices[i].Position.z);    // Find smallest z value in model

				//Get the largest vertex 
				maxVertex.x = std::max(maxVertex.x, vertices[i].Position.x);    // Find largest x value in model
				maxVertex.y = std::max(maxVertex.y, vertices[i].Position.y);    // Find largest y value in model
				maxVertex.z = std::max(maxVertex.z, vertices[i].Position.z);    // Find largest z value in model
			}
			mHeightMaps[tileIndex]->mAABB = { minVertex, maxVertex };

			delete[] vertices;
			vertices = NULL;

			D3D11_BUFFER_DESC indexBufferDesc;
			indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
			indexBufferDesc.ByteWidth = sizeof(unsigned long) * mHeightMaps[tileIndex]->mIndexCountNonTS;
			indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			indexBufferDesc.CPUAccessFlags = 0;
			indexBufferDesc.MiscFlags = 0;
			indexBufferDesc.StructureByteStride = 0;

			// Give the subresource structure a pointer to the index data.
			D3D11_SUBRESOURCE_DATA indexSubResourceData;
			ZeroMemory(&indexSubResourceData, sizeof(indexSubResourceData));
			indexSubResourceData.pSysMem = indices;

			HRESULT hr2;
			if (FAILED(hr2 = device->CreateBuffer(&indexBufferDesc, &indexSubResourceData, &(mHeightMaps[tileIndex]->mIndexBufferNonTS))))
				throw GameException("ID3D11Device::CreateBuffer() failed while generating index buffer of CPU terrain mesh tile");

			delete[] indices;
			indices = NULL;

			mHeightMaps[tileIndex]->mDebugGizmoAABB = new ER_RenderableAABB(*GetGame(), XMFLOAT4(0.0, 0.0, 1.0, 1.0));
			mHeightMaps[tileIndex]->mDebugGizmoAABB->InitializeGeometry({ mHeightMaps[tileIndex]->mAABB.first,mHeightMaps[tileIndex]->mAABB.second });
		}
	}

	void ER_Terrain::Draw(ER_ShadowMapper* worldShadowMapper, ER_LightProbesManager* probeManager, int shadowMapCascade)
	{
		if (!mEnabled || !mLoaded)
			return;

		for (int i = 0; i < mHeightMaps.size(); i++)
			DrawTessellated(i, worldShadowMapper, probeManager, shadowMapCascade);
	}

	void ER_Terrain::DrawDebugGizmos()
	{
		if (!mEnabled || !mLoaded || !mDrawDebugAABBs)
			return;

		for (int i = 0; i < mHeightMaps.size(); i++)
			mHeightMaps[i]->mDebugGizmoAABB->Draw();
	}

	void ER_Terrain::Update(const GameTime& gameTime)
	{
		Camera* camera = (Camera*)(mGame->Services().GetService(Camera::TypeIdClass()));

		int visibleTiles = 0;
		for (int i = 0; i < mHeightMaps.size(); i++)
		{
			if (!mHeightMaps[i]->PerformCPUFrustumCulling(mDoCPUFrustumCulling ? camera : nullptr))
				visibleTiles++;
		}

		if (mShowDebug) {
			ImGui::Begin("Terrain System");
			
			std::string cullText = "Visible tiles: " + std::to_string(visibleTiles) + "/" + std::to_string(mHeightMaps.size());
			ImGui::Text(cullText.c_str());
			ImGui::Checkbox("Enabled", &mEnabled);
			ImGui::Checkbox("CPU frustum culling", &mDoCPUFrustumCulling);
			ImGui::Checkbox("Debug tiles AABBs", &mDrawDebugAABBs);
			ImGui::Checkbox("Render wireframe", &mIsWireframe);
			ImGui::SliderInt("Tessellation factor static", &mTessellationFactor, 1, 64);
			ImGui::SliderInt("Tessellation factor dynamic", &mTessellationFactorDynamic, 1, 64);
			ImGui::Checkbox("Use dynamic tessellation", &mUseDynamicTessellation);
			ImGui::SliderFloat("Dynamic LOD distance factor", &mTessellationDistanceFactor, 0.0001f, 0.1f);
			ImGui::SliderFloat("Tessellated terrain height scale", &mTerrainTessellatedHeightScale, 0.0f, 1000.0f);
			ImGui::SliderFloat("Placement height delta", &mPlacementHeightDelta, 0.0f, 10.0f);
			ImGui::End();
		}
	}

	void ER_Terrain::DrawTessellated(int tileIndex, ER_ShadowMapper* worldShadowMapper, ER_LightProbesManager* probeManager, int shadowMapCascade)
	{
		if (mHeightMaps[tileIndex]->IsCulled() && shadowMapCascade == -1) //for shadow mapping pass we dont want to cull with main camera frustum
			return;

		Camera* camera = (Camera*)(mGame->Services().GetService(Camera::TypeIdClass()));
		assert(camera);

		ID3D11DeviceContext* context = GetGame()->Direct3DDeviceContext();
		D3D11_PRIMITIVE_TOPOLOGY originalPrimitiveTopology;
		context->IAGetPrimitiveTopology(&originalPrimitiveTopology);
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST);
		context->IASetInputLayout(mInputLayout);

		UINT stride = sizeof(float) * 4;
		UINT offset = 0;
		context->IASetVertexBuffers(0, 1, &(mHeightMaps[tileIndex]->mVertexBufferTS), &stride, &offset);

		if (worldShadowMapper)
		{
			for (int cascade = 0; cascade < NUM_SHADOW_CASCADES; cascade++)
				mTerrainConstantBuffer.Data.ShadowMatrices[cascade] = XMMatrixTranspose(worldShadowMapper->GetViewMatrix(cascade) * worldShadowMapper->GetProjectionMatrix(cascade) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix()));
			mTerrainConstantBuffer.Data.ShadowTexelSize = XMFLOAT4{ 1.0f / worldShadowMapper->GetResolution(), 1.0f, 1.0f , 1.0f };
			mTerrainConstantBuffer.Data.ShadowCascadeDistances = XMFLOAT4{ camera->GetCameraFarShadowCascadeDistance(0), camera->GetCameraFarShadowCascadeDistance(1), camera->GetCameraFarShadowCascadeDistance(2), 1.0f };
			
			if (shadowMapCascade != -1)
			{
				XMMATRIX lvp = worldShadowMapper->GetViewMatrix(shadowMapCascade) * worldShadowMapper->GetProjectionMatrix(shadowMapCascade);
				mTerrainConstantBuffer.Data.WorldLightViewProjection = XMMatrixTranspose(mHeightMaps[tileIndex]->mWorldMatrixTS * lvp);
			}
		}

		mTerrainConstantBuffer.Data.World = XMMatrixTranspose(mHeightMaps[tileIndex]->mWorldMatrixTS);
		mTerrainConstantBuffer.Data.View = XMMatrixTranspose(camera->ViewMatrix());
		mTerrainConstantBuffer.Data.Projection = XMMatrixTranspose(camera->ProjectionMatrix());
		mTerrainConstantBuffer.Data.SunDirection = XMFLOAT4(-mDirectionalLight.Direction().x, -mDirectionalLight.Direction().y, -mDirectionalLight.Direction().z, 1.0f);
		mTerrainConstantBuffer.Data.SunColor = XMFLOAT4{ mDirectionalLight.GetDirectionalLightColor().x, mDirectionalLight.GetDirectionalLightColor().y, mDirectionalLight.GetDirectionalLightColor().z, mDirectionalLight.GetDirectionalLightIntensity() };
		mTerrainConstantBuffer.Data.CameraPosition = XMFLOAT4(camera->Position().x, camera->Position().y, camera->Position().z, 1.0f);
		mTerrainConstantBuffer.Data.TessellationFactor = static_cast<float>(mTessellationFactor);
		mTerrainConstantBuffer.Data.TerrainHeightScale = mTerrainTessellatedHeightScale;
		mTerrainConstantBuffer.Data.TessellationFactorDynamic = static_cast<float>(mTessellationFactorDynamic);
		mTerrainConstantBuffer.Data.UseDynamicTessellation = mUseDynamicTessellation ? 1.0f : 0.0f;
		mTerrainConstantBuffer.Data.DistanceFactor = mTessellationDistanceFactor;
		mTerrainConstantBuffer.Data.TileSize = mTileResolution * mTileScale;
		mTerrainConstantBuffer.ApplyChanges(context);
		ID3D11Buffer* CBs[1] = { mTerrainConstantBuffer.Buffer() };

		context->VSSetConstantBuffers(0, 1, CBs);
		context->DSSetConstantBuffers(0, 1, CBs);
		context->HSSetConstantBuffers(0, 1, CBs);
		context->PSSetConstantBuffers(0, 1, CBs);

		const int probesShift = 10; //WARNING: verify with Illumination system
		ID3D11ShaderResourceView* SRVs[1 /*splat*/ + 4 /*splat channels*/ + NUM_SHADOW_CASCADES + probesShift /*probe GI*/ + 1 /*height*/] =
		{
			mHeightMaps[tileIndex]->mSplatTexture->GetSRV(),
			mSplatChannelTextures[0]->GetSRV(),
			mSplatChannelTextures[1]->GetSRV(),
			mSplatChannelTextures[2]->GetSRV(),
			mSplatChannelTextures[3]->GetSRV()
		};

		if (worldShadowMapper)
			for (int c = 0; c < NUM_SHADOW_CASCADES; c++)
				SRVs[1 + 4 + c] = worldShadowMapper->GetShadowTexture(c);

		if (probeManager)
		{
			SRVs[8] = (probeManager->IsEnabled() || probeManager->AreGlobalProbesReady()) ? probeManager->GetGlobalDiffuseProbe()->GetCubemapSRV() : nullptr;
			SRVs[9] = nullptr;
			SRVs[10] = nullptr;
			SRVs[11] = nullptr;
			SRVs[12] = (probeManager->IsEnabled() || probeManager->AreGlobalProbesReady()) ? probeManager->GetGlobalSpecularProbe()->GetCubemapSRV() : nullptr;
			SRVs[13] = nullptr;
			SRVs[14] = nullptr;
			SRVs[15] = nullptr;
			SRVs[16] = nullptr;
			SRVs[17] = (probeManager->IsEnabled() || probeManager->AreGlobalProbesReady()) ? probeManager->GetIntegrationMap() : nullptr;
		}

		SRVs[18] = mHeightMaps[tileIndex]->mHeightTexture->GetSRV();

		context->DSSetShaderResources(0, 1 + 4 + NUM_SHADOW_CASCADES + probesShift + 1, SRVs);
		context->PSSetShaderResources(0, 1 + 4 + NUM_SHADOW_CASCADES + probesShift + 1, SRVs);

		ID3D11SamplerState* SS[3] = { SamplerStates::TrilinearWrap, SamplerStates::TrilinearClamp, SamplerStates::ShadowSamplerState };
		context->DSSetSamplers(0, 3, SS);
		context->PSSetSamplers(0, 3, SS);

		context->VSSetShader(mVS, NULL, NULL);
		context->HSSetShader(mHS, NULL, NULL);
		context->DSSetShader((shadowMapCascade != -1) ? mDS_ShadowMap : mDS, NULL, NULL);
		context->PSSetShader((shadowMapCascade != -1) ? mPS_ShadowMap : mPS, NULL, NULL);

		if (mIsWireframe)
		{
			context->RSSetState(RasterizerStates::Wireframe);
			context->Draw(NUM_TERRAIN_PATCHES_PER_TILE * NUM_TERRAIN_PATCHES_PER_TILE, 0);
			context->RSSetState(nullptr);
		}
		else
			context->Draw(NUM_TERRAIN_PATCHES_PER_TILE * NUM_TERRAIN_PATCHES_PER_TILE, 0);

		//reset back
		ID3D11SamplerState* SS_null[1] = { nullptr };
		context->DSSetSamplers(0, 1, SS_null);
		context->PSSetSamplers(0, 1, SS_null);

		context->IASetPrimitiveTopology(originalPrimitiveTopology);
		context->VSSetShader(NULL, NULL, 0);
		context->HSSetShader(NULL, NULL, 0);
		context->DSSetShader(NULL, NULL, 0);
		context->PSSetShader(NULL, NULL, 0);
	}


	float HeightMap::FindHeightFromPosition(float x, float z)
	{
		int index = 0;
		float vertex1[3] = { 0.0, 0.0, 0.0 };
		float vertex2[3] = { 0.0, 0.0, 0.0 }; 
		float vertex3[3] = { 0.0, 0.0, 0.0 }; 
		float normals[3] = { 0.0, 0.0, 0.0 }; 
		float height = 0.0f;
		bool isFound = false;

		int index1 = 0;
		int index2 = 0;
		int index3 = 0;

		for (int i = 0; i < mVertexCountNonTS / 3; i++)
		{
			index = i * 3;

			vertex1[0] = mVertexList[index].x;
			vertex1[1] = mVertexList[index].y;
			vertex1[2] = mVertexList[index].z;
			index++;

			vertex2[0] = mVertexList[index].x;
			vertex2[1] = mVertexList[index].y;
			vertex2[2] = mVertexList[index].z;
			index++;

			vertex3[0] = mVertexList[index].x;
			vertex3[1] = mVertexList[index].y;
			vertex3[2] = mVertexList[index].z;

			// Check to see if this is the polygon we are looking for.
			isFound = GetHeightFromTriangle(x, z, vertex1, vertex2, vertex3, normals, height);
			if (isFound)
				return height;
		}
		return -1.0f;
	}

	bool HeightMap::PerformCPUFrustumCulling(Camera* camera)
	{
		if (!camera)
		{
			mIsCulled = false;
			return mIsCulled;
		}

		auto frustum = camera->GetFrustum();
		bool culled = false;
		// start a loop through all frustum planes
		for (int planeID = 0; planeID < 6; ++planeID)
		{
			XMVECTOR planeNormal = XMVectorSet(frustum.Planes()[planeID].x, frustum.Planes()[planeID].y, frustum.Planes()[planeID].z, 0.0f);
			float planeConstant = frustum.Planes()[planeID].w;

			XMFLOAT3 axisVert;

			// x-axis
			if (frustum.Planes()[planeID].x > 0.0f)
				axisVert.x = mAABB.first.x;
			else
				axisVert.x = mAABB.second.x;

			// y-axis
			if (frustum.Planes()[planeID].y > 0.0f)
				axisVert.y = mAABB.first.y;
			else
				axisVert.y = mAABB.second.y;

			// z-axis
			if (frustum.Planes()[planeID].z > 0.0f)
				axisVert.z = mAABB.first.z;
			else
				axisVert.z = mAABB.second.z;


			if (XMVectorGetX(XMVector3Dot(planeNormal, XMLoadFloat3(&axisVert))) + planeConstant > 0.0f)
			{
				culled = true;
				// Skip remaining planes to check and move on 
				break;
			}
		}
		mIsCulled = culled;
		return culled;
	}

	bool HeightMap::RayIntersectsTriangle(float x, float z, float v0[3], float v1[3], float v2[3], float normals[3], float& height)
	{
		const float EPSILON = 0.00001f;

		XMVECTOR directionVector = { 0.0f, 1.0f, 0.0f };
		XMVECTOR originVector = { x, 0.0f, z };

		XMVECTOR edge1 = { v1[0] - v0[0],v1[1] - v0[1], v1[2] - v0[2] };
		XMVECTOR edge2 = { v2[0] - v0[0],v2[1] - v0[1], v2[2] - v0[2] };

		XMVECTOR h = XMVector3Cross(directionVector, edge2);

		float a = 0.0f;
		XMStoreFloat(&a, XMVector3Dot(edge1, h));

		if (a > -EPSILON && a < EPSILON)
			return false;    // This ray is parallel to this triangle.
		float f = 1.0f / a;
		XMVECTOR s = XMVectorSubtract(originVector, XMVECTOR{v0[0],v0[1],v0[2]});
		
		float dotSH;
		XMStoreFloat(&dotSH, XMVector3Dot(s, h));

		float u = f * dotSH;
		if (u < 0.0 || u > 1.0)
			return false;

		XMVECTOR q = XMVector3Cross(s, edge1);

		float dotDirQ;
		XMStoreFloat(&dotDirQ, XMVector3Dot(directionVector, q));

		float v = f * dotDirQ;
		if (v < 0.0 || u + v > 1.0)
			return false;

		// At this stage we can compute t to find out where the intersection point is on the line.
		float dotEdge2Q;
		XMStoreFloat(&dotEdge2Q, XMVector3Dot(edge2, q));
		float t = f * dotEdge2Q;
		if (t > EPSILON) // ray intersection
		{
			//outIntersectionPoint = rayOrigin + rayVector * t;
			return true;
		}
		else // This means that there is a line intersection but not a ray intersection.
			return false;
	}

	bool HeightMap::GetHeightFromTriangle(float x, float z, float v0[3], float v1[3], float v2[3], float normals[3], float& height)
	{
		float startVector[3], directionVector[3], edge1[3], edge2[3], normal[3];
		float Q[3], e1[3], e2[3], e3[3], edgeNormal[3], temp[3];
		float magnitude, D, denominator, numerator, t, determinant;


		// Starting position of the ray that is being cast.
		startVector[0] = x;
		startVector[1] = 0.0f;
		startVector[2] = z;

		// The direction the ray is being cast.
		directionVector[0] = 0.0f;
		directionVector[1] = -1.0f;
		directionVector[2] = 0.0f;

		// Calculate the two edges from the three points given.
		edge1[0] = v1[0] - v0[0];
		edge1[1] = v1[1] - v0[1];
		edge1[2] = v1[2] - v0[2];

		edge2[0] = v2[0] - v0[0];
		edge2[1] = v2[1] - v0[1];
		edge2[2] = v2[2] - v0[2];

		// Calculate the normal of the triangle from the two edges.
		normal[0] = (edge1[1] * edge2[2]) - (edge1[2] * edge2[1]);
		normal[1] = (edge1[2] * edge2[0]) - (edge1[0] * edge2[2]);
		normal[2] = (edge1[0] * edge2[1]) - (edge1[1] * edge2[0]);

		magnitude = (float)sqrt((normal[0] * normal[0]) + (normal[1] * normal[1]) + (normal[2] * normal[2]));
		normal[0] = normal[0] / magnitude;
		normal[1] = normal[1] / magnitude;
		normal[2] = normal[2] / magnitude;

		// Find the distance from the origin to the plane.
		D = ((-normal[0] * v0[0]) + (-normal[1] * v0[1]) + (-normal[2] * v0[2]));

		// Get the denominator of the equation.
		denominator = ((normal[0] * directionVector[0]) + (normal[1] * directionVector[1]) + (normal[2] * directionVector[2]));

		// Make sure the result doesn't get too close to zero to prevent divide by zero.
		if (fabs(denominator) < 0.0001f)
		{
			return false;
		}

		// Get the numerator of the equation.
		numerator = -1.0f * (((normal[0] * startVector[0]) + (normal[1] * startVector[1]) + (normal[2] * startVector[2])) + D);

		// Calculate where we intersect the triangle.
		t = numerator / denominator;

		// Find the intersection vector.
		Q[0] = startVector[0] + (directionVector[0] * t);
		Q[1] = startVector[1] + (directionVector[1] * t);
		Q[2] = startVector[2] + (directionVector[2] * t);

		// Find the three edges of the triangle.
		e1[0] = v1[0] - v0[0];
		e1[1] = v1[1] - v0[1];
		e1[2] = v1[2] - v0[2];

		e2[0] = v2[0] - v1[0];
		e2[1] = v2[1] - v1[1];
		e2[2] = v2[2] - v1[2];

		e3[0] = v0[0] - v2[0];
		e3[1] = v0[1] - v2[1];
		e3[2] = v0[2] - v2[2];

		// Calculate the normal for the first edge.
		edgeNormal[0] = (e1[1] * normal[2]) - (e1[2] * normal[1]);
		edgeNormal[1] = (e1[2] * normal[0]) - (e1[0] * normal[2]);
		edgeNormal[2] = (e1[0] * normal[1]) - (e1[1] * normal[0]);

		// Calculate the determinant to see if it is on the inside, outside, or directly on the edge.
		temp[0] = Q[0] - v0[0];
		temp[1] = Q[1] - v0[1];
		temp[2] = Q[2] - v0[2];

		determinant = ((edgeNormal[0] * temp[0]) + (edgeNormal[1] * temp[1]) + (edgeNormal[2] * temp[2]));

		// Check if it is outside.
		if (determinant > 0.001f)
		{
			return false;
		}

		// Calculate the normal for the second edge.
		edgeNormal[0] = (e2[1] * normal[2]) - (e2[2] * normal[1]);
		edgeNormal[1] = (e2[2] * normal[0]) - (e2[0] * normal[2]);
		edgeNormal[2] = (e2[0] * normal[1]) - (e2[1] * normal[0]);

		// Calculate the determinant to see if it is on the inside, outside, or directly on the edge.
		temp[0] = Q[0] - v1[0];
		temp[1] = Q[1] - v1[1];
		temp[2] = Q[2] - v1[2];

		determinant = ((edgeNormal[0] * temp[0]) + (edgeNormal[1] * temp[1]) + (edgeNormal[2] * temp[2]));

		// Check if it is outside.
		if (determinant > 0.001f)
		{
			return false;
		}

		// Calculate the normal for the third edge.
		edgeNormal[0] = (e3[1] * normal[2]) - (e3[2] * normal[1]);
		edgeNormal[1] = (e3[2] * normal[0]) - (e3[0] * normal[2]);
		edgeNormal[2] = (e3[0] * normal[1]) - (e3[1] * normal[0]);

		// Calculate the determinant to see if it is on the inside, outside, or directly on the edge.
		temp[0] = Q[0] - v2[0];
		temp[1] = Q[1] - v2[1];
		temp[2] = Q[2] - v2[2];

		determinant = ((edgeNormal[0] * temp[0]) + (edgeNormal[1] * temp[1]) + (edgeNormal[2] * temp[2]));

		// Check if it is outside.
		if (determinant > 0.001f)
		{
			return false;
		}

		// Now we have our height.
		height = Q[1];

		return true;
	}

	bool HeightMap::IsColliding(const XMFLOAT4& position, bool onlyXZCheck)
	{
		bool isColliding =  onlyXZCheck ?
			((position.x <= mAABB.second.x && position.x >= mAABB.first.x) &&
			(position.z <= mAABB.second.z && position.z >= mAABB.first.z)) :
			((position.x <= mAABB.second.x && position.x >= mAABB.first.x) &&
			(position.y <= mAABB.second.y && position.y >= mAABB.first.y) &&
			(position.z <= mAABB.second.z && position.z >= mAABB.first.z));

		return isColliding;
	}

	// Method for displacing points on terrain (send some points to the GPU, get transformed points from the GPU).
	// GPU does everything in a compute shader (it finds a proper terrain tile, checks for the splat channel and transforms the provided points).
	// There is also some older functionality (USE_RAYCASTING_FOR_ON_TERRAIN_PLACEMENT) if you do not want to check heightmap collisions (fast) but use raycasts to geometry instead (slow).
	// Warning: ideally you should not be reading back from the GPU in the same frame (-> stalling), but this method is not supposed to be called every frame
	// 
	// Use cases: 
	// - placing ER_RenderingObject(s) on terrain (even their instances individually)
	// - placing ER_Foliage patches on terrain (batch placement)
	void ER_Terrain::PlaceOnTerrain(XMFLOAT4* positions, int positionsCount, TerrainSplatChannels splatChannel, XMFLOAT4* terrainVertices, int terrainVertexCount)
	{
		auto context = GetGame()->Direct3DDeviceContext();

		UINT initCounts = 0;

		ID3D11Buffer* posBuffer = NULL;
		ID3D11Buffer* outputPosBuffer = NULL;
		ID3D11UnorderedAccessView* posUAV = NULL;

		ID3D11ShaderResourceView* terrainBufferSRV = NULL;
		ID3D11Buffer* terrainBuffer = NULL;
#if USE_RAYCASTING_FOR_ON_TERRAIN_PLACEMENT
		// terrain vertex buffer
		{
			D3D11_SUBRESOURCE_DATA data = { terrainVertices, 0, 0 };

			D3D11_BUFFER_DESC buf_descTerrain;
			buf_descTerrain.ByteWidth = sizeof(XMFLOAT4) * terrainVertexCount;
			buf_descTerrain.Usage = D3D11_USAGE_DEFAULT;
			buf_descTerrain.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			buf_descTerrain.CPUAccessFlags = 0;
			buf_descTerrain.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
			buf_descTerrain.StructureByteStride = sizeof(XMFLOAT4);
			if (FAILED(mGame->Direct3DDevice()->CreateBuffer(&buf_descTerrain, terrainVertices != NULL ? &data : NULL, &terrainBuffer)))
				throw GameException("Failed to create terrain vertices buffer in ER_Terrain::PlaceOnTerrainTile(). Maybe increase TDR of your graphics driver...");

			D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
			srv_desc.Format = DXGI_FORMAT_UNKNOWN;
			srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			srv_desc.Buffer.FirstElement = 0;
			srv_desc.Buffer.NumElements = terrainVertexCount;
			if (FAILED(mGame->Direct3DDevice()->CreateShaderResourceView(terrainBuffer, &srv_desc, &terrainBufferSRV)))
				throw GameException("Failed to create terrain vertices SRV buffer in ER_Terrain::PlaceOnTerrainTile().");
		}
#endif
		// positions buffers
		{
			D3D11_SUBRESOURCE_DATA init_data = { positions, 0, 0 };
			D3D11_BUFFER_DESC buf_desc;
			buf_desc.ByteWidth = sizeof(XMFLOAT4) * positionsCount;
			buf_desc.Usage = D3D11_USAGE_DEFAULT;
			buf_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
			buf_desc.CPUAccessFlags = 0;
			buf_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
			buf_desc.StructureByteStride = sizeof(XMFLOAT4);
			if (FAILED(mGame->Direct3DDevice()->CreateBuffer(&buf_desc, positions != NULL ? &init_data : NULL, &posBuffer)))
				throw GameException("Failed to create positions GPU structured buffer in ER_Terrain::PlaceOnTerrainTile().");

			// uav for positions
			D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
			uav_desc.Format = DXGI_FORMAT_UNKNOWN;
			uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			uav_desc.Buffer.FirstElement = 0;
			uav_desc.Buffer.NumElements = positionsCount;
			uav_desc.Buffer.Flags = 0;
			if (FAILED(mGame->Direct3DDevice()->CreateUnorderedAccessView(posBuffer, &uav_desc, &posUAV)))
				throw GameException("Failed to create UAV of positions buffer in ER_Terrain::PlaceOnTerrainTile().");

			// create the ouput buffer for storing data from GPU for positions
			buf_desc.Usage = D3D11_USAGE_STAGING;
			buf_desc.BindFlags = 0;
			buf_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
			if (FAILED(mGame->Direct3DDevice()->CreateBuffer(&buf_desc, 0, &outputPosBuffer)))
				throw GameException("Failed to create positions GPU output buffer in ER_Terrain::PlaceOnTerrain().");
		}

		context->CSSetShader(mPlaceOnTerrainCS, NULL, 0);

		mPlaceOnTerrainConstantBuffer.Data.HeightScale = mTerrainTessellatedHeightScale;
		mPlaceOnTerrainConstantBuffer.Data.SplatChannel = splatChannel == TerrainSplatChannels::NONE ? -1.0f : static_cast<float>(splatChannel);
		mPlaceOnTerrainConstantBuffer.Data.TerrainTileCount = static_cast<int>(mNumTiles);
		mPlaceOnTerrainConstantBuffer.Data.PlacementHeightDelta = mPlacementHeightDelta;
		mPlaceOnTerrainConstantBuffer.ApplyChanges(context);
		ID3D11Buffer* CBs[1] = { mPlaceOnTerrainConstantBuffer.Buffer() };
		context->CSSetConstantBuffers(0, 1, CBs);

		ID3D11SamplerState* SS[2] = { SamplerStates::TrilinearPointClamp, SamplerStates::TrilinearWrap };
		context->CSSetSamplers(0, 2, SS);

		ID3D11ShaderResourceView* SRVs[4] = {
			mTerrainTilesDataGPU->GetBufferSRV(),
			terrainBufferSRV,
			mTerrainTilesHeightmapsArrayTexture->GetSRV(),
			mTerrainTilesSplatmapsArrayTexture->GetSRV()
		};

		context->CSSetShaderResources(0, 4, SRVs);
		context->CSSetUnorderedAccessViews(0, 1, &posUAV, &initCounts);
		context->Dispatch(512, 1, 1);

		// read results
		context->CopyResource(outputPosBuffer, posBuffer);
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		HRESULT hr = context->Map(outputPosBuffer, 0, D3D11_MAP_READ, 0, &mappedResource);

		if (SUCCEEDED(hr))
		{
			XMFLOAT4* newPositions = reinterpret_cast<XMFLOAT4*>(mappedResource.pData);
			for (size_t i = 0; i < positionsCount; i++)
				positions[i] = newPositions[i];
		}
		else
			throw GameException("Failed to read new positions from GPU in output buffer in ER_Terrain::PlaceOnTerrain().");

		context->Unmap(outputPosBuffer, 0);

		// Unbind resources for CS
		ID3D11UnorderedAccessView* UAViewNULL[1] = { NULL };
		context->CSSetUnorderedAccessViews(0, 1, UAViewNULL, &initCounts);
		ID3D11ShaderResourceView* SRVNULL[3] = { NULL, NULL, NULL };
		context->CSSetShaderResources(0, 3, SRVNULL);
		ID3D11Buffer* CBNULL[1] = { NULL };
		context->CSSetConstantBuffers(0, 1, CBNULL);
		ID3D11SamplerState* SSNULL[2] = { NULL, NULL };
		context->CSSetSamplers(0, 2, SSNULL);

		ReleaseObject(posBuffer);
		ReleaseObject(outputPosBuffer);
		ReleaseObject(posUAV);
		ReleaseObject(terrainBuffer);
		ReleaseObject(terrainBufferSRV);
	}

	HeightMap::HeightMap(int width, int height)
	{
		mData = new MapData[width * height];
		mVertexList = new Vertex[(width - 1) * (height - 1) * 6];
	}

	HeightMap::~HeightMap()
	{		
		ReleaseObject(mVertexBufferTS);
		ReleaseObject(mVertexBufferNonTS);
		ReleaseObject(mIndexBufferNonTS);
		DeleteObject(mSplatTexture);
		DeleteObject(mHeightTexture);
		DeleteObjects(mVertexList);
		DeleteObjects(mData);
		DeleteObject(mDebugGizmoAABB);
	}
}