#include "stdafx.h"

#include "ER_RenderableAABB.h"
#include "Game.h"
#include "ER_CoreException.h"
#include "ER_BasicColorMaterial.h"
#include "ER_VertexDeclarations.h"
#include "ER_ColorHelper.h"
#include "ER_MatrixHelper.h"
#include "ER_VectorHelper.h"
#include "ER_Camera.h"
#include "ER_Utility.h"
#include "ER_GPUBuffer.h"

namespace Library
{
	const XMVECTORF32 DefaultColor = ER_ColorHelper::Blue;
	const UINT AABBVertexCount = 8;
	const UINT AABBPrimitiveCount = 12;
	const UINT AABBIndicesPerPrimitive = 2;
	const UINT AABBIndexCount = AABBPrimitiveCount * AABBIndicesPerPrimitive;
	
	USHORT AABBIndices[] = {
		// Near plane lines
		0, 1,
		1, 2,
		2, 3,
		3, 0,

		// Sides
		0, 4,
		1, 5,
		2, 6,
		3, 7,

		// Far plane lines
		4, 5,
		5, 6,
		6, 7,
		7, 4
	};

	ER_RenderableAABB::ER_RenderableAABB(Game& game, const XMFLOAT4& color)
		: mGame(game),
		mVertexBuffer(nullptr), mIndexBuffer(nullptr), mMaterial(nullptr),
		mColor(color)
	{
		mMaterial = new ER_BasicColorMaterial(mGame, {}, HAS_VERTEX_SHADER | HAS_PIXEL_SHADER);

		auto device = mGame.Direct3DDevice();
		mIndexBuffer = new ER_GPUBuffer(device, AABBIndices, AABBIndexCount, sizeof(USHORT), D3D11_USAGE_IMMUTABLE, D3D11_BIND_INDEX_BUFFER);
	}

	ER_RenderableAABB::~ER_RenderableAABB()
	{
		DeleteObject(mVertexBuffer);
		DeleteObject(mIndexBuffer);
		DeleteObject(mMaterial);
	}

	void ER_RenderableAABB::SetAABB(const std::vector<XMFLOAT3>& aabb)
	{
		mAABB = std::make_pair(aabb[0], aabb[1]);
	}

	void ER_RenderableAABB::InitializeGeometry(const std::vector<XMFLOAT3>& aabb)
	{
		mAABB = std::make_pair(aabb[0], aabb[1]);

		mVertices[0] = XMFLOAT4(aabb[0].x, aabb[1].y, aabb[0].z, 1.0f);
		mVertices[1] = XMFLOAT4(aabb[1].x, aabb[1].y, aabb[0].z, 1.0f);
		mVertices[2] = XMFLOAT4(aabb[1].x, aabb[0].y, aabb[0].z, 1.0f);
		mVertices[3] = XMFLOAT4(aabb[0].x, aabb[0].y, aabb[0].z, 1.0f);
		mVertices[4] = XMFLOAT4(aabb[0].x, aabb[1].y, aabb[1].z, 1.0f);
		mVertices[5] = XMFLOAT4(aabb[1].x, aabb[1].y, aabb[1].z, 1.0f);
		mVertices[6] = XMFLOAT4(aabb[1].x, aabb[0].y, aabb[1].z, 1.0f);
		mVertices[7] = XMFLOAT4(aabb[0].x, aabb[0].y, aabb[1].z, 1.0f);

		auto device = mGame.Direct3DDevice();
		mVertexBuffer = new ER_GPUBuffer(device, mVertices, AABBVertexCount, sizeof(VertexPosition), D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE);
	}

	void ER_RenderableAABB::Update(ER_AABB& aabb)
	{
		mAABB = aabb;
		UpdateVertices();
	}

	void ER_RenderableAABB::Draw()
	{
		ID3D11DeviceContext* context = mGame.Direct3DDeviceContext();
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

		UINT stride = mMaterial->VertexSize();
		UINT offset = 0;
		auto vertexBuffer = mVertexBuffer->GetBuffer();
		context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
		context->IASetIndexBuffer(mIndexBuffer->GetBuffer(), DXGI_FORMAT_R16_UINT, 0);

		mMaterial->PrepareForRendering(XMMatrixIdentity(), mColor);
		context->DrawIndexed(AABBIndexCount, 0, 0);
	}

	void ER_RenderableAABB::SetColor(const XMFLOAT4& color)
	{
		mColor = color;
	}

	void ER_RenderableAABB::UpdateVertices()
	{
		mVertices[0] = XMFLOAT4(mAABB.first.x, mAABB.second.y, mAABB.first.z, 1.0f);
		mVertices[1] = XMFLOAT4(mAABB.second.x, mAABB.second.y, mAABB.first.z, 1.0f);
		mVertices[2] = XMFLOAT4(mAABB.second.x, mAABB.first.y, mAABB.first.z, 1.0f);
		mVertices[3] = XMFLOAT4(mAABB.first.x, mAABB.first.y, mAABB.first.z, 1.0f);
		mVertices[4] = XMFLOAT4(mAABB.first.x, mAABB.second.y, mAABB.second.z, 1.0f);
		mVertices[5] = XMFLOAT4(mAABB.second.x, mAABB.second.y, mAABB.second.z, 1.0f);
		mVertices[6] = XMFLOAT4(mAABB.second.x, mAABB.first.y, mAABB.second.z, 1.0f);
		mVertices[7] = XMFLOAT4(mAABB.first.x, mAABB.first.y, mAABB.second.z, 1.0f);

		auto context = mGame.Direct3DDeviceContext();
		mVertexBuffer->Update(context, mVertices, AABBVertexCount * sizeof(VertexPosition), D3D11_MAP_WRITE_DISCARD);
	}
}