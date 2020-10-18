#pragma once
#include "Common.h"
#include "GameComponent.h"
#include "Camera.h"
#include "DirectionalLight.h"
#include "PostProcessingStack.h"

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
			float u, v;
			float uTile, vTile;
		};
	public:

		HeightMap(int width, int height);
		~HeightMap();

		ID3D11Buffer* mVertexBuffer = nullptr;
		ID3D11Buffer* mVertexBufferTS = nullptr;
		ID3D11Buffer* mIndexBuffer = nullptr;
		ID3D11ShaderResourceView* mSplatTexture = nullptr;
		int mVertexCount = 0;
		int mIndexCount = 0;
		XMMATRIX mWorldMatrix = XMMatrixIdentity();

		MapData* mData = nullptr;
	};

	class Terrain : public GameComponent
	{
	public:
		Terrain(std::string path, Game& game, Camera& camera, DirectionalLight& light, Rendering::PostProcessingStack& pp, bool isWireframe);
		~Terrain();

		UINT GetWidth() { return mWidth; }
		UINT GetHeight() { return mHeight; }

		void Draw();
		void Draw(int tileIndex);
		void Update();

		void SetWireframeMode(bool flag) { mIsWireframe = flag; }
		void SetTessellationMode(bool flag) { mUseTessellation = flag; }

	private:
		Camera& mCamera;

		void LoadTextures(std::string path);
		void LoadTileGroup(int threadIndex, std::string path);
		void GenerateTileMesh(int tileIndex);
		void LoadRawHeightmapTile(int tileIndexX, int tileIndexY, std::string path);
		void LoadSplatmap(int tileIndexX, int tileIndexY, std::string path);

		TerrainMaterial* mMaterial;

		DirectionalLight& mDirectionalLight;

		Rendering::PostProcessingStack& mPPStack;

		UINT mWidth = /*1024*/ 512;
		UINT mHeight = /*1024*/ 512;
		
		bool mIsWireframe = false;

		std::vector<HeightMap*> mHeightMaps;
		int mNumTiles = 16;
		float mHeightScale = 200.0f;

		bool mUseTessellation = true;

		ID3D11ShaderResourceView* mGrassTexture = nullptr;
		ID3D11ShaderResourceView* mGroundTexture = nullptr;
		ID3D11ShaderResourceView* mRockTexture = nullptr;
		ID3D11ShaderResourceView* mMudTexture = nullptr;

		ID3D11Buffer* mVertexPatchBuffer = nullptr;
	};
}