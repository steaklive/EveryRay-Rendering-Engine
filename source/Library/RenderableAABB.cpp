#include "stdafx.h"

#include "RenderableAABB.h"
#include "Game.h"
#include "GameException.h"
#include "ER_BasicColorMaterial.h"
#include "VertexDeclarations.h"
#include "ColorHelper.h"
#include "MatrixHelper.h"
#include "VectorHelper.h"
#include "Camera.h"
#include "Utility.h"

namespace Library
{
	RTTI_DEFINITIONS(RenderableAABB)

	const XMVECTORF32 RenderableAABB::DefaultColor = ColorHelper::Blue;

	const UINT RenderableAABB::AABBVertexCount = 8;
	const UINT RenderableAABB::AABBPrimitiveCount = 12;
	const UINT RenderableAABB::AABBIndicesPerPrimitive = 2;
	const UINT RenderableAABB::AABBIndexCount = AABBPrimitiveCount * AABBIndicesPerPrimitive;
	const USHORT RenderableAABB::AABBIndices[] = {
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
	RenderableAABB::RenderableAABB(Game& game, Camera& camera)
		: DrawableGameComponent(game, camera),
		mVertexBuffer(nullptr), mIndexBuffer(nullptr), mMaterial(nullptr),
		mColor(DefaultColor), mPosition(Vector3Helper::Zero), mDirection(Vector3Helper::Forward), mUp(Vector3Helper::Up), mRight(Vector3Helper::Right),
		mWorldMatrix(MatrixHelper::Identity), mVertices(0), mScale(XMFLOAT3(1,1,1)), mTransformMatrix(XMMatrixIdentity())
	{
	}

	RenderableAABB::RenderableAABB(Game& game, Camera& camera, const XMFLOAT4& color)
		: DrawableGameComponent(game, camera),
		mVertexBuffer(nullptr), mIndexBuffer(nullptr), mMaterial(nullptr),
		mColor(color), mPosition(Vector3Helper::Zero), mDirection(Vector3Helper::Forward), mUp(Vector3Helper::Up), mRight(Vector3Helper::Right),
		mWorldMatrix(MatrixHelper::Identity), mVertices(0), mScale(XMFLOAT3(1, 1, 1)), mTransformMatrix(XMMatrixIdentity())
	{
	}

	RenderableAABB::~RenderableAABB()
	{
		ReleaseObject(mIndexBuffer);
		ReleaseObject(mVertexBuffer);
		DeleteObject(mMaterial);
	}

	void RenderableAABB::SetPosition(const XMFLOAT3& position)
	{
		mPosition = position;
	}

	void RenderableAABB::SetRotationMatrix(const XMMATRIX& rotationMat)
	{
		mTransformMatrix = rotationMat;
	}

	void RenderableAABB::SetAABB(const std::vector<XMFLOAT3>& aabb)
	{
		mAABB = std::make_pair(aabb[0], aabb[1]);
	}

	void RenderableAABB::InitializeGeometry(const std::vector<XMFLOAT3>& aabb, XMMATRIX matrix)
	{
		mAABB = std::make_pair(aabb[0], aabb[1]);

		XMVECTOR scaleVector;
		XMVECTOR rotQuatVector;
		XMVECTOR translationVector;

		XMFLOAT3 scale;
		XMFLOAT3 translation;

		XMMatrixDecompose(&scaleVector, &rotQuatVector, &translationVector, matrix);
		
		XMStoreFloat3(&scale, scaleVector);
		XMStoreFloat3(&translation, translationVector);

		//std::vector<XMFLOAT3> vertices;
		mVertices.push_back(XMFLOAT3(aabb[0].x * scale.x, aabb[1].y  * scale.y, aabb[0].z  * scale.z));
		mVertices.push_back(XMFLOAT3(aabb[1].x * scale.x, aabb[1].y  * scale.y, aabb[0].z  * scale.z));
		mVertices.push_back(XMFLOAT3(aabb[1].x * scale.x, aabb[0].y  * scale.y, aabb[0].z  * scale.z));
		mVertices.push_back(XMFLOAT3(aabb[0].x * scale.x, aabb[0].y  * scale.y, aabb[0].z  * scale.z));
		mVertices.push_back(XMFLOAT3(aabb[0].x * scale.x, aabb[1].y  * scale.y, aabb[1].z  * scale.z));
		mVertices.push_back(XMFLOAT3(aabb[1].x * scale.x, aabb[1].y  * scale.y, aabb[1].z  * scale.z));
		mVertices.push_back(XMFLOAT3(aabb[1].x * scale.x, aabb[0].y  * scale.y, aabb[1].z  * scale.z));
		mVertices.push_back(XMFLOAT3(aabb[0].x * scale.x, aabb[0].y  * scale.y, aabb[1].z  * scale.z));

		InitializeVertexBuffer(mVertices);
	}

	void RenderableAABB::Initialize()
	{
		mMaterial = new ER_BasicColorMaterial(*mGame, {}, HAS_VERTEX_SHADER | HAS_PIXEL_SHADER);
		InitializeIndexBuffer();
	}

	void RenderableAABB::Update()
	{
		XMMATRIX worldMatrix = XMMatrixIdentity();
		MatrixHelper::SetForward(worldMatrix, mDirection);
		MatrixHelper::SetUp(worldMatrix, mUp);
		MatrixHelper::SetRight(worldMatrix, mRight);
		XMMATRIX scale = XMMatrixScaling(mScale.x,mScale.y,mScale.z);
		worldMatrix *= scale;
		worldMatrix *= mTransformMatrix;
		MatrixHelper::SetTranslation(worldMatrix, mPosition);

		XMStoreFloat4x4(&mWorldMatrix, worldMatrix);

		//mAABB.first = XMVector3Transform(XMLoadFloat3(&mAABB.first), worldMatrix);
		//mAABB.second = XMVector3Transform(XMLoadFloat3(&mAABB.second), worldMatrix);
		//ResizeAABB();
	}

	void RenderableAABB::Draw()
	{
		ID3D11DeviceContext* context = mGame->Direct3DDeviceContext();
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

		UINT stride = mMaterial->VertexSize();
		UINT offset = 0;
		context->IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &offset);
		context->IASetIndexBuffer(mIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

		mMaterial->PrepareForRendering(XMLoadFloat4x4(&mWorldMatrix), mColor);

		context->DrawIndexed(AABBIndexCount, 0, 0);
	}
	void RenderableAABB::SetColor(const XMFLOAT4& color)
	{
		mColor = color;
	}

	//TODO refactor duplication
	void RenderableAABB::UpdateColor(const XMFLOAT4& color)
	{
		mColor = color;
	}

	void RenderableAABB::ResizeAABB()
	{
		if (mVertices.size() != 8)
			return;

		// update the vertices of AABB (due to tranformation we had to recalculate AABB)
		mVertices.at(0) = (XMFLOAT3(mAABB.first.x, mAABB.second.y, mAABB.first.z));
		mVertices.at(1) = (XMFLOAT3(mAABB.second.x, mAABB.second.y, mAABB.first.z));
		mVertices.at(2) = (XMFLOAT3(mAABB.second.x, mAABB.first.y, mAABB.first.z));
		mVertices.at(3) = (XMFLOAT3(mAABB.first.x, mAABB.first.y, mAABB.first.z));
		mVertices.at(4) = (XMFLOAT3(mAABB.first.x, mAABB.second.y, mAABB.second.z));
		mVertices.at(5) = (XMFLOAT3(mAABB.second.x, mAABB.second.y, mAABB.second.z));
		mVertices.at(6) = (XMFLOAT3(mAABB.second.x, mAABB.first.y, mAABB.second.z));
		mVertices.at(7) = (XMFLOAT3(mAABB.first.x, mAABB.first.y, mAABB.second.z));
	}


	void RenderableAABB::InitializeVertexBuffer(const std::vector<XMFLOAT3>& aabb)
	{
		ReleaseObject(mVertexBuffer);

		VertexPosition vertices[AABBVertexCount];
		for (UINT i = 0; i < AABBVertexCount; i++)
			vertices[i].Position = XMFLOAT4(aabb[i].x, aabb[i].y, aabb[i].z, 1.0f);

		D3D11_BUFFER_DESC vertexBufferDesc;
		ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
		vertexBufferDesc.ByteWidth = sizeof(VertexPosition) * AABBVertexCount;
		vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA vertexSubResourceData;
		ZeroMemory(&vertexSubResourceData, sizeof(vertexSubResourceData));
		vertexSubResourceData.pSysMem = vertices;
		if (FAILED(mGame->Direct3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexSubResourceData, &mVertexBuffer)))
		{
			throw GameException("ID3D11Device::CreateBuffer() failed.");
		}
	}

	void RenderableAABB::InitializeIndexBuffer()
	{
		D3D11_BUFFER_DESC indexBufferDesc;
		ZeroMemory(&indexBufferDesc, sizeof(indexBufferDesc));
		indexBufferDesc.ByteWidth = sizeof(USHORT) * AABBIndexCount;
		indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

		D3D11_SUBRESOURCE_DATA indexSubResourceData;
		ZeroMemory(&indexSubResourceData, sizeof(indexSubResourceData));
		indexSubResourceData.pSysMem = AABBIndices;
		if (FAILED(mGame->Direct3DDevice()->CreateBuffer(&indexBufferDesc, &indexSubResourceData, &mIndexBuffer)))
		{
			throw GameException("ID3D11Device::CreateBuffer() failed.");
		}
	}
}