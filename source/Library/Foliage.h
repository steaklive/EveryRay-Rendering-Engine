#pragma once
#include "Common.h"
#include "GameComponent.h"
#include "Camera.h"
#include "DirectionalLight.h"
#include "PostProcessingStack.h"

namespace Library
{
	struct FoliagePatchData //for GPU vertex buffer
	{
		XMFLOAT4 pos;
		XMFLOAT2 uv;
	};

	struct FoliageInstanceData //for GPU instance buffer
	{
		XMMATRIX worldMatrix = XMMatrixIdentity();
		XMFLOAT3 color;
	};

	struct FoliageData //for CPU buffer
	{
		float x, y, z;
		float r, g, b;
	};

	class Foliage : public GameComponent
	{
	public:
		Foliage(Game& pGame, Camera& pCamera,int pPatchesCount);
		~Foliage();

		void Initialize();
		void InitializeBuffersGPU();
		void InitializeBuffersCPU();
		void Draw();
		void Update(const GameTime& gameTime);

		int GetPatchesCount() { return mPatchesCount; }
		void SetWireframe(bool flag) { mIsWireframe = flag; }

	private:
		Camera& mCamera;

		//FoliageMaterial* mMaterial;

		ID3D11Buffer* mVertexBuffer = nullptr;
		ID3D11Buffer* mIndexBuffer = nullptr;
		ID3D11Buffer* mInstanceBuffer = nullptr;
		ID3D11ShaderResourceView* mAlbedoTexture = nullptr;

		FoliageInstanceData* mPatchesBufferGPU;
		FoliageData* mPatchesBufferCPU;

		int mPatchesCount;
		bool mIsWireframe;
	};
}