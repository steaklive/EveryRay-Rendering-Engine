#pragma once
#include "Common.h"
#include "GameComponent.h"
#include "Camera.h"

namespace Library 
{
	class TerrainMaterial;

	struct NormalVector
	{
		float x, y, z;
	};

	class HeightMap
	{
		struct MapData
		{
			float x, y, z;
			float normalX, normalY, normalZ;
		};
	public:

		HeightMap(int width, int height);
		~HeightMap();

		ID3D11Buffer* mVertexBuffer;
		ID3D11Buffer* mIndexBuffer;
		int mVertexCount = 0;
		int mIndexCount = 0;
		XMMATRIX mWorldMatrix = XMMatrixIdentity();

		MapData* mData;
	};

	class Terrain : public GameComponent
	{
	public:
		Terrain(std::string path, Game& game, Camera& camera, bool isWireframe);
		~Terrain();

		UINT GetWidth() { return mWidth; }
		UINT GetHeight() { return mHeight; }

		void Draw();
		void Draw(int tileIndex);
		void Update();

	private:
		Camera& mCamera;

		void LoadTileGroup(int threadIndex, std::string path);
		void GenerateTileMesh(int tileIndex);
		void LoadRawHeightmapTile(int tileIndexX, int tileIndexY, std::string path);

		TerrainMaterial* mMaterial;

		UINT mWidth = /*1024*/ 512;
		UINT mHeight = /*1024*/ 512;
		
		bool mIsWireframe = false;

		std::vector<HeightMap*> mHeightMaps;
		int mNumTiles = 16;
		float mHeightScale = 200.0f;

	};
}