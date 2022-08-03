#include "stdafx.h"

#include "ER_RenderableAABB.h"
#include "ER_Core.h"
#include "ER_CoreException.h"
#include "ER_BasicColorMaterial.h"
#include "ER_VertexDeclarations.h"
#include "ER_ColorHelper.h"
#include "ER_MatrixHelper.h"
#include "ER_VectorHelper.h"
#include "ER_Camera.h"
#include "ER_Utility.h"

namespace EveryRay_Core
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

	ER_RenderableAABB::ER_RenderableAABB(ER_Core& game, const XMFLOAT4& color)
		: mCore(game),
		mVertexBuffer(nullptr), mIndexBuffer(nullptr), mMaterial(nullptr),
		mColor(color)
	{
		mMaterial = new ER_BasicColorMaterial(mCore, {}, HAS_VERTEX_SHADER | HAS_PIXEL_SHADER);

		auto rhi = mCore.GetRHI();
		mIndexBuffer = rhi->CreateGPUBuffer();
		mIndexBuffer->CreateGPUBufferResource(rhi, AABBIndices, AABBIndexCount, sizeof(USHORT), false, ER_BIND_INDEX_BUFFER, 0, ER_RESOURCE_MISC_NONE, ER_FORMAT_R16_UINT);
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

		auto rhi = mCore.GetRHI();
		mVertexBuffer = rhi->CreateGPUBuffer();
		mVertexBuffer->CreateGPUBufferResource(rhi, mVertices, AABBVertexCount, sizeof(VertexPosition), true, ER_BIND_VERTEX_BUFFER);
	}

	void ER_RenderableAABB::Update(ER_AABB& aabb)
	{
		mAABB = aabb;
		UpdateVertices();
	}

	void ER_RenderableAABB::Draw()
	{
		ER_RHI* rhi = mCore.GetRHI();
		rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_LINELIST);
		rhi->SetVertexBuffers({ mVertexBuffer });
		rhi->SetIndexBuffer(mIndexBuffer);
		mMaterial->PrepareForRendering(XMMatrixIdentity(), mColor);
		rhi->DrawIndexed(AABBIndexCount);
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

		auto rhi = mCore.GetRHI();
		rhi->UpdateBuffer(mVertexBuffer, mVertices, AABBVertexCount * sizeof(VertexPosition));
	}
}