#include "Foliage.h"
#include "GameException.h"
#include "Game.h"
#include "MatrixHelper.h"
#include "Utility.h"
#include "VertexDeclarations.h"
#include "RasterizerStates.h"

namespace Library
{
	Foliage::Foliage(Game& pGame, Camera& pCamera, int pPatchesCount)
		:
		GameComponent(pGame),
		mCamera(pCamera),
		mPatchesCount(pPatchesCount)
	{
		Initialize();
	}

	Foliage::~Foliage()
	{
		ReleaseObject(mVertexBuffer);
		ReleaseObject(mInstanceBuffer);
		ReleaseObject(mIndexBuffer);
		ReleaseObject(mAlbedoTexture);
		DeleteObject(mPatchesBufferCPU);
		DeleteObject(mPatchesBufferGPU);
	}

	void Foliage::Initialize()
	{
		InitializeBuffersCPU();
		InitializeBuffersGPU();
	}

	void Foliage::InitializeBuffersGPU()
	{
		D3D11_BUFFER_DESC vertexBufferDesc, instanceBufferDesc;
		D3D11_SUBRESOURCE_DATA vertexData, instanceData;

		int vertexCount = 6;
		FoliagePatchData* patchData = new FoliagePatchData[vertexCount];

		// Load the vertex array with data.
		patchData[0].pos = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);  // Bottom left.
		patchData[0].uv = XMFLOAT2(0.0f, 1.0f);

		patchData[1].pos = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);  // Top left.
		patchData[1].uv = XMFLOAT2(0.0f, 0.0f);

		patchData[2].pos = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);  // Bottom right.
		patchData[2].uv = XMFLOAT2(1.0f, 1.0f);

		patchData[3].pos = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);  // Bottom right.
		patchData[3].uv = XMFLOAT2(1.0f, 1.0f);

		patchData[4].pos = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);  // Top left.
		patchData[4].uv = XMFLOAT2(0.0f, 0.0f);

		patchData[5].pos = XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f);  // Top right.
		patchData[5].uv = XMFLOAT2(1.0f, 0.0f);

		// Set up the description of the vertex buffer.
		vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		vertexBufferDesc.ByteWidth = sizeof(FoliagePatchData) * vertexCount;
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertexBufferDesc.CPUAccessFlags = 0;
		vertexBufferDesc.MiscFlags = 0;
		vertexBufferDesc.StructureByteStride = 0;

		// Give the subresource structure a pointer to the vertex data.
		vertexData.pSysMem = patchData;
		vertexData.SysMemPitch = 0;
		vertexData.SysMemSlicePitch = 0;

		if (FAILED(mGame->Direct3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexData, &mVertexBuffer)))
			throw GameException("ID3D11Device::CreateBuffer() failed while generating vertex buffer of foliage mesh patch");

		delete patchData;
		patchData = nullptr;

		// instance buffer
		int instanceCount = mPatchesCount;
		mPatchesBufferGPU = new FoliageInstanceData[instanceCount];

		for (int i = 0; i < instanceCount; i++)
		{
			mPatchesBufferGPU[i].worldMatrix = XMMatrixTranslation(mPatchesBufferCPU[i].x, mPatchesBufferCPU[i].y, mPatchesBufferCPU[i].z);
			mPatchesBufferGPU[i].color = XMFLOAT3(mPatchesBufferCPU[i].r, mPatchesBufferCPU[i].g, mPatchesBufferCPU[i].b);
		}

		// Set up the description of the instance buffer.
		instanceBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		instanceBufferDesc.ByteWidth = sizeof(FoliageInstanceData) * instanceCount;
		instanceBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		instanceBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		instanceBufferDesc.MiscFlags = 0;
		instanceBufferDesc.StructureByteStride = 0;

		// Give the subresource structure a pointer to the instance data.
		instanceData.pSysMem = mPatchesBufferGPU;
		instanceData.SysMemPitch = 0;
		instanceData.SysMemSlicePitch = 0;

		if (FAILED(mGame->Direct3DDevice()->CreateBuffer(&instanceBufferDesc, &instanceData, &mInstanceBuffer)))
			throw GameException("ID3D11Device::CreateBuffer() failed while generating instance buffer of foliage patches");
	}

	void Foliage::InitializeBuffersCPU()
	{
		// randomly generate positions and color
		mPatchesBufferCPU = new FoliageData[mPatchesCount];

		for (int i = 0; i < mPatchesCount; i++)
		{
			mPatchesBufferCPU[i].x = ((float)rand() / (float)(RAND_MAX)) * 9.0f - 4.5f;
			mPatchesBufferCPU[i].y = 0.0f;
			mPatchesBufferCPU[i].z = ((float)rand() / (float)(RAND_MAX)) * 9.0f - 4.5f;

			mPatchesBufferCPU[i].r = ((float)rand() / (float)(RAND_MAX)) * 1.0f + 1.0f;
			mPatchesBufferCPU[i].g = ((float)rand() / (float)(RAND_MAX)) * 1.0f + 0.5f;
			mPatchesBufferCPU[i].b = 0.0f;
		}
	}

	void Foliage::Draw()
	{
		ID3D11DeviceContext* context = GetGame()->Direct3DDeviceContext();

		// dont forget to set alpha blend state if not using the one declared in .fx
		//context->OMSetBlendState(alphaBlendState, blendFactor, 0xffffffff);

		unsigned int strides[2];
		unsigned int offsets[2];
		ID3D11Buffer* bufferPointers[2];

		strides[0] = sizeof(FoliagePatchData);
		strides[1] = sizeof(FoliageInstanceData);

		offsets[0] = 0;
		offsets[1] = 0;

		// Set the array of pointers to the vertex and instance buffers.
		bufferPointers[0] = mVertexBuffer;
		bufferPointers[1] = mInstanceBuffer;

		context->IASetVertexBuffers(0, 2, bufferPointers, strides, offsets);
		context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// material stuff

		if (mIsWireframe)
		{
			context->RSSetState(RasterizerStates::Wireframe);
			context->DrawInstanced(6, mPatchesCount, 0, 0);
			context->RSSetState(nullptr);
		}
		else
			context->DrawInstanced(6, mPatchesCount, 0, 0);

	}
}