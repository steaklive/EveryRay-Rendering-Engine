#include "stdafx.h"

#include "RenderableAABB.h"
#include "Game.h"
#include "GameException.h"
#include "BasicMaterial.h"
#include "VertexDeclarations.h"
#include "ColorHelper.h"
#include "MatrixHelper.h"
#include "VectorHelper.h"
#include "Camera.h"
#include "VertexDeclarations.h"
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
		mVertexBuffer(nullptr), mIndexBuffer(nullptr), mMaterial(nullptr), mPass(nullptr), mInputLayout(nullptr),
		mColor(DefaultColor), mPosition(Vector3Helper::Zero), mDirection(Vector3Helper::Forward), mUp(Vector3Helper::Up), mRight(Vector3Helper::Right),
		mWorldMatrix(MatrixHelper::Identity), mVertices(0), mScale(XMFLOAT3(1,1,1)), mTransformMatrix(XMMatrixIdentity())
	{
	}

	RenderableAABB::RenderableAABB(Game& game, Camera& camera, const XMFLOAT4& color)
		: DrawableGameComponent(game, camera),
		mVertexBuffer(nullptr), mIndexBuffer(nullptr), mMaterial(nullptr), mPass(nullptr), mInputLayout(nullptr),
		mColor(color), mPosition(Vector3Helper::Zero), mDirection(Vector3Helper::Forward), mUp(Vector3Helper::Up), mRight(Vector3Helper::Right),
		mWorldMatrix(MatrixHelper::Identity), mVertices(0), mScale(XMFLOAT3(1, 1, 1)), mTransformMatrix(XMMatrixIdentity())
	{
	}

	RenderableAABB::~RenderableAABB()
	{
		ReleaseObject(mIndexBuffer);
		ReleaseObject(mVertexBuffer);

		if (mMaterial != nullptr)
		{
			delete mMaterial->GetEffect();
			delete mMaterial;
			mMaterial = nullptr;
		}
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
		mAABB = &aabb;
	}

	void RenderableAABB::InitializeGeometry(const std::vector<XMFLOAT3>& aabb, XMMATRIX matrix)
	{
		mAABB = &aabb;

		mModifiedAABB = aabb;
		mModifiedAABB2 = aabb;

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
		Effect* effect = new Effect(*mGame);
		effect->CompileFromFile(Utility::GetFilePath(L"content\\effects\\BasicEffect.fx"));

		mMaterial = new BasicMaterial();
		mMaterial->Initialize(effect);

		mPass = mMaterial->CurrentTechnique()->Passes().at(0);
		mInputLayout = mMaterial->InputLayouts().at(mPass);

		InitializeIndexBuffer();
	}

	void RenderableAABB::Update(const GameTime& gameTime)
	{
		ResizeAABB();

		XMMATRIX worldMatrix = XMMatrixIdentity();
		MatrixHelper::SetForward(worldMatrix, mDirection);
		MatrixHelper::SetUp(worldMatrix, mUp);
		MatrixHelper::SetRight(worldMatrix, mRight);
		XMMATRIX scale = XMMatrixScaling(mScale.x,mScale.y,mScale.z);
		worldMatrix *= scale;
		worldMatrix *= mTransformMatrix;
		MatrixHelper::SetTranslation(worldMatrix, mPosition);

		XMStoreFloat4x4(&mWorldMatrix, worldMatrix);

		//XMVECTOR minAABB = XMLoadFloat3(&mModifiedAABB.at(0));
		//XMVECTOR maxAABB = XMLoadFloat3(&mModifiedAABB.at(1));

		if (mAABB->size() == 2)
		{
			XMVECTOR minAABB = XMVector3Transform(XMLoadFloat3(&mAABB->at(0)), worldMatrix);
			XMVECTOR maxAABB = XMVector3Transform(XMLoadFloat3(&mAABB->at(1)), worldMatrix);

			XMStoreFloat3(&mModifiedAABB.at(0), minAABB);
			XMStoreFloat3(&mModifiedAABB.at(1), maxAABB);
		}

	}

	void RenderableAABB::Draw()
	{
		assert(mPass != nullptr);
		assert(mInputLayout != nullptr);

		ID3D11DeviceContext* direct3DDeviceContext = mGame->Direct3DDeviceContext();
		direct3DDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		direct3DDeviceContext->IASetInputLayout(mInputLayout);

		UINT stride = sizeof(VertexPositionColor);
		UINT offset = 0;
		direct3DDeviceContext->IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &offset);
		direct3DDeviceContext->IASetIndexBuffer(mIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

		XMMATRIX world = XMLoadFloat4x4(&mWorldMatrix);
		XMMATRIX wvp = world * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();
		mMaterial->WorldViewProjection() << wvp;

		mPass->Apply(0, direct3DDeviceContext);

		direct3DDeviceContext->DrawIndexed(AABBIndexCount, 0, 0);
	}
	void RenderableAABB::SetColor(XMFLOAT4 color)
	{
		mColor = color;
	}

	void RenderableAABB::UpdateColor(XMFLOAT4 color)
	{

		VertexPositionColor vertices[AABBVertexCount];

		for (UINT i = 0; i < AABBVertexCount; i++)
		{
			vertices[i].Position = XMFLOAT4(mVertices[i].x, mVertices[i].y, mVertices[i].z, 1.0f);
			vertices[i].Color = color;
		}

		D3D11_MAPPED_SUBRESOURCE mappedResource;
		ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
		mGame->Direct3DDeviceContext()->Map(mVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		memcpy(mappedResource.pData, vertices, sizeof(VertexPositionColor) * AABBVertexCount);
		mGame->Direct3DDeviceContext()->Unmap(mVertexBuffer, 0);

	}

	void RenderableAABB::ResizeAABB()
	{

		if (mVertices.size() != 8 || mAABB->size()!=2) return;

		// update the vertices of AABB (due to tranformation we had to recalculate AABB)
		mVertices.at(0) = (XMFLOAT3(mAABB->at(0).x, mAABB->at(1).y, mAABB->at(0).z));
		mVertices.at(1) = (XMFLOAT3(mAABB->at(1).x, mAABB->at(1).y, mAABB->at(0).z));
		mVertices.at(2) = (XMFLOAT3(mAABB->at(1).x, mAABB->at(0).y, mAABB->at(0).z));
		mVertices.at(3) = (XMFLOAT3(mAABB->at(0).x, mAABB->at(0).y, mAABB->at(0).z));
		mVertices.at(4) = (XMFLOAT3(mAABB->at(0).x, mAABB->at(1).y, mAABB->at(1).z));
		mVertices.at(5) = (XMFLOAT3(mAABB->at(1).x, mAABB->at(1).y, mAABB->at(1).z));
		mVertices.at(6) = (XMFLOAT3(mAABB->at(1).x, mAABB->at(0).y, mAABB->at(1).z));
		mVertices.at(7) = (XMFLOAT3(mAABB->at(0).x, mAABB->at(0).y, mAABB->at(1).z));

	}


	void RenderableAABB::InitializeVertexBuffer(const std::vector<XMFLOAT3>& aabb)
	{
		ReleaseObject(mVertexBuffer);

		VertexPositionColor vertices[AABBVertexCount];
		//const XMFLOAT3* corners = frustum.Corners();
		for (UINT i = 0; i < AABBVertexCount; i++)
		{
			vertices[i].Position = XMFLOAT4(aabb[i].x, aabb[i].y, aabb[i].z, 1.0f);
			vertices[i].Color = mColor;
		}

		D3D11_BUFFER_DESC vertexBufferDesc;
		ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
		vertexBufferDesc.ByteWidth = sizeof(VertexPositionColor) * AABBVertexCount;
		vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
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