#include "stdafx.h"

#include "ER_DebugProxyObject.h"
#include "Game.h"
#include "ER_CoreTime.h"
#include "ER_CoreException.h"
#include "MatrixHelper.h"
#include "VectorHelper.h"
#include "ER_Model.h"
#include "ER_Mesh.h"
#include "Utility.h"
#include "ER_Camera.h"
#include "RasterizerStates.h"
#include "ER_BasicColorMaterial.h"
#include "ER_MaterialsCallbacks.h"

namespace Library
{
	ER_DebugProxyObject::ER_DebugProxyObject(Game& game, ER_Camera& camera, const std::string& modelFileName, float scale)
		:
		mGame(game),
		mModelFileName(modelFileName), mMaterial(nullptr),
		mVertexBuffer(nullptr), mIndexBuffer(nullptr), mIndexCount(0),
		mWorldMatrix(MatrixHelper::Identity), mScaleMatrix(MatrixHelper::Identity), mDisplayWireframe(false),
		mPosition(Vector3Helper::Zero), mDirection(Vector3Helper::Forward), mUp(Vector3Helper::Up), mRight(Vector3Helper::Right)
	{
		XMStoreFloat4x4(&mScaleMatrix, XMMatrixScaling(scale, scale, scale));
	}

	ER_DebugProxyObject::~ER_DebugProxyObject()
	{
		DeleteObject(mMaterial);
		ReleaseObject(mVertexBuffer);
		ReleaseObject(mIndexBuffer);
	}

	const XMFLOAT3& ER_DebugProxyObject::Position() const
	{
		return mPosition;
	}

	const XMFLOAT3& ER_DebugProxyObject::Direction() const
	{
		return mDirection;
	}

	const XMFLOAT3& ER_DebugProxyObject::Up() const
	{
		return mUp;
	}

	const XMFLOAT3& ER_DebugProxyObject::Right() const
	{
		return mRight;
	}

	XMVECTOR ER_DebugProxyObject::PositionVector() const
	{
		return XMLoadFloat3(&mPosition);
	}

	XMVECTOR ER_DebugProxyObject::DirectionVector() const
	{
		return XMLoadFloat3(&mDirection);
	}

	XMVECTOR ER_DebugProxyObject::UpVector() const
	{
		return XMLoadFloat3(&mUp);
	}

	XMVECTOR ER_DebugProxyObject::RightVector() const
	{
		return XMLoadFloat3(&mRight);
	}

	bool& ER_DebugProxyObject::DisplayWireframe()
	{
		return mDisplayWireframe;
	}

	void ER_DebugProxyObject::SetPosition(FLOAT x, FLOAT y, FLOAT z)
	{
		XMVECTOR position = XMVectorSet(x, y, z, 1.0f);
		SetPosition(position);
	}

	void ER_DebugProxyObject::SetPosition(FXMVECTOR position)
	{
		XMStoreFloat3(&mPosition, position);
	}

	void ER_DebugProxyObject::SetPosition(const XMFLOAT3& position)
	{
		mPosition = position;
	}

	void ER_DebugProxyObject::ApplyRotation(CXMMATRIX transform)
	{
		XMVECTOR direction = XMLoadFloat3(&mDirection);
		XMVECTOR up = XMLoadFloat3(&mUp);

		direction = XMVector3TransformNormal(direction, transform);
		direction = XMVector3Normalize(direction);

		up = XMVector3TransformNormal(up, transform);
		up = XMVector3Normalize(up);

		XMVECTOR right = XMVector3Cross(direction, up);
		up = XMVector3Cross(right, direction);

		XMStoreFloat3(&mDirection, direction);
		XMStoreFloat3(&mUp, up);
		XMStoreFloat3(&mRight, right);
	}
	void ER_DebugProxyObject::ApplyTransform(XMMATRIX transformMatrix)
	{
		XMVECTOR direction = XMVECTOR{ 0.0f, 0.0, -1.0f };
		XMVECTOR up = XMVECTOR{ 0.0f, 1.0, 0.0f };

		direction = XMVector3TransformNormal(direction, transformMatrix);
		direction = XMVector3Normalize(direction);

		up = XMVector3TransformNormal(up, transformMatrix);
		up = XMVector3Normalize(up);

		XMVECTOR right = XMVector3Cross(direction, up);
		up = XMVector3Cross(right, direction);

		XMStoreFloat3(&mDirection, direction);
		XMStoreFloat3(&mUp, up);
		XMStoreFloat3(&mRight, right);
	}
	void ER_DebugProxyObject::ApplyRotation(const XMFLOAT4X4& transform)
	{
		XMMATRIX transformMatrix = XMLoadFloat4x4(&transform);
		ApplyRotation(transformMatrix);
	}

	void ER_DebugProxyObject::ApplyRotaitonAroundPoint(float radius, float angle)
	{
		XMMATRIX rotation = XMMatrixRotationX(angle);
		XMMATRIX translate = XMMatrixTranslation(0.0f, 0.0f, radius);
		XMMATRIX matFinal = rotation * translate;
	}

	void ER_DebugProxyObject::Initialize()
	{
		SetCurrentDirectory(Utility::ExecutableDirectory().c_str());

		std::unique_ptr<ER_Model> model(new ER_Model(mGame, mModelFileName, true));

		mMaterial = new ER_BasicColorMaterial(mGame, {}, HAS_VERTEX_SHADER | HAS_PIXEL_SHADER);

		auto& meshes = model->Meshes();
		mMaterial->CreateVertexBuffer(meshes[0], &mVertexBuffer);
		meshes[0].CreateIndexBuffer(&mIndexBuffer);
		mIndexCount = meshes[0].Indices().size();
	}

	void ER_DebugProxyObject::Update(const ER_CoreTime& gameTime)
	{
		XMMATRIX worldMatrix = XMMatrixIdentity();
		MatrixHelper::SetForward(worldMatrix, mDirection);
		MatrixHelper::SetUp(worldMatrix, mUp);
		MatrixHelper::SetRight(worldMatrix, mRight);
		MatrixHelper::SetTranslation(worldMatrix, mPosition);
		XMStoreFloat4x4(&mWorldMatrix, XMLoadFloat4x4(&mScaleMatrix) * worldMatrix);
	}

	void ER_DebugProxyObject::Draw(const ER_CoreTime& gametime)
	{
		auto context = mGame.Direct3DDeviceContext();

		UINT stride = mMaterial->VertexSize();
		UINT offset = 0;
		context->IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &offset);
		context->IASetIndexBuffer(mIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

		mMaterial->PrepareForRendering(XMLoadFloat4x4(&mWorldMatrix), { 1.0f, 0.65f, 0.0f, 1.0f });
		context->DrawIndexed(mIndexCount, 0, 0);
	}
}