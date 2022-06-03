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
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Terrain\\Terrain.hlsl").c_str(), "PSMain", "ps_5_0", &blob)))
				throw GameException("Failed to load PSMain from shader: Terrain.hlsl!");
			if (FAILED(GetGame()->Direct3DDevice()->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mPS)))
				throw GameException("Failed to create pixel shader from Terrain.hlsl!");
			blob->Release();
		}

		mTerrainConstantBuffer.Initialize(GetGame()->Direct3DDevice());
	}

	ER_Terrain::~ER_Terrain()
	{
		DeletePointerCollection(mHeightMaps);
		for (size_t i = 0; i < NUM_TEXTURE_SPLAT_CHANNELS; i++)
			DeleteObject(mSplatChannelTextures[i]);

		ReleaseObject(mVS);
		ReleaseObject(mHS);
		ReleaseObject(mDS);
		ReleaseObject(mPS);
		ReleaseObject(mInputLayout);
		mTerrainConstantBuffer.Release();
	}

	void ER_Terrain::LoadTerrainData(ER_Scene* aScene)
	{
		if (!aScene->HasTerrain())
		{
			mEnabled = false;
			return;
		}
		assert(!mLevelPath.empty());

		mNumTiles = aScene->GetTerrainTilesCount();
		assert(mNumTiles > 0);

		mWidth = mHeight = /*aTileResolution*/ TERRAIN_TILE_RESOLUTION; //TODO

		if (!(mNumTiles && !(mNumTiles & (mNumTiles - 1))))
			throw GameException("Number of tiles defined is not a power of 2!");

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
				
				//std::wstring filePathNormalmap = aTexturesPath;
				//filePathNormalmap += L"terrainNormal_x" + std::to_wstring(i) + L"_y" + std::to_wstring(j) + L".png";
				//LoadNormalmapPerTileGPU(i, j, filePathNormalmap); //unfortunately, not thread safe
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

		LoadRawHeightmapPerTileCPU(tileX, tileY, filePathHeightmap);
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

	//void Terrain::LoadNormalmapPerTileGPU(int tileIndexX, int tileIndexY, const std::string& path)
	//{
	//	int tileIndex = tileIndexX * sqrt(mNumTiles) + tileIndexY;
	//	if (tileIndex >= mHeightMaps.size())
	//		return;
	//
	//	std::wstring pathW = Utility::ToWideString(path);
	//	if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), pathW.c_str(), nullptr, &(mHeightMaps[tileIndex]->mNormalTexture))))
	//	{
	//		std::string errorMessage = "Failed to create tile's 'Normal Map' SRV: " + path;
	//		throw GameException(errorMessage.c_str());
	//	}
	//}

	void ER_Terrain::LoadRawHeightmapPerTileCPU(int tileIndexX, int tileIndexY, const std::wstring& aPath)
	{
		int tileIndex = tileIndexX * sqrt(mNumTiles) + tileIndexY;
		if (tileIndex >= mHeightMaps.size())
			return;
		//assert(tileIndex < mHeightMaps.size());

		mHeightMaps[tileIndex]->mUVOffsetToTextureSpace = XMFLOAT2(TERRAIN_TILE_RESOLUTION - tileIndexX * TERRAIN_TILE_RESOLUTION, tileIndexY * TERRAIN_TILE_RESOLUTION);

		int error, i, j, index;
		FILE* filePtr;
		unsigned long long imageSize, count;
		unsigned short* rawImage;

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


		mHeightMaps[tileIndex]->mWorldMatrixTS = XMMatrixTranslation(TERRAIN_TILE_RESOLUTION * (tileIndexX - 1), 0.0f, TERRAIN_TILE_RESOLUTION * -tileIndexY);

		// genertate tessellated vertex buffers
		{
			// creating terrain vertex buffer for patches
			float* patches_rawdata = new float[NUM_TERRAIN_PATCHES_PER_TILE * NUM_TERRAIN_PATCHES_PER_TILE * 4];
			for (int i = 0; i < NUM_TERRAIN_PATCHES_PER_TILE; i++)
				for (int j = 0; j < NUM_TERRAIN_PATCHES_PER_TILE; j++)
				{
					patches_rawdata[(i + j * NUM_TERRAIN_PATCHES_PER_TILE) * 4 + 0] = i * (TERRAIN_TILE_RESOLUTION) / NUM_TERRAIN_PATCHES_PER_TILE;
					patches_rawdata[(i + j * NUM_TERRAIN_PATCHES_PER_TILE) * 4 + 1] = j * (TERRAIN_TILE_RESOLUTION) / NUM_TERRAIN_PATCHES_PER_TILE;
					patches_rawdata[(i + j * NUM_TERRAIN_PATCHES_PER_TILE) * 4 + 2] = TERRAIN_TILE_RESOLUTION / NUM_TERRAIN_PATCHES_PER_TILE;
					patches_rawdata[(i + j * NUM_TERRAIN_PATCHES_PER_TILE) * 4 + 3] = TERRAIN_TILE_RESOLUTION / NUM_TERRAIN_PATCHES_PER_TILE;
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

			if (FAILED(mGame->Direct3DDevice()->CreateBuffer(&buf_desc, &subresource_data, &(mHeightMaps[tileIndex]->mVertexBufferTS))))
				throw GameException("ID3D11Device::CreateBuffer() failed while generating vertex buffer of terrain mesh patch");

			free(patches_rawdata);
		}

		// Copy the image data into the height map array.
		for (j = 0; j < static_cast<int>(mHeight); j++)
		{
			for (i = 0; i < static_cast<int>(mWidth); i++)
			{
				index = (mWidth * j) + i;

				// Store the height at this point in the height map array.
				mHeightMaps[tileIndex]->mData[index].x = static_cast<float>((i + static_cast<int>(mWidth) * (tileIndexX - 1)));
				mHeightMaps[tileIndex]->mData[index].y = static_cast<float>(rawImage[index]) / 200.0f;//TODO mTerrainNonTessellatedHeightScale;
				mHeightMaps[tileIndex]->mData[index].z = static_cast<float>((j - static_cast<int>(mHeight) * tileIndexY));

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

		return; //i dont remember why i wrote the code below, but its not needed for the GPU tessellated terrain

		//calculate normals
		{
			int i, j, index1, index2, index3, index, count;
			float vertex1[3], vertex2[3], vertex3[3], vector1[3], vector2[3], sum[3], length;
			NormalVector* normals = new NormalVector[((int)mHeight - 1) * ((int)mWidth - 1)];

			for (j = 0; j < (static_cast<int>(mHeight) - 1); j++)
			{
				for (i = 0; i < (static_cast<int>(mWidth) - 1); i++)
				{
					index1 = (j * mHeight) + i;
					index2 = (j * mHeight) + (i + 1);
					index3 = ((j + 1) * mHeight) + i;

					// Get three vertices from the face.
					vertex1[0] = mHeightMaps[tileIndexX *  sqrt(mNumTiles) + tileIndexY]->mData[index1].x;
					vertex1[1] = mHeightMaps[tileIndexX *  sqrt(mNumTiles) + tileIndexY]->mData[index1].y;
					vertex1[2] = mHeightMaps[tileIndexX *  sqrt(mNumTiles) + tileIndexY]->mData[index1].z;
					vertex2[0] = mHeightMaps[tileIndexX *  sqrt(mNumTiles) + tileIndexY]->mData[index2].x;
					vertex2[1] = mHeightMaps[tileIndexX *  sqrt(mNumTiles) + tileIndexY]->mData[index2].y;
					vertex2[2] = mHeightMaps[tileIndexX *  sqrt(mNumTiles) + tileIndexY]->mData[index2].z;
					vertex3[0] = mHeightMaps[tileIndexX *  sqrt(mNumTiles) + tileIndexY]->mData[index3].x;
					vertex3[1] = mHeightMaps[tileIndexX *  sqrt(mNumTiles) + tileIndexY]->mData[index3].y;
					vertex3[2] = mHeightMaps[tileIndexX *  sqrt(mNumTiles) + tileIndexY]->mData[index3].z;

					// Calculate the two vectors for this face.
					vector1[0] = vertex1[0] - vertex3[0];
					vector1[1] = vertex1[1] - vertex3[1];
					vector1[2] = vertex1[2] - vertex3[2];
					vector2[0] = vertex3[0] - vertex2[0];
					vector2[1] = vertex3[1] - vertex2[1];
					vector2[2] = vertex3[2] - vertex2[2];

					index = (j * (mHeight - 1)) + i;

					// Calculate the cross product of those two vectors to get the un-normalized value for this face normal.
					normals[index].x = (vector1[1] * vector2[2]) - (vector1[2] * vector2[1]);
					normals[index].y = (vector1[2] * vector2[0]) - (vector1[0] * vector2[2]);
					normals[index].z = (vector1[0] * vector2[1]) - (vector1[1] * vector2[0]);
				}
			}

			// Now go through all the vertices and take an average of each face normal 	
			// that the vertex touches to get the averaged normal for that vertex.
			for (j = 0; j < static_cast<int>(mHeight); j++)
			{
				for (i = 0; i < static_cast<int>(mWidth); i++)
				{
					// Initialize the sum.
					sum[0] = 0.0f;
					sum[1] = 0.0f;
					sum[2] = 0.0f;

					// Initialize the count.
					count = 0;

					// Bottom left face.
					if (((i - 1) >= 0) && ((j - 1) >= 0))
					{
						index = ((j - 1) * (static_cast<int>(mHeight) - 1)) + (i - 1);

						sum[0] += normals[index].x;
						sum[1] += normals[index].y;
						sum[2] += normals[index].z;
						count++;
					}

					// Bottom right face.
					if ((i < ((int)mWidth - 1)) && ((j - 1) >= 0))
					{
						index = ((j - 1) * (static_cast<int>(mHeight) - 1)) + i;

						sum[0] += normals[index].x;
						sum[1] += normals[index].y;
						sum[2] += normals[index].z;
						count++;
					}

					// Upper left face.
					if (((i - 1) >= 0) && (j < (static_cast<int>(mHeight) - 1)))
					{
						index = (j * (static_cast<int>(mHeight) - 1)) + (i - 1);

						sum[0] += normals[index].x;
						sum[1] += normals[index].y;
						sum[2] += normals[index].z;
						count++;
					}

					// Upper right face.
					if (i < ((static_cast<int>(mWidth) - 1)) && j < (static_cast<int>(mHeight) - 1))
					{
						index = (j * (static_cast<int>(mHeight) - 1)) + i;

						sum[0] += normals[index].x;
						sum[1] += normals[index].y;
						sum[2] += normals[index].z;
						count++;
					}

					// Take the average of the faces touching this vertex.
					sum[0] = (sum[0] / static_cast<float>(count));
					sum[1] = (sum[1] / static_cast<float>(count));
					sum[2] = (sum[2] / static_cast<float>(count));

					// Calculate the length of this normal.
					length = sqrt((sum[0] * sum[0]) + (sum[1] * sum[1]) + (sum[2] * sum[2]));

					// Get an index to the vertex location in the height map array.
					index = (j * static_cast<int>(mHeight)) + i;

					// Normalize the final shared normal for this vertex and store it in the height map array.
					mHeightMaps[tileIndexX *  sqrt(mNumTiles) + tileIndexY]->mData[index].normalX = (sum[0] / length);
					mHeightMaps[tileIndexX *  sqrt(mNumTiles) + tileIndexY]->mData[index].normalY = (sum[1] / length);
					mHeightMaps[tileIndexX *  sqrt(mNumTiles) + tileIndexY]->mData[index].normalZ = (sum[2] / length);
				}
			}
			
			delete[] normals;
			normals = NULL;
		}

		//calculate uvs
		{
			int index, incrementCount, incrementCountTile, i, j, uCount, vCount, uCountTile, vCountTile, repeatValue = 32;
			float incrementValue, incrementValueTile, uCoord, vCoord, uCoordTile, vCoordTile;

			incrementValue = static_cast<float>(repeatValue) / static_cast<float>(mWidth);
			incrementCount = static_cast<int>(mWidth) / repeatValue;	

			incrementValueTile = 1.0f / static_cast<float>(mWidth);
			incrementCountTile = static_cast<int>(mWidth);

			uCoord = 0.0f;
			vCoord = 1.0f;	
			uCoordTile = 0.0f;
			vCoordTile = 1.0f;

			// Initialize the tu and tv coordinate indices.
			uCount = 0;
			vCount = 0;	
			uCountTile = 0;
			vCountTile = 0;

			for (j = 0; j < static_cast<int>(mHeight); j++)
			{
				for (i = 0; i < static_cast<int>(mWidth); i++)
				{
					index = (j * static_cast<int>(mHeight)) + i;

					// Store the texture coordinate in the height map.
					mHeightMaps[tileIndexX *  sqrt(mNumTiles) + tileIndexY]->mData[index].u = uCoord;
					mHeightMaps[tileIndexX *  sqrt(mNumTiles) + tileIndexY]->mData[index].v = vCoord;	
					mHeightMaps[tileIndexX *  sqrt(mNumTiles) + tileIndexY]->mData[index].uTile = uCoordTile;
					mHeightMaps[tileIndexX *  sqrt(mNumTiles) + tileIndexY]->mData[index].vTile = vCoordTile;
					
					// Increment the tu texture coordinate by the increment value and increment the index by one.
					uCoord += incrementValue;
					uCount++;

					uCoordTile += incrementValueTile;
					uCountTile++;

					// Check if at the far right end of the texture and if so then start at the beginning again.
					if (uCount == incrementCount)
					{
						uCoord = 0.0f;
						uCount = 0;
					}
					if (uCountTile == incrementCountTile)
					{
						uCoordTile = 0.0f;
						uCountTile = 0;
					}
				}

				vCoord -= incrementValue;
				vCount++;

				// Check if at the top of the texture and if so then start at the bottom again.
				if (vCount == incrementCount)
				{
					vCoord = 1.0f;
					vCount = 0;
				}

				vCoordTile -= incrementValueTile;
				vCountTile++;

				// Check if at the top of the texture and if so then start at the bottom again.
				if (vCountTile == incrementCountTile)
				{
					vCoordTile = 1.0f;
					vCountTile = 0;
				}
			}
		}
	}

	void ER_Terrain::Draw(ER_ShadowMapper* worldShadowMapper)
	{
		if (!mEnabled)
			return;

		for (int i = 0; i < mHeightMaps.size(); i++)
			DrawTessellated(i, worldShadowMapper);
	}

	void ER_Terrain::Update(const GameTime& gameTime)
	{
		if (mShowDebug) {
			ImGui::Begin("Terrain System");
			ImGui::Checkbox("Enabled", &mEnabled);
			ImGui::Checkbox("Render wireframe", &mIsWireframe);
			ImGui::SliderInt("Tessellation factor static", &mTessellationFactor, 1, 64);
			ImGui::SliderInt("Tessellation factor dynamic", &mTessellationFactorDynamic, 1, 64);
			ImGui::Checkbox("Use dynamic tessellation", &mUseDynamicTessellation);
			ImGui::SliderFloat("Dynamic LOD distance factor", &mTessellationDistanceFactor, 0.0001f, 0.1f);
			ImGui::SliderFloat("Tessellated terrain height scale", &mTerrainTessellatedHeightScale, 0.0f, 1000.0f);
			ImGui::End();
		}
	}

	void ER_Terrain::DrawTessellated(int tileIndex, ER_ShadowMapper* worldShadowMapper)
	{
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
		}

		mTerrainConstantBuffer.Data.World = XMMatrixTranspose(mHeightMaps[tileIndex]->mWorldMatrixTS);
		mTerrainConstantBuffer.Data.View = XMMatrixTranspose(camera->ViewMatrix());
		mTerrainConstantBuffer.Data.Projection = XMMatrixTranspose(camera->ProjectionMatrix());
		mTerrainConstantBuffer.Data.SunDirection = XMFLOAT4(-mDirectionalLight.Direction().x, -mDirectionalLight.Direction().y, -mDirectionalLight.Direction().z, 1.0f);
		mTerrainConstantBuffer.Data.SunColor = XMFLOAT4{ mDirectionalLight.GetDirectionalLightColor().x, mDirectionalLight.GetDirectionalLightColor().y, mDirectionalLight.GetDirectionalLightColor().z , 1.0f };
		mTerrainConstantBuffer.Data.CameraPosition = XMFLOAT4(camera->Position().x, camera->Position().y, camera->Position().z, 1.0f);
		mTerrainConstantBuffer.Data.TessellationFactor = static_cast<float>(mTessellationFactor);
		mTerrainConstantBuffer.Data.TerrainHeightScale = mTerrainTessellatedHeightScale;
		mTerrainConstantBuffer.Data.TessellationFactorDynamic = static_cast<float>(mTessellationFactorDynamic);
		mTerrainConstantBuffer.Data.UseDynamicTessellation = mUseDynamicTessellation ? 1.0f : 0.0f;
		mTerrainConstantBuffer.Data.DistanceFactor = mTessellationDistanceFactor;
		mTerrainConstantBuffer.ApplyChanges(context);
		ID3D11Buffer* CBs[1] = { mTerrainConstantBuffer.Buffer() };

		context->VSSetConstantBuffers(0, 1, CBs);
		context->DSSetConstantBuffers(0, 1, CBs);
		context->HSSetConstantBuffers(0, 1, CBs);
		context->PSSetConstantBuffers(0, 1, CBs);

		ID3D11ShaderResourceView* SRVS[2 + 4 + NUM_SHADOW_CASCADES] =
		{
			mHeightMaps[tileIndex]->mHeightTexture->GetSRV(),
			mHeightMaps[tileIndex]->mSplatTexture->GetSRV(),
			mSplatChannelTextures[0]->GetSRV(),
			mSplatChannelTextures[1]->GetSRV(),
			mSplatChannelTextures[2]->GetSRV(),
			mSplatChannelTextures[3]->GetSRV()
		};

		if (worldShadowMapper)
			for (int c = 0; c < NUM_SHADOW_CASCADES; c++)
				SRVS[6 + c] = worldShadowMapper->GetShadowTexture(c);

		context->DSSetShaderResources(0, 2 + 4 + NUM_SHADOW_CASCADES, SRVS);
		context->PSSetShaderResources(0, 2 + 4 + NUM_SHADOW_CASCADES, SRVS);

		ID3D11SamplerState* SS[3] = { SamplerStates::TrilinearWrap, SamplerStates::TrilinearClamp, SamplerStates::ShadowSamplerState };
		context->DSSetSamplers(0, 3, SS);
		context->PSSetSamplers(0, 3, SS);

		context->VSSetShader(mVS, NULL, NULL);
		context->HSSetShader(mHS, NULL, NULL);
		context->DSSetShader(mDS, NULL, NULL);
		context->PSSetShader(mPS, NULL, NULL);

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
		float vertex1[3], vertex2[3], vertex3[3], vector1[3], vector2[3], normals[3];
		float height = 0.0f;
		bool isFound = false;

		int index1, index2, index3;

		for (int i = 0; i < mVertexCount / 3; i++)
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
	bool HeightMap::RayIntersectsTriangle(float x, float z, float v0[3], float v1[3], float v2[3], float normals[3],float& height)
	{
		const float EPSILON = 0.0000001;

		XMVECTOR directionVector = { 0.0f, 1.0f, 0.0f };
		XMVECTOR originVector = { x, 0.0f, z };

		XMVECTOR edge1 = { v1[0] - v0[0],v1[1] - v0[1], v1[2] - v0[2] };
		XMVECTOR edge2 = { v2[0] - v0[0],v2[1] - v0[1], v2[2] - v0[2] };

		XMVECTOR h = XMVector3Cross(directionVector, edge2);

		float a;
		XMStoreFloat(&a, XMVector3Dot(edge1, h));

		if (a > -EPSILON && a < EPSILON)
			return false;    // This ray is parallel to this triangle.
		float f = 1.0 / a;
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

	HeightMap::HeightMap(int width, int height)
	{
		mData = new MapData[width * height];
	}

	HeightMap::~HeightMap()
	{		
		ReleaseObject(mVertexBufferTS);
		DeleteObject(mSplatTexture);
		DeleteObject(mHeightTexture);
		DeleteObjects(mData);
	}
}