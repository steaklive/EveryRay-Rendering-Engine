#include "stdafx.h"

#include "Projector.h"
#include "Game.h"
#include "GameTime.h"
#include "VectorHelper.h"
#include "MatrixHelper.h"

namespace Library
{
	RTTI_DEFINITIONS(Projector)

	const float Projector::DefaultFieldOfView = XM_PIDIV4;
	const float Projector::DefaultAspectRatio = 4.0f / 3.0f;
	const float Projector::DefaultNearPlaneDistance = .5f;
	const float Projector::DefaultFarPlaneDistance = 500.0f;
	const float Projector::DefaultScreenWidth = 1024.0f;
	const float Projector::DefaultScreenHeight = 1024.0f;

	Projector::Projector(Game& game)
		: GameComponent(game),
		mFieldOfView(DefaultFieldOfView), mAspectRatio(DefaultAspectRatio), mNearPlaneDistance(DefaultNearPlaneDistance), mFarPlaneDistance(DefaultFarPlaneDistance),
		mPosition(), mDirection(), mUp(), mRight(), mViewMatrix(), mProjectionMatrix()
	{
	}

	Projector::Projector(Game& game, float fieldOfView, float aspectRatio, float nearPlaneDistance, float farPlaneDistance)
		: GameComponent(game),
		mFieldOfView(fieldOfView), mAspectRatio(aspectRatio), mNearPlaneDistance(nearPlaneDistance), mFarPlaneDistance(farPlaneDistance),
		mPosition(), mDirection(), mUp(), mRight(), mViewMatrix(), mProjectionMatrix()
	{
	}

	Projector::~Projector()
	{
	}

	const XMFLOAT3& Projector::Position() const
	{
		return mPosition;
	}

	const XMFLOAT3& Projector::Direction() const
	{
		return mDirection;
	}

	const XMFLOAT3& Projector::Up() const
	{
		return mUp;
	}

	const XMFLOAT3& Projector::Right() const
	{
		return mRight;
	}

	XMVECTOR Projector::PositionVector() const
	{
		return XMLoadFloat3(&mPosition);
	}

	XMVECTOR Projector::DirectionVector() const
	{
		return XMLoadFloat3(&mDirection);
	}

	XMVECTOR Projector::UpVector() const
	{
		return XMLoadFloat3(&mUp);
	}

	XMVECTOR Projector::RightVector() const
	{
		return XMLoadFloat3(&mRight);
	}

	float Projector::AspectRatio() const
	{
		return mAspectRatio;
	}

	float Projector::FieldOfView() const
	{
		return mFieldOfView;
	}

	float Projector::NearPlaneDistance() const
	{
		return mNearPlaneDistance;
	}

	float Projector::FarPlaneDistance() const
	{
		return mFarPlaneDistance;
	}

	XMMATRIX Projector::ViewMatrix() const
	{
		return XMLoadFloat4x4(&mViewMatrix);
	}

	XMMATRIX Projector::ProjectionMatrix() const
	{
		return XMLoadFloat4x4(&mProjectionMatrix);
	}

	XMMATRIX Projector::ViewProjectionMatrix() const
	{
		XMMATRIX viewMatrix = XMLoadFloat4x4(&mViewMatrix);
		XMMATRIX projectionMatrix = XMLoadFloat4x4(&mProjectionMatrix);

		return XMMatrixMultiply(viewMatrix, projectionMatrix);
	}

	void Projector::SetPosition(FLOAT x, FLOAT y, FLOAT z)
	{
		XMVECTOR position = XMVectorSet(x, y, z, 1.0f);
		SetPosition(position);
	}

	void Projector::SetPosition(FXMVECTOR position)
	{
		XMStoreFloat3(&mPosition, position);
	}

	void Projector::SetPosition(const XMFLOAT3& position)
	{
		mPosition = position;
	}

	void Projector::Reset()
	{
		mPosition = Vector3Helper::Zero;
		mDirection = Vector3Helper::Forward;
		mUp = Vector3Helper::Up;
		mRight = Vector3Helper::Right;

		UpdateViewMatrix();
	}

	void Projector::Initialize()
	{
		//UpdateProjectionMatrix();
		//SetProjectionMatrix();
		Reset();
	}

	void Projector::Update(const GameTime& gameTime)
	{
		UpdateViewMatrix();
	}

	void Projector::UpdateViewMatrix()
	{
		XMVECTOR eyePosition = XMLoadFloat3(&mPosition);
		XMVECTOR direction = XMLoadFloat3(&mDirection);
		XMVECTOR upDirection = XMLoadFloat3(&mUp);

		XMMATRIX viewMatrix = XMMatrixLookToRH(eyePosition, direction, upDirection);
		XMStoreFloat4x4(&mViewMatrix, viewMatrix);
	}

	void Projector::SetProjectionMatrix(CXMMATRIX projectionMatrix)
	{
		//orthographic
		//XMMATRIX projectionMatrix = XMMatrixOrthographicLH(256.0f, 256.0f, mNearPlaneDistance, -mFarPlaneDistance);

		//perspective
		//XMMATRIX projectionMatrix = XMMatrixPerspectiveFovRH(mFieldOfView, mAspectRatio, mNearPlaneDistance, mFarPlaneDistance);

		XMStoreFloat4x4(&mProjectionMatrix, projectionMatrix);
	}

	void Projector::UpdateProjectionMatrix()
	{
		//orthographic
		XMMATRIX projectionMatrix = XMMatrixOrthographicLH(256.0f, 256.0f, mNearPlaneDistance, -mFarPlaneDistance);
		
		//perspective
		//XMMATRIX projectionMatrix = XMMatrixPerspectiveFovRH(mFieldOfView, mAspectRatio, mNearPlaneDistance, mFarPlaneDistance);
		
		XMStoreFloat4x4(&mProjectionMatrix, projectionMatrix);
	}

	void Projector::ApplyRotation(CXMMATRIX transform)
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

	void Projector::ApplyRotation(const XMFLOAT4X4& transform)
	{
		XMMATRIX transformMatrix = XMLoadFloat4x4(&transform);
		ApplyRotation(transformMatrix);
	}
}