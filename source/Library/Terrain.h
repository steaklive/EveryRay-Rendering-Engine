#pragma once
#include "Common.h"
#include "GameComponent.h"
#include "Camera.h"

namespace Library 
{
	class BasicMaterial;

	struct HeightMap
	{
		float x, y, z;
	};

	class Terrain : public GameComponent
	{
	public:
		Terrain(std::string path, Game& game, Camera& camera, float cellScale, bool isWireframe);
		~Terrain();

		UINT GetWidth() { return mWidth; }
		UINT GetHeight() { return mHeight; }
		float GetCellScale() { return mCellScale; }

		void Draw();
		void Update();

	private:
		Camera& mCamera;

		void GenerateMesh();
		void InitializeGrid();
		void LoadTextures(std::string path);

		ID3D11Buffer* mVertexBuffer;
		ID3D11Buffer* mIndexBuffer;
		int mVertexCount = 0;
		int mIndexCount = 0;

		BasicMaterial* mMaterial;

		//UINT mSize = 0;

		UINT mWidth = 0;
		UINT mHeight = 0;
		
		float mCellScale = 0.0f;
		XMMATRIX mWorldMatrix;

		bool mIsWireframe = false;

		HeightMap* mHeightMap = nullptr;
	};
}