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
		mWorldMatrix(MatrixHelper::Identity)
	{
	}

	RenderableAABB::RenderableAABB(Game& game, Camera& camera, const XMFLOAT4& color)
		: DrawableGameComponent(game, camera),
		mVertexBuffer(nullptr), mIndexBuffer(nullptr), mMaterial(nullptr), mPass(nullptr), mInputLayout(nullptr),
		mColor(color), mPosition(Vector3Helper::Zero), mDirection(Vector3Helper::Forward), mUp(Vector3Helper::Up), mRight(Vector3Helper::Right),
		mWorldMatrix(MatrixHelper::Identity)
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

	void RenderableAABB::InitializeGeometry(const std::vector<XMFLOAT3>& aabb, XMMATRIX matrix)
	{

		XMVECTOR scaleVector;
		XMVECTOR rotQuatVector;
		XMVECTOR translationVector;

		XMFLOAT3 scale;
		XMFLOAT3 translation;

		XMMatrixDecompose(&scaleVector, &rotQuatVector, &translationVector, matrix);
		
		XMStoreFloat3(&scale, scaleVector);
		XMStoreFloat3(&translation, translationVector);


		std::vector<XMFLOAT3> vertices;
		vertices.push_back(XMFLOAT3(aabb[0].x * scale.x, aabb[1].y  * scale.y, aabb[0].z  * scale.z));
		vertices.push_back(XMFLOAT3(aabb[1].x * scale.x, aabb[1].y  * scale.y, aabb[0].z  * scale.z));
		vertices.push_back(XMFLOAT3(aabb[1].x * scale.x, aabb[0].y  * scale.y, aabb[0].z  * scale.z));
		vertices.push_back(XMFLOAT3(aabb[0].x * scale.x, aabb[0].y  * scale.y, aabb[0].z  * scale.z));
		vertices.push_back(XMFLOAT3(aabb[0].x * scale.x, aabb[1].y  * scale.y, aabb[1].z  * scale.z));
		vertices.push_back(XMFLOAT3(aabb[1].x * scale.x, aabb[1].y  * scale.y, aabb[1].z  * scale.z));
		vertices.push_back(XMFLOAT3(aabb[1].x * scale.x, aabb[0].y  * scale.y, aabb[1].z  * scale.z));
		vertices.push_back(XMFLOAT3(aabb[0].x * scale.x, aabb[0].y  * scale.y, aabb[1].z  * scale.z));



		InitializeVertexBuffer(vertices);
	}

	void RenderableAABB::Initialize()
	{
		Effect* effect = new Effect(*mGame);
		effect->CompileFromFile(L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Effects\\BasicEffect.fx");

		mMaterial = new BasicMaterial();
		mMaterial->Initialize(effect);

		mPass = mMaterial->CurrentTechnique()->Passes().at(0);
		mInputLayout = mMaterial->InputLayouts().at(mPass);

		InitializeIndexBuffer();
	}

	void RenderableAABB::Update(const GameTime& gameTime)
	{
		XMMATRIX worldMatrix = XMMatrixIdentity();
		MatrixHelper::SetForward(worldMatrix, mDirection);
		MatrixHelper::SetUp(worldMatrix, mUp);
		MatrixHelper::SetRight(worldMatrix, mRight);
		MatrixHelper::SetTranslation(worldMatrix, mPosition);

		XMStoreFloat4x4(&mWorldMatrix, worldMatrix);
	}

	void RenderableAABB::Draw(const GameTime& gameTime)
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