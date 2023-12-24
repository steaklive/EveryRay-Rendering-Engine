
#include <stdio.h>

#include "ER_Terrain.h"
#include "ER_CoreException.h"
#include "ER_Model.h"
#include "ER_Mesh.h"
#include "ER_Core.h"
#include "ER_MatrixHelper.h"
#include "ER_Utility.h"
#include "ER_VertexDeclarations.h"
#include "ER_ShadowMapper.h"
#include "ER_Scene.h"
#include "ER_DirectionalLight.h"
#include "ER_LightProbesManager.h"
#include "ER_LightProbe.h"
#include "ER_RenderableAABB.h"
#include "ER_Camera.h"
#include "ER_GBuffer.h"

#define USE_RAYCASTING_FOR_ON_TERRAIN_PLACEMENT 0

#define PLACEMENT_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define PLACEMENT_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX 1
#define PLACEMENT_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 2

//used for gbuffer, shadows, forward
#define TERRAIN_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0 
#define TERRAIN_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 1

namespace EveryRay_Core
{
	ER_Terrain::ER_Terrain(ER_Core& pCore, ER_DirectionalLight& light) :
		ER_CoreComponent(pCore),
		mHeightMaps(0, nullptr),
		mDirectionalLight(light)
	{
		ER_RHI* rhi = pCore.GetRHI();

		//shaders
		{
			ER_RHI_INPUT_ELEMENT_DESC inputElementDescriptions[] =
			{
				{ "PATCH_INFO", 0, ER_FORMAT_R32G32B32A32_FLOAT, 0, 0, true, 0 },
				{ "TILE_INDEX", 0, ER_FORMAT_R32_FLOAT, 0, 0xffffffff, true, 0 } //too much for tile index, but whatever for now...
			};
			mInputLayout = rhi->CreateInputLayout(inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));

			mVS = rhi->CreateGPUShader();
			mVS->CompileShader(rhi, "content\\shaders\\Terrain\\Terrain.hlsl", "VSMain", ER_VERTEX, mInputLayout);

			mHS = rhi->CreateGPUShader();
			mHS->CompileShader(rhi, "content\\shaders\\Terrain\\Terrain.hlsl", "HSMain", ER_TESSELLATION_HULL);

			mDS = rhi->CreateGPUShader();
			mDS->CompileShader(rhi, "content\\shaders\\Terrain\\Terrain.hlsl", "DSMain", ER_TESSELLATION_DOMAIN);	
			
			mDS_ShadowMap = rhi->CreateGPUShader();
			mDS_ShadowMap->CompileShader(rhi, "content\\shaders\\Terrain\\Terrain.hlsl", "DSShadowMap", ER_TESSELLATION_DOMAIN);

			mPS = rhi->CreateGPUShader();
			mPS->CompileShader(rhi, "content\\shaders\\Terrain\\Terrain.hlsl", "PSMain", ER_PIXEL);

			mPS_ShadowMap = rhi->CreateGPUShader();
			mPS_ShadowMap->CompileShader(rhi, "content\\shaders\\Terrain\\Terrain.hlsl", "PSShadowMap", ER_PIXEL);

			mPS_GBuffer = rhi->CreateGPUShader();
			mPS_GBuffer->CompileShader(rhi, "content\\shaders\\Terrain\\Terrain.hlsl", "PSGBuffer", ER_PIXEL);

			mPlaceOnTerrainCS = rhi->CreateGPUShader();
			mPlaceOnTerrainCS->CompileShader(rhi, "content\\shaders\\Terrain\\PlaceObjectsOnTerrain.hlsl", "CSMain", ER_COMPUTE);
		}

		// root signatures
		{ 
			mTerrainCommonPassRS = rhi->CreateRootSignature(2, 3);
			if (mTerrainCommonPassRS)
			{
				mTerrainCommonPassRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP);
				mTerrainCommonPassRS->InitStaticSampler(rhi, 1, ER_RHI_SAMPLER_STATE::ER_SHADOW_SS);
				mTerrainCommonPassRS->InitStaticSampler(rhi, 2, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_CLAMP);
				mTerrainCommonPassRS->InitDescriptorTable(rhi, TERRAIN_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 19 });
				mTerrainCommonPassRS->InitDescriptorTable(rhi, TERRAIN_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 2 });
				mTerrainCommonPassRS->Finalize(rhi, "ER_RHI_GPURootSignature: Terrain Common Pass", true);
			}

			mTerrainPlacementPassRS = rhi->CreateRootSignature(3, 2);
			if (mTerrainPlacementPassRS)
			{
				mTerrainPlacementPassRS->InitStaticSampler(rhi, 0, ER_RHI_SAMPLER_STATE::ER_BILINEAR_CLAMP);
				mTerrainPlacementPassRS->InitStaticSampler(rhi, 1, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP);
				mTerrainPlacementPassRS->InitDescriptorTable(rhi, PLACEMENT_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 3 });
				mTerrainPlacementPassRS->InitDescriptorTable(rhi, PLACEMENT_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_UAV }, { 0 }, { 1 });
				mTerrainPlacementPassRS->InitDescriptorTable(rhi, PLACEMENT_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 1 });
				mTerrainPlacementPassRS->Finalize(rhi, "ER_RHI_GPURootSignature: Terrain Placement Pass");
			}
		}
		mTerrainConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: Terrain CB");
		mPlaceOnTerrainConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: Place On Terrain CB");
		for (int i = 0; i < NUM_SHADOW_CASCADES; i++)
			mTerrainShadowBuffers[i].Initialize(rhi, "ER_RHI_GPUBuffer: Terrain Shadow CB #" + std::to_string(i));
	}

	ER_Terrain::~ER_Terrain()
	{
		DeletePointerCollection(mHeightMaps);
		for (int i = 0; i < NUM_TEXTURE_SPLAT_CHANNELS; i++)
			DeleteObject(mSplatChannelTextures[i]);

		DeleteObject(mVS);
		DeleteObject(mHS);
		DeleteObject(mDS);
		DeleteObject(mDS_ShadowMap);
		DeleteObject(mPS);
		DeleteObject(mPS_ShadowMap);
		DeleteObject(mPS_GBuffer);
		DeleteObject(mPlaceOnTerrainCS);
		DeleteObject(mInputLayout);
		DeleteObject(mTerrainTilesDataGPU);
		DeleteObject(mTerrainTilesHeightmapsArrayTexture);
		DeleteObject(mTerrainTilesSplatmapsArrayTexture);
		DeleteObject(mTerrainCommonPassRS);
		DeleteObject(mTerrainPlacementPassRS);

		mTerrainConstantBuffer.Release();
		mPlaceOnTerrainConstantBuffer.Release();
		for (int i = 0; i < NUM_SHADOW_CASCADES; i++)
			mTerrainShadowBuffers[i].Release();

		ReadbackPlacedPositionsOnInitEvent->RemoveAllListeners();
		DeleteObject(ReadbackPlacedPositionsOnInitEvent);
		ReadbackPlacedPositionsOnUpdateEvent->RemoveAllListeners();
		DeleteObject(ReadbackPlacedPositionsOnUpdateEvent);
	}

	void ER_Terrain::LoadTerrainData(ER_Scene* aScene)
	{
		assert(!mLevelPath.empty());
		ER_RHI* rhi = GetCore()->GetRHI();

		if (!aScene->IsValueInSceneRoot("terrain_num_tiles"))
		{
			mEnabled = false;
			mLoaded = false;
			return;
		}

		mNumTiles = aScene->GetValueFromSceneRoot<int>("terrain_num_tiles");
		if (!(mNumTiles && !(mNumTiles & (mNumTiles - 1))))
			throw ER_CoreException("Number of tiles defined is not a power of 2!");

		if (mNumTiles > MAX_TERRAIN_TILE_COUNT)
			throw ER_CoreException("Number of tiles exceeds MAX_TERRAIN_TILE_COUNT!");

		mTileScale = aScene->GetValueFromSceneRoot<float>("terrain_tile_scale");

		mTerrainTessellatedHeightScale = aScene->GetValueFromSceneRoot<float>("terrain_tessellated_height_scale");
		mTerrainNonTessellatedHeightScale = aScene->GetValueFromSceneRoot<float>("terrain_non_tessellated_height_scale");

		mTileResolution = aScene->GetValueFromSceneRoot<int>("terrain_tile_resolution");
		if (mTileResolution == 0)
			throw ER_CoreException("Tile resolution (= heightmap texture tile resolution) of the terrain is 0!");
		mWidth = mHeight = mTileResolution;

		std::wstring terrainSplatLayersTextureNames[NUM_TEXTURE_SPLAT_CHANNELS];
		const std::string baseName = "terrain_texture_splat_layer";
		std::string currentName = baseName;

		for (int i = 0; i < NUM_TEXTURE_SPLAT_CHANNELS; i++)
		{
			currentName += std::to_string(i);
			if (aScene->IsValueInSceneRoot(currentName.c_str()))
				terrainSplatLayersTextureNames[i] = ER_Utility::ToWideString(aScene->GetValueFromSceneRoot<std::string>(currentName.c_str()));
			else
				terrainSplatLayersTextureNames[i] = L"";

			currentName = baseName;
		}

		mLoaded = true;
		
		for (int i = 0; i < mNumTiles; i++)
			mHeightMaps.push_back(new HeightMap(mWidth, mHeight));

		std::wstring path = mLevelPath + L"terrain\\";
		LoadTextures(path, 
			path + terrainSplatLayersTextureNames[0],
			path + terrainSplatLayersTextureNames[1],
			path + terrainSplatLayersTextureNames[2],
			path + terrainSplatLayersTextureNames[3]
		); //not thread-safe

		for (int i = 0; i < mNumTiles; i++)
		{
			LoadTile(i, path); //not thread-safe
		}

		int tileSize = mTileScale * mTileResolution;
		TerrainTileDataGPU* terrainTilesDataCPUBuffer = new TerrainTileDataGPU[mNumTiles];
		for (int tileIndex = 0; tileIndex < mNumTiles; tileIndex++)
		{
			terrainTilesDataCPUBuffer[tileIndex].UVoffsetTileSize = XMFLOAT4(mHeightMaps[tileIndex]->mTileUVOffset.x, mHeightMaps[tileIndex]->mTileUVOffset.y, tileSize, tileSize);
			terrainTilesDataCPUBuffer[tileIndex].AABBMinPoint = XMFLOAT4(mHeightMaps[tileIndex]->mAABB.first.x, mHeightMaps[tileIndex]->mAABB.first.y, mHeightMaps[tileIndex]->mAABB.first.z, 1.0);
			terrainTilesDataCPUBuffer[tileIndex].AABBMaxPoint = XMFLOAT4(mHeightMaps[tileIndex]->mAABB.second.x, mHeightMaps[tileIndex]->mAABB.second.y, mHeightMaps[tileIndex]->mAABB.second.z, 1.0);
		}
		mTerrainTilesDataGPU = rhi->CreateGPUBuffer("ER_RHI_GPUBuffer: Terrain Tiles Data Buffer");
		mTerrainTilesDataGPU->CreateGPUBufferResource(rhi, terrainTilesDataCPUBuffer, mNumTiles, sizeof(TerrainTileDataGPU), false, ER_BIND_SHADER_RESOURCE, 0, ER_RESOURCE_MISC_BUFFER_STRUCTURED);
		DeleteObjects(terrainTilesDataCPUBuffer);

		mTerrainTilesHeightmapsArrayTexture = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Terrain Tiles Heightmaps Array");
		mTerrainTilesHeightmapsArrayTexture->CreateGPUTextureResource(rhi, mTileResolution, mTileResolution, 1, ER_FORMAT_R16_UNORM, ER_BIND_SHADER_RESOURCE, 1, -1, mNumTiles);
		
		mTerrainTilesSplatmapsArrayTexture = rhi->CreateGPUTexture(L"ER_RHI_GPUTexture: Terraub Tiles Splatmaps Array");
		mTerrainTilesSplatmapsArrayTexture->CreateGPUTextureResource(rhi, mTileResolution, mTileResolution, 1, ER_FORMAT_R16G16B16A16_UNORM, ER_BIND_SHADER_RESOURCE, 1, -1, mNumTiles);
		
		for (int tileIndex = 0; tileIndex < mNumTiles; tileIndex++)
		{
			//MipSlice + ArraySlice * MipLevels; => 0 + tileIndex * 1 = tileIndex
			rhi->CopyGPUTextureSubresourceRegion(mTerrainTilesHeightmapsArrayTexture, tileIndex, 0, 0, 0, mHeightMaps[tileIndex]->mHeightTexture, 0);
			rhi->CopyGPUTextureSubresourceRegion(mTerrainTilesSplatmapsArrayTexture, tileIndex, 0, 0, 0, mHeightMaps[tileIndex]->mSplatTexture, 0);
		}
	}

	void ER_Terrain::LoadTextures(const std::wstring& aTexturesPath, const std::wstring& splatLayer0Path, const std::wstring& splatLayer1Path, const std::wstring& splatLayer2Path, const std::wstring& splatLayer3Path)
	{
		ER_RHI* rhi = GetCore()->GetRHI();

		if (!splatLayer0Path.empty())
		{
			mSplatChannelTextures[0] = rhi->CreateGPUTexture(L"");
			mSplatChannelTextures[0]->CreateGPUTextureResource(rhi, splatLayer0Path, true);
			rhi->GenerateMipsWithTextureReplacement(&mSplatChannelTextures[0],
				[this](ER_RHI_GPUTexture** aNewTextureWithMips)
				{
					assert(*aNewTextureWithMips);
					DeleteObject(mSplatChannelTextures[0]);
					mSplatChannelTextures[0] = *aNewTextureWithMips;
				}
			);
		}
		if (!splatLayer1Path.empty())
		{
			mSplatChannelTextures[1] = rhi->CreateGPUTexture(L"");
			mSplatChannelTextures[1]->CreateGPUTextureResource(rhi, splatLayer1Path, true);
			rhi->GenerateMipsWithTextureReplacement(&mSplatChannelTextures[1],
				[this](ER_RHI_GPUTexture** aNewTextureWithMips)
				{
					assert(*aNewTextureWithMips);
					DeleteObject(mSplatChannelTextures[1]);
					mSplatChannelTextures[1] = *aNewTextureWithMips;
				}
			);

		}
		if (!splatLayer2Path.empty())
		{
			mSplatChannelTextures[2] = rhi->CreateGPUTexture(L"");
			mSplatChannelTextures[2]->CreateGPUTextureResource(rhi, splatLayer2Path, true);
			rhi->GenerateMipsWithTextureReplacement(&mSplatChannelTextures[2],
				[this](ER_RHI_GPUTexture** aNewTextureWithMips)
				{
					assert(*aNewTextureWithMips);
					DeleteObject(mSplatChannelTextures[2]);
					mSplatChannelTextures[2] = *aNewTextureWithMips;
				}
			);
		}
		if (!splatLayer3Path.empty())
		{
			mSplatChannelTextures[3] = rhi->CreateGPUTexture(L"");
			mSplatChannelTextures[3]->CreateGPUTextureResource(rhi, splatLayer3Path, true);
			rhi->GenerateMipsWithTextureReplacement(&mSplatChannelTextures[3],
				[this](ER_RHI_GPUTexture** aNewTextureWithMips)
				{
					assert(*aNewTextureWithMips);
					DeleteObject(mSplatChannelTextures[3]);
					mSplatChannelTextures[3] = *aNewTextureWithMips;
				}
			);
		}

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
		ER_RHI* rhi = GetCore()->GetRHI();

		int tileIndex = tileIndexX * sqrt(mNumTiles) + tileIndexY;
		if (tileIndex >= mHeightMaps.size())
			return;

		mHeightMaps[tileIndex]->mSplatTexture = rhi->CreateGPUTexture(L"");
		mHeightMaps[tileIndex]->mSplatTexture->CreateGPUTextureResource(rhi, path, true);
		rhi->GenerateMipsWithTextureReplacement(&mHeightMaps[tileIndex]->mSplatTexture,
			[this, tileIndex](ER_RHI_GPUTexture** aNewTextureWithMips)
			{
				assert(*aNewTextureWithMips);
				DeleteObject(mHeightMaps[tileIndex]->mSplatTexture);
				mHeightMaps[tileIndex]->mSplatTexture = *aNewTextureWithMips;
			}
		);

	}

	void ER_Terrain::LoadHeightmapPerTileGPU(int tileIndexX, int tileIndexY, const std::wstring& path)
	{
		ER_RHI* rhi = GetCore()->GetRHI();

		int tileIndex = tileIndexX * sqrt(mNumTiles) + tileIndexY;
		if (tileIndex >= mHeightMaps.size())
			return;
		
		mHeightMaps[tileIndex]->mHeightTexture = rhi->CreateGPUTexture(L"");
		mHeightMaps[tileIndex]->mHeightTexture->CreateGPUTextureResource(rhi, path, true);
	}

	// Create pre-tessellated patch data which is used for terrain rendering (using GPU tessellation pipeline)
	void ER_Terrain::CreateTerrainTileDataGPU(int tileIndexX, int tileIndexY)
	{
		ER_RHI* rhi = GetCore()->GetRHI();

		int tileIndex = tileIndexX * sqrt(mNumTiles) + tileIndexY;
		assert(tileIndex < mHeightMaps.size());

		int terrainTileSize = mTileResolution * mTileScale;

		const int numOfFloats = 5;

		// creating terrain vertex buffer for patches
		float* patches_rawdata = new float[NUM_TERRAIN_PATCHES_PER_TILE * NUM_TERRAIN_PATCHES_PER_TILE * numOfFloats];
		for (int i = 0; i < NUM_TERRAIN_PATCHES_PER_TILE; i++)
		{
			for (int j = 0; j < NUM_TERRAIN_PATCHES_PER_TILE; j++)
			{
				patches_rawdata[(i + j * NUM_TERRAIN_PATCHES_PER_TILE) * numOfFloats + 0] = i * terrainTileSize / NUM_TERRAIN_PATCHES_PER_TILE;
				patches_rawdata[(i + j * NUM_TERRAIN_PATCHES_PER_TILE) * numOfFloats + 1] = j * terrainTileSize / NUM_TERRAIN_PATCHES_PER_TILE;
				patches_rawdata[(i + j * NUM_TERRAIN_PATCHES_PER_TILE) * numOfFloats + 2] = terrainTileSize / NUM_TERRAIN_PATCHES_PER_TILE;
				patches_rawdata[(i + j * NUM_TERRAIN_PATCHES_PER_TILE) * numOfFloats + 3] = terrainTileSize / NUM_TERRAIN_PATCHES_PER_TILE;

				patches_rawdata[(i + j * NUM_TERRAIN_PATCHES_PER_TILE) * numOfFloats + 4] = static_cast<float>(tileIndex);
			}
		}

		mHeightMaps[tileIndex]->mVertexBufferTS = rhi->CreateGPUBuffer("ER_RHI_GPUBuffer: Terrain Tile (TS) - Vertex Buffer, tile index: " + std::to_string(tileIndex));
		mHeightMaps[tileIndex]->mVertexBufferTS->CreateGPUBufferResource(rhi, patches_rawdata, NUM_TERRAIN_PATCHES_PER_TILE * NUM_TERRAIN_PATCHES_PER_TILE, numOfFloats * sizeof(float), false, ER_BIND_VERTEX_BUFFER);

		DeleteObjects(patches_rawdata);

		mHeightMaps[tileIndex]->mWorldMatrixTS = XMMatrixTranslation(terrainTileSize * (tileIndexX - 1), 0.0f, terrainTileSize * -tileIndexY);
		mHeightMaps[tileIndex]->mTileUVOffset = XMFLOAT2(terrainTileSize - tileIndexX * terrainTileSize, tileIndexY * terrainTileSize);
	}

	// Create CPU tile data which is used for terrain debugging, collisions, placement of ER_RenderingObject(s) (no GPU tessellation pipeline)
	void ER_Terrain::CreateTerrainTileDataCPU(int tileIndexX, int tileIndexY, const std::wstring& aPath)
	{
		int tileIndex = tileIndexX * sqrt(mNumTiles) + tileIndexY;
		assert(tileIndex < mHeightMaps.size());
		ER_RHI* rhi = GetCore()->GetRHI();

		int error, i, j, index;
		FILE* filePtr;
		unsigned long long imageSize, count;
		unsigned short* rawImage;

		// Load raw heightmap file
		{
			// Open the 16 bit raw height map file for reading in binary.
			error = _wfopen_s(&filePtr, aPath.c_str(), L"rb");
			if (error != 0)
				throw ER_CoreException("Can not open the terrain's heightmap RAW!");

			// Calculate the size of the raw image data.
			imageSize = mWidth * mHeight;

			// Allocate memory for the raw image data.
			rawImage = new unsigned short[imageSize];

			// Read in the raw image data.
			count = fread(rawImage, sizeof(unsigned short), imageSize, filePtr);
			if (count != imageSize)
				throw ER_CoreException("Can not read the terrain's heightmap RAW file!");

			// Close the file.
			error = fclose(filePtr);
			if (error != 0)
				throw ER_CoreException("Can not close the terrain's heightmap RAW file!");
		}

		// Copy the image data into the height map array.
		{
			assert(mTerrainNonTessellatedHeightScale > std::numeric_limits<float>::epsilon());

			int tileSize = mTileResolution * mTileScale;
			for (j = 0; j < static_cast<int>(mHeight); j++)
			{
				for (i = 0; i < static_cast<int>(mWidth); i++)
				{
					index = (mWidth * j) + i;

					// Store the height at this point in the height map array.
					mHeightMaps[tileIndex]->mData[index].x = static_cast<float>(i * mTileScale + tileSize * (tileIndexX - 1));
					mHeightMaps[tileIndex]->mData[index].y = static_cast<float>(rawImage[index]) / mTerrainNonTessellatedHeightScale;
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

			mHeightMaps[tileIndex]->mVertexBufferNonTS = rhi->CreateGPUBuffer("ER_RHI_GPUBuffer: Terrain Tile (non-TS) - Vertex Buffer, tile index: " + std::to_string(tileIndex));
			mHeightMaps[tileIndex]->mVertexBufferNonTS->CreateGPUBufferResource(rhi, vertices, mHeightMaps[tileIndex]->mVertexCountNonTS, sizeof(DebugTerrainVertexInput), false, ER_BIND_VERTEX_BUFFER);

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

			DeleteObjects(vertices);

			mHeightMaps[tileIndex]->mIndexBufferNonTS = rhi->CreateGPUBuffer("ER_RHI_GPUBuffer: Terrain Tile (non-TS) - Index Buffer, tile index: " + std::to_string(tileIndex));
			mHeightMaps[tileIndex]->mIndexBufferNonTS->CreateGPUBufferResource(rhi, indices, mHeightMaps[tileIndex]->mIndexCountNonTS, sizeof(unsigned long), false, ER_BIND_INDEX_BUFFER);

			DeleteObjects(indices);

			mHeightMaps[tileIndex]->mDebugGizmoAABB = new ER_RenderableAABB(*GetCore(), XMFLOAT4(0.0, 0.0, 1.0, 1.0));
			mHeightMaps[tileIndex]->mDebugGizmoAABB->InitializeGeometry({ mHeightMaps[tileIndex]->mAABB.first,mHeightMaps[tileIndex]->mAABB.second });
		}
	}

	void ER_Terrain::Draw(TerrainRenderPass aPass, const std::vector<ER_RHI_GPUTexture*>& aRenderTargets, ER_RHI_GPUTexture* aDepthTarget, ER_ShadowMapper* worldShadowMapper, ER_LightProbesManager* probeManager, int shadowMapCascade)
	{
		if (!mEnabled || !mLoaded)
			return;

		if (aPass == TerrainRenderPass::TERRAIN_GBUFFER && !(GetCore()->GetLevel()->mGBuffer->IsEnabled()))
			return;

		ER_RHI* rhi = mCore->GetRHI();
		ER_Camera* camera = (ER_Camera*)(mCore->GetServices().FindService(ER_Camera::TypeIdClass()));

		if (worldShadowMapper && aPass != TerrainRenderPass::TERRAIN_GBUFFER)
		{
			for (int cascade = 0; cascade < NUM_SHADOW_CASCADES; cascade++)
			{
				mTerrainShadowBuffers[cascade].Data.LightViewProjection = XMMatrixTranspose(worldShadowMapper->GetViewMatrix(cascade) * worldShadowMapper->GetProjectionMatrix(cascade));
				mTerrainShadowBuffers[cascade].ApplyChanges(rhi);

				mTerrainConstantBuffer.Data.ShadowMatrices[cascade] = XMMatrixTranspose(worldShadowMapper->GetViewMatrix(cascade) * worldShadowMapper->GetProjectionMatrix(cascade) * XMLoadFloat4x4(&ER_MatrixHelper::GetProjectionShadowMatrix()));
			}

			mTerrainConstantBuffer.Data.ShadowTexelSize = XMFLOAT4{ 1.0f / worldShadowMapper->GetResolution(), 1.0f, 1.0f , 1.0f };
			mTerrainConstantBuffer.Data.ShadowCascadeDistances = XMFLOAT4{ worldShadowMapper->GetCameraFarShadowCascadeDistance(0), worldShadowMapper->GetCameraFarShadowCascadeDistance(1), worldShadowMapper->GetCameraFarShadowCascadeDistance(2), 1.0f };
		}

		int tileIndex = 0;
		for (int i = 0; i < MAX_TERRAIN_TILE_COUNT; i++, tileIndex++)
		{
			if (tileIndex < static_cast<int>(mHeightMaps.size()))
				mTerrainConstantBuffer.Data.TerrainTileWorld[tileIndex] = XMMatrixTranspose(mHeightMaps[tileIndex]->mWorldMatrixTS);
			else
				mTerrainConstantBuffer.Data.TerrainTileWorld[i] = XMMatrixIdentity();
		}
		mTerrainConstantBuffer.Data.View = XMMatrixTranspose(camera->ViewMatrix());
		mTerrainConstantBuffer.Data.Projection = XMMatrixTranspose(camera->ProjectionMatrix());
		mTerrainConstantBuffer.Data.SunDirection = XMFLOAT4(-mDirectionalLight.Direction().x, -mDirectionalLight.Direction().y, -mDirectionalLight.Direction().z, 1.0f);
		mTerrainConstantBuffer.Data.SunColor = XMFLOAT4{ mDirectionalLight.GetColor().x, mDirectionalLight.GetColor().y, mDirectionalLight.GetColor().z, mDirectionalLight.mLightIntensity };
		mTerrainConstantBuffer.Data.CameraPosition = XMFLOAT4(camera->Position().x, camera->Position().y, camera->Position().z, 1.0f);
		mTerrainConstantBuffer.Data.TessellationFactor = static_cast<float>(mTessellationFactor);
		mTerrainConstantBuffer.Data.TerrainHeightScale = mTerrainTessellatedHeightScale;
		mTerrainConstantBuffer.Data.TessellationFactorDynamic = static_cast<float>(mTessellationFactorDynamic);
		mTerrainConstantBuffer.Data.UseDynamicTessellation = mUseDynamicTessellation ? 1.0f : 0.0f;
		mTerrainConstantBuffer.Data.DistanceFactor = mTessellationDistanceFactor;
		mTerrainConstantBuffer.Data.TileSize = mTileResolution * mTileScale;
		mTerrainConstantBuffer.ApplyChanges(rhi);

		for (int i = 0; i < mHeightMaps.size(); i++)
			DrawTessellated(aPass, aRenderTargets, aDepthTarget, i, worldShadowMapper, probeManager, shadowMapCascade);
	}

	void ER_Terrain::DrawDebugGizmos(ER_RHI_GPUTexture* aRenderTarget, ER_RHI_GPUTexture* aDepth, ER_RHI_GPURootSignature* rs)
	{
		if (!mEnabled || !mLoaded || !mDrawDebugAABBs)
			return;

		for (int i = 0; i < mHeightMaps.size(); i++)
			mHeightMaps[i]->mDebugGizmoAABB->Draw(aRenderTarget, aDepth, rs);
	}

	void ER_Terrain::Update(const ER_CoreTime& gameTime)
	{
		if (!mEnabled && !mLoaded)
			return;

		ER_Camera* camera = (ER_Camera*)(mCore->GetServices().FindService(ER_Camera::TypeIdClass()));

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
			ImGui::SliderInt("Tessellation factor static", &mTessellationFactor, 1, 64);
			ImGui::SliderInt("Tessellation factor dynamic", &mTessellationFactorDynamic, 1, 64);
			ImGui::Checkbox("Use dynamic tessellation", &mUseDynamicTessellation);
			ImGui::SliderFloat("Dynamic LOD distance factor", &mTessellationDistanceFactor, 0.0001f, 0.1f);
			ImGui::SliderFloat("Tessellated terrain height scale", &mTerrainTessellatedHeightScale, 0.0f, 1000.0f);
			ImGui::SliderFloat("Placement height delta", &mPlacementHeightDelta, 0.0f, 10.0f);
			ImGui::End();
		}
	}

	void ER_Terrain::DrawTessellated(TerrainRenderPass aPass, const std::vector<ER_RHI_GPUTexture*>& aRenderTargets, ER_RHI_GPUTexture* aDepthTarget, int tileIndex, ER_ShadowMapper* worldShadowMapper, ER_LightProbesManager* probeManager, int shadowMapCascade)
	{
		if (aPass == TerrainRenderPass::TERRAIN_SHADOW)
			assert(shadowMapCascade != -1);
		
		//for shadow mapping pass we dont want to cull with main camera frustum
		if (mHeightMaps[tileIndex]->IsCulled() && (aPass == TerrainRenderPass::TERRAIN_FORWARD || aPass == TerrainRenderPass::TERRAIN_GBUFFER))
			return;

		ER_RHI* rhi = mCore->GetRHI();

		ER_Camera* camera = (ER_Camera*)(mCore->GetServices().FindService(ER_Camera::TypeIdClass()));
		assert(camera);

		ER_RHI_PRIMITIVE_TYPE originalPrimitiveTopology = rhi->GetCurrentTopologyType();

		ER_RHI_GPURootSignature* rootSig = mTerrainCommonPassRS;
		std::string psoName = ER_Utility::IsWireframe ? mTerrainMainPassWireframePSOName : mTerrainMainPassPSOName;
		if (aPass == TERRAIN_SHADOW)
			psoName = mTerrainShadowPassPSOName;
		else if (aPass == TERRAIN_GBUFFER)
			psoName = ER_Utility::IsWireframe ? mTerrainGBufferPassWireframePSOName : mTerrainGBufferPassPSOName;

		rhi->SetRootSignature(rootSig);
		rhi->SetVertexBuffers({ mHeightMaps[tileIndex]->mVertexBufferTS });
		rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_CONTROL_POINT_PATCHLIST);

		if (!rhi->IsPSOReady(psoName))
		{
			rhi->InitializePSO(psoName);
			rhi->SetTopologyTypeToPSO(psoName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_CONTROL_POINT_PATCHLIST);
			rhi->SetInputLayout(mInputLayout);
			rhi->SetShader(mVS);
			rhi->SetShader(mHS);
			rhi->SetRootSignatureToPSO(psoName, rootSig);
			rhi->SetBlendState(ER_RHI_BLEND_STATE::ER_NO_BLEND);
			rhi->SetDepthStencilState(ER_RHI_DEPTH_STENCIL_STATE::ER_DEPTH_ONLY_WRITE_COMPARISON_LESS_EQUAL);
			if (aPass == TerrainRenderPass::TERRAIN_SHADOW)
			{
				rhi->SetShader(mDS_ShadowMap);
				rhi->SetShader(mPS_ShadowMap);
				rhi->SetRenderTargetFormats({}, worldShadowMapper->GetShadowTexture(shadowMapCascade));
				rhi->SetRasterizerState(ER_RHI_RASTERIZER_STATE::ER_SHADOW_RS);
			}
			else 
			{
				rhi->SetShader(mDS);
				if (aPass == TerrainRenderPass::TERRAIN_FORWARD)
					rhi->SetShader(mPS);
				else if (aPass == TerrainRenderPass::TERRAIN_GBUFFER)
					rhi->SetShader(mPS_GBuffer);

				rhi->SetRasterizerState(ER_Utility::IsWireframe ? ER_RHI_RASTERIZER_STATE::ER_WIREFRAME : ER_RHI_RASTERIZER_STATE::ER_NO_CULLING);
				rhi->SetRenderTargetFormats(aRenderTargets, aDepthTarget);
			}
				
			rhi->FinalizePSO(psoName);
		}
		rhi->SetPSO(psoName);
		
		//set shader resources
		{
			rhi->SetConstantBuffers(ER_VERTEX, { mTerrainConstantBuffer.Buffer() }, 0, rootSig, TERRAIN_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
			rhi->SetConstantBuffers(ER_TESSELLATION_HULL, { mTerrainConstantBuffer.Buffer() }, 0, rootSig, TERRAIN_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
			if (aPass == TerrainRenderPass::TERRAIN_SHADOW)
			{
				// if we only bind (b1) to domain shader (which is the only shader stage that needs that buffer), we get a null resource in (b1)
				// but if we bind (b1) to domain stage + something else, like pixel stage, (b1) is not null anymore in domain :)
				// might be a driver bug or smth broken in dx12
				rhi->SetConstantBuffers(ER_TESSELLATION_DOMAIN, { mTerrainConstantBuffer.Buffer(), mTerrainShadowBuffers[shadowMapCascade].Buffer() }, 0, rootSig, TERRAIN_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
				rhi->SetConstantBuffers(ER_PIXEL,				{ mTerrainConstantBuffer.Buffer(), mTerrainShadowBuffers[shadowMapCascade].Buffer() }, 0, rootSig, TERRAIN_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
			}
			else
			{
				rhi->SetConstantBuffers(ER_TESSELLATION_DOMAIN, { mTerrainConstantBuffer.Buffer() }, 0, rootSig, TERRAIN_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
				rhi->SetConstantBuffers(ER_PIXEL,				{ mTerrainConstantBuffer.Buffer() }, 0, rootSig, TERRAIN_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
			}

			std::vector<ER_RHI_GPUResource*> resources(19);
			resources[0] = mHeightMaps[tileIndex]->mSplatTexture;
			resources[1] = mSplatChannelTextures[0];
			resources[2] = mSplatChannelTextures[1];
			resources[3] = mSplatChannelTextures[2];
			resources[4] = mSplatChannelTextures[3];
			if (aPass == TerrainRenderPass::TERRAIN_FORWARD)
			{
				if (worldShadowMapper)
				{
					for (int c = 0; c < NUM_SHADOW_CASCADES; c++)
						resources[5 + c] = worldShadowMapper->GetShadowTexture(c);
				}

				if (probeManager && probeManager->AreGlobalProbesReady())
				{
					resources[8] = probeManager->GetGlobalDiffuseProbe()->GetCubemapTexture();
					resources[12] = probeManager->GetGlobalSpecularProbe()->GetCubemapTexture();
					resources[17] = probeManager->GetIntegrationMap();
				}
			}
			resources[18] = mHeightMaps[tileIndex]->mHeightTexture;

			rhi->SetShaderResources(ER_RHI_SHADER_TYPE::ER_TESSELLATION_DOMAIN, resources, 0, rootSig, TERRAIN_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);
			rhi->SetShaderResources(ER_RHI_SHADER_TYPE::ER_PIXEL, resources, 0, rootSig, TERRAIN_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);

			rhi->SetSamplers(ER_TESSELLATION_DOMAIN, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SAMPLER_STATE::ER_SHADOW_SS, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_CLAMP });
			rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SAMPLER_STATE::ER_SHADOW_SS, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_CLAMP });
		}
		
		rhi->Draw(NUM_TERRAIN_PATCHES_PER_TILE * NUM_TERRAIN_PATCHES_PER_TILE);
		
		rhi->UnsetPSO();

		//reset back
		rhi->SetTopologyType(originalPrimitiveTopology);
		rhi->UnbindResourcesFromShader(ER_VERTEX);
		rhi->UnbindResourcesFromShader(ER_TESSELLATION_HULL);
		rhi->UnbindResourcesFromShader(ER_TESSELLATION_DOMAIN);
		rhi->UnbindResourcesFromShader(ER_PIXEL);
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

	bool HeightMap::PerformCPUFrustumCulling(ER_Camera* camera)
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
			(position.z <= mAABB.second.z && position.z >= mAABB.first.z))
			:
			((position.x <= mAABB.second.x && position.x >= mAABB.first.x) &&
			(position.y <= mAABB.second.y && position.y >= mAABB.first.y) &&
			(position.z <= mAABB.second.z && position.z >= mAABB.first.z));

		return isColliding;
	}

	// Method for displacing points on terrain (send some points to the GPU, get transformed points from the GPU).
	// GPU does everything in a compute shader (it finds a proper terrain tile, checks for the splat channel and transforms the provided points).
	// There is also some older functionality (USE_RAYCASTING_FOR_ON_TERRAIN_PLACEMENT) if you do not want to check heightmap collisions (fast) but use raycasts to geometry instead (slow).
	// 
	// Use cases: 
	// - placing ER_RenderingObject(s) on terrain (even their instances individually)
	// - placing ER_Foliage patches on terrain (batch placement)
	void ER_Terrain::PlaceOnTerrain(ER_RHI_GPUBuffer* outputBuffer, ER_RHI_GPUBuffer* inputBuffer, XMFLOAT4* positions, int positionsCount,
		TerrainSplatChannels splatChannel, XMFLOAT4* terrainVertices, int terrainVertexCount, float customDampDelta)
	{
		assert(inputBuffer && outputBuffer);
		ER_RHI* rhi = GetCore()->GetRHI();

#if USE_RAYCASTING_FOR_ON_TERRAIN_PLACEMENT
		ER_RHI_GPUBuffer* terrainBuffer = rhi->CreateGPUBuffer("ER_RHI_GPUBuffer: Terrain raycasting placement data");
		terrainBuffer->CreateGPUBufferResource(rhi, terrainVertices, terrainVertexCount, sizeof(XMFLOAT4), false, ER_BIND_SHADER_RESOURCE, 0, ER_RESOURCE_MISC_BUFFER_STRUCTURED);
		//...
		//DeleteObject(terrainBuffer);
#endif
		rhi->SetRootSignature(mTerrainPlacementPassRS, true);		
		if (!rhi->IsPSOReady(mTerrainPlacementPassPSOName, true))
		{
			rhi->InitializePSO(mTerrainPlacementPassPSOName, true);
			rhi->SetRootSignatureToPSO(mTerrainPlacementPassPSOName, mTerrainPlacementPassRS, true);
			rhi->SetShader(mPlaceOnTerrainCS);
			rhi->FinalizePSO(mTerrainPlacementPassPSOName, true);
		}
		rhi->SetPSO(mTerrainPlacementPassPSOName, true);
		mPlaceOnTerrainConstantBuffer.Data.HeightScale = mTerrainTessellatedHeightScale;
		mPlaceOnTerrainConstantBuffer.Data.SplatChannel = splatChannel == TerrainSplatChannels::NONE ? -1.0f : static_cast<float>(splatChannel);
		mPlaceOnTerrainConstantBuffer.Data.TerrainTileCount = static_cast<int>(mNumTiles);
		mPlaceOnTerrainConstantBuffer.Data.PlacementHeightDelta = abs(customDampDelta - FLT_MAX) < std::numeric_limits<float>::epsilon() ? mPlacementHeightDelta : customDampDelta;
		mPlaceOnTerrainConstantBuffer.ApplyChanges(rhi);
		rhi->SetConstantBuffers(ER_COMPUTE, { mPlaceOnTerrainConstantBuffer.Buffer() }, 0,
			mTerrainPlacementPassRS, PLACEMENT_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, true);
		rhi->SetShaderResources(ER_COMPUTE, { mTerrainTilesDataGPU, mTerrainTilesHeightmapsArrayTexture, mTerrainTilesSplatmapsArrayTexture }, 0,
			mTerrainPlacementPassRS, PLACEMENT_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, true);
		rhi->SetUnorderedAccessResources(ER_COMPUTE, { inputBuffer }, 0,
			mTerrainPlacementPassRS, PLACEMENT_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX, true);
		rhi->SetSamplers(ER_COMPUTE, { ER_RHI_SAMPLER_STATE::ER_BILINEAR_CLAMP, ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
		rhi->Dispatch(512, 1, 1);
		rhi->UnsetPSO();
		rhi->UnbindResourcesFromShader(ER_COMPUTE);

#ifdef ER_PLATFORM_WIN64_DX11
		ReadbackPlacedPositions(outputBuffer, inputBuffer, positions, positionsCount); //direct readback in the dx11 immediate context
#endif
	}

	// Read-back (GPU to CPU) new positions from placement compute pass
	// WARNING: we are doing that in a current frame and waiting when GPU is finished => this produces a stall
	// Ideally we should do that in async (on modern APIs) or read back on CPU in the next frame
	void ER_Terrain::ReadbackPlacedPositions(ER_RHI_GPUBuffer* outputBuffer, ER_RHI_GPUBuffer* inputBuffer, XMFLOAT4* positions, int positionsCount)
	{
		ER_RHI* rhi = GetCore()->GetRHI();

		rhi->BeginCopyCommandList();
		rhi->CopyBuffer(outputBuffer, inputBuffer, 0, true);
		rhi->EndCopyCommandList();
		rhi->ExecuteCopyCommandList(); // it will wait for GPU on a copy fence in this method, too

		void* outputData = nullptr;
		rhi->BeginBufferRead(outputBuffer, &outputData);
		{
			assert(outputData);
			XMFLOAT4* newPositions = reinterpret_cast<XMFLOAT4*>(outputData);
			for (size_t i = 0; i < positionsCount; i++)
				positions[i] = newPositions[i];
		}
		rhi->EndBufferRead(outputBuffer);
	}

	HeightMap::HeightMap(int width, int height)
	{
		mData = new MapData[width * height];
		mVertexList = new Vertex[(width - 1) * (height - 1) * 6];
	}

	HeightMap::~HeightMap()
	{		
		DeleteObject(mVertexBufferTS);
		DeleteObject(mVertexBufferNonTS);
		DeleteObject(mIndexBufferNonTS);
		DeleteObject(mSplatTexture);
		DeleteObject(mHeightTexture);
		DeleteObjects(mVertexList);
		DeleteObjects(mData);
		DeleteObject(mDebugGizmoAABB);
	}
}