#include "stdafx.h"

#include "ProxyModel.h"
#include "Game.h"
#include "GameException.h"
#include "BasicMaterial.h"
#include "Camera.h"
#include "MatrixHelper.h"
#include "VectorHelper.h"
#include "Model.h"
#include "Mesh.h"
#include "Utility.h"
#include "RasterizerStates.h"

namespace Library
{
	RTTI_DEFINITIONS(ProxyModel)

		ProxyModel::ProxyModel(Game& game, Camera& camera, const std::string& modelFileName, float scale)
		: DrawableGameComponent(game, camera),
		mModelFileName(modelFileName), mEffect(nullptr), mMaterial(nullptr),
		mVertexBuffer(nullptr), mIndexBuffer(nullptr), mIndexCount(0),
		mWorldMatrix(MatrixHelper::Identity), mScaleMatrix(MatrixHelper::Identity), mDisplayWireframe(true),
		mPosition(Vector3Helper::Zero), mDirection(Vector3Helper::Forward), mUp(Vector3Helper::Up), mRight(Vector3Helper::Right)
	{
		XMStoreFloat4x4(&mScaleMatrix, XMMatrixScaling(scale, scale, scale));
	}

	ProxyModel::~ProxyModel()
	{
		DeleteObject(mMaterial);
		DeleteObject(mEffect);
		ReleaseObject(mVertexBuffer);
		ReleaseObject(mIndexBuffer);
	}

	const XMFLOAT3& ProxyModel::Position() const
	{
		return mPosition;
	}

	const XMFLOAT3& ProxyModel::Direction() const
	{
		return mDirection;
	}

	const XMFLOAT3& ProxyModel::Up() const
	{
		return mUp;
	}

	const XMFLOAT3& ProxyModel::Right() const
	{
		return mRight;
	}

	XMVECTOR ProxyModel::PositionVector() const
	{
		return XMLoadFloat3(&mPosition);
	}

	XMVECTOR ProxyModel::DirectionVector() const
	{
		return XMLoadFloat3(&mDirection);
	}

	XMVECTOR ProxyModel::UpVector() const
	{
		return XMLoadFloat3(&mUp);
	}

	XMVECTOR ProxyModel::RightVector() const
	{
		return XMLoadFloat3(&mRight);
	}

	bool& ProxyModel::DisplayWireframe()
	{
		return mDisplayWireframe;
	}

	void ProxyModel::SetPosition(FLOAT x, FLOAT y, FLOAT z)
	{
		XMVECTOR position = XMVectorSet(x, y, z, 1.0f);
		SetPosition(position);
	}

	void ProxyModel::SetPosition(FXMVECTOR position)
	{
		XMStoreFloat3(&mPosition, position);
	}

	void ProxyModel::SetPosition(const XMFLOAT3& position)
	{
		mPosition = position;
	}

	void ProxyModel::ApplyRotation(CXMMATRIX transform)
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

	void ProxyModel::ApplyRotation(const XMFLOAT4X4& transform)
	{
		XMMATRIX transformMatrix = XMLoadFloat4x4(&transform);
		ApplyRotation(transformMatrix);
	}

	void ProxyModel::Initialize()
	{
		SetCurrentDirectory(Utility::ExecutableDirectory().c_str());

		std::unique_ptr<Model> model(new Model(*mGame, mModelFileName, true));

		mEffect = new Effect(*mGame);
		mEffect->CompileFromFile(L"C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Effects\\BasicEffect.fx");

		mMaterial = new BasicMaterial();
		mMaterial->Initialize(mEffect);

		Mesh* mesh = model->Meshes().at(0);
		mMaterial->CreateVertexBuffer(mGame->Direct3DDevice(), *mesh, &mVertexBuffer);
		mesh->CreateIndexBuffer(&mIndexBuffer);
		mIndexCount = mesh->Indices().size();
	}

	void ProxyModel::Update(const GameTime& gameTime)
	{
		XMMATRIX worldMatrix = XMMatrixIdentity();
		MatrixHelper::SetForward(worldMatrix, mDirection);
		MatrixHelper::SetUp(worldMatrix, mUp);
		MatrixHelper::SetRight(worldMatrix, mRight);
		MatrixHelper::SetTranslation(worldMatrix, mPosition);

		XMStoreFloat4x4(&mWorldMatrix, XMLoadFloat4x4(&mScaleMatrix) * worldMatrix);
	}

	void ProxyModel::Draw(const GameTime& gametime)
	{
		ID3D11DeviceContext* direct3DDeviceContext = mGame->Direct3DDeviceContext();
		direct3DDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		Pass* pass = mMaterial->CurrentTechnique()->Passes().at(0);
		ID3D11InputLayout* inputLayout = mMaterial->InputLayouts().at(pass);
		direct3DDeviceContext->IASetInputLayout(inputLayout);

		UINT stride = mMaterial->VertexSize();
		UINT offset = 0;
		direct3DDeviceContext->IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &offset);
		direct3DDeviceContext->IASetIndexBuffer(mIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

		XMMATRIX wvp = XMLoadFloat4x4(&mWorldMatrix) * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();
		mMaterial->WorldViewProjection() << wvp;

		pass->Apply(0, direct3DDeviceContext);

		if (mDisplayWireframe)
		{
			mGame->Direct3DDeviceContext()->RSSetState(RasterizerStates::Wireframe);
			direct3DDeviceContext->DrawIndexed(mIndexCount, 0, 0);
			mGame->Direct3DDeviceContext()->RSSetState(nullptr);
		}
		else
		{
			direct3DDeviceContext->DrawIndexed(mIndexCount, 0, 0);
		}
	}
}