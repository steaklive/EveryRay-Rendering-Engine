#include "stdafx.h"

#include "ER_Camera.h"
#include "Game.h"
#include "GameTime.h"
#include "VectorHelper.h"
#include "MatrixHelper.h"
#include "ER_RenderingObject.h"
#include "Utility.h"

#define MAX_NUM_CASCADES 3

namespace Library
{
	RTTI_DEFINITIONS(ER_Camera)

	const float ER_Camera::DefaultFieldOfView = XM_PIDIV2;
	const float ER_Camera::DefaultNearPlaneDistance = 0.01f;
	const float ER_Camera::DefaultFarPlaneDistance = 600;

	const float cameraCascadeDistances[MAX_NUM_CASCADES] = { 125.0f, 500.0f, 1200.0f };
	//const float cascadeDistances[MAX_NUM_CASCADES] = { 75.0f, 150.0f, 600.0f };

	ER_Camera::ER_Camera(Game& game)
		: ER_CoreComponent(game),
		mFieldOfView(DefaultFieldOfView), mAspectRatio(game.AspectRatio()), mNearPlaneDistance(DefaultNearPlaneDistance), mFarPlaneDistance(DefaultFarPlaneDistance),
		mPosition(), mDirection(), mUp(), mRight(), mViewMatrix(), mProjectionMatrix(), mFrustum(XMMatrixIdentity())
	{
	}

	ER_Camera::ER_Camera(Game& game, float fieldOfView, float aspectRatio, float nearPlaneDistance, float farPlaneDistance)
		: ER_CoreComponent(game),
		mFieldOfView(fieldOfView), mAspectRatio(aspectRatio), mNearPlaneDistance(nearPlaneDistance), mFarPlaneDistance(farPlaneDistance),
		mPosition(), mDirection(), mUp(), mRight(), mViewMatrix(), mProjectionMatrix(), mFrustum(XMMatrixIdentity())
	{
	}

	ER_Camera::~ER_Camera()
	{
	}

	const XMFLOAT3& ER_Camera::Position() const
	{
		return mPosition;
	}

	const XMFLOAT3& ER_Camera::Direction() const
	{
		return mDirection;
	}

	const XMFLOAT3& ER_Camera::Up() const
	{
		return mUp;
	}

	const XMFLOAT3& ER_Camera::Right() const
	{
		return mRight;
	}

	XMVECTOR ER_Camera::PositionVector() const
	{
		return XMLoadFloat3(&mPosition);
	}

	XMVECTOR ER_Camera::DirectionVector() const
	{
		return XMLoadFloat3(&mDirection);
	}

	XMVECTOR ER_Camera::UpVector() const
	{
		return XMLoadFloat3(&mUp);
	}

	XMVECTOR ER_Camera::RightVector() const
	{
		return XMLoadFloat3(&mRight);
	}

	float ER_Camera::AspectRatio() const
	{
		return mAspectRatio;
	}

	float ER_Camera::FieldOfView() const
	{
		return mFieldOfView;
	}

	float ER_Camera::NearPlaneDistance() const
	{
		return mNearPlaneDistance;
	}

	float ER_Camera::FarPlaneDistance() const
	{
		return mFarPlaneDistance;
	}

	XMMATRIX ER_Camera::ViewMatrix() const
	{
		return XMLoadFloat4x4(&mViewMatrix);
	}	
	XMFLOAT4X4 ER_Camera::ViewMatrix4X4() const
	{
		return mViewMatrix;
	}

	XMMATRIX ER_Camera::ProjectionMatrix() const
	{
		return XMLoadFloat4x4(&mProjectionMatrix);
	}

	XMFLOAT4X4 ER_Camera::ProjectionMatrix4X4() const
	{
		return mProjectionMatrix;
	}

	XMMATRIX ER_Camera::ViewProjectionMatrix() const
	{
		XMMATRIX viewMatrix = XMLoadFloat4x4(&mViewMatrix);
		XMMATRIX projectionMatrix = XMLoadFloat4x4(&mProjectionMatrix);

		return XMMatrixMultiply(viewMatrix, projectionMatrix);
	}

	void ER_Camera::SetPosition(FLOAT x, FLOAT y, FLOAT z)
	{
		XMVECTOR position = XMVectorSet(x, y, z, 1.0f);
		SetPosition(position);
	}

	void ER_Camera::SetPosition(FXMVECTOR position)
	{
		XMStoreFloat3(&mPosition, position);
	}

	void ER_Camera::SetPosition(const XMFLOAT3& position)
	{
		mPosition = position;
	}

	void ER_Camera::SetDirection(const XMFLOAT3& direction)
	{
		mDirection = direction;
	}

	void ER_Camera::SetUp(const XMFLOAT3& up)
	{
		mUp = up;
	}

	void ER_Camera::SetFOV(float fov)
	{
		mFieldOfView = fov;
		UpdateProjectionMatrix();
	}

	void ER_Camera::SetFarPlaneDistance(float value)
	{
		mFarPlaneDistance = value;
		UpdateProjectionMatrix();
	}

	XMMATRIX ER_Camera::GetCustomViewProjectionMatrixForCascade(int cascadeIndex)
	{
		XMMATRIX projectionMatrix;
		float delta = 5.0f;

		switch (cascadeIndex)
		{
		case 0:
			projectionMatrix = XMMatrixPerspectiveFovRH(mFieldOfView, mAspectRatio, mNearPlaneDistance, cameraCascadeDistances[0]);
			break;
		case 1:
			projectionMatrix = XMMatrixPerspectiveFovRH(mFieldOfView, mAspectRatio, cameraCascadeDistances[0], cameraCascadeDistances[1]);
			break;
		case 2:
			projectionMatrix = XMMatrixPerspectiveFovRH(mFieldOfView, mAspectRatio, cameraCascadeDistances[1], cameraCascadeDistances[2]);
			break;


		default:
			projectionMatrix = XMMatrixPerspectiveFovRH(mFieldOfView, mAspectRatio, mNearPlaneDistance, cameraCascadeDistances[2]);
		}

		XMMATRIX viewMatrix = XMLoadFloat4x4(&mViewMatrix);

		return XMMatrixMultiply(viewMatrix, projectionMatrix);
	}

	void ER_Camera::SetNearPlaneDistance(float value)
	{
		mNearPlaneDistance = value;
		UpdateProjectionMatrix();
	}

	void ER_Camera::Reset()
	{
		mPosition = Vector3Helper::Zero;
		mDirection = Vector3Helper::Forward;
		mUp = Vector3Helper::Up;
		mRight = Vector3Helper::Right;

		UpdateViewMatrix();
	}

	void ER_Camera::Initialize()
	{
		UpdateProjectionMatrix();
		Reset();

		mFrustum.SetMatrix(ViewProjectionMatrix());
	}

	void ER_Camera::Update(const GameTime& gameTime)
	{
		UpdateViewMatrix();
		mFrustum.SetMatrix(ViewProjectionMatrix());
	}

	void ER_Camera::UpdateViewMatrix(bool leftHanded)
	{
		XMVECTOR eyePosition = XMLoadFloat3(&mPosition);
		XMVECTOR direction = XMLoadFloat3(&mDirection);
		XMVECTOR upDirection = XMLoadFloat3(&mUp);

		XMMATRIX viewMatrix = (leftHanded) 
			? XMMatrixLookToLH(eyePosition, direction, upDirection) 
			: XMMatrixLookToRH(eyePosition, direction, upDirection);
		XMStoreFloat4x4(&mViewMatrix, viewMatrix);
	}

	void ER_Camera::UpdateProjectionMatrix(bool leftHanded)
	{
		XMMATRIX projectionMatrix = (leftHanded) 
			? XMMatrixPerspectiveFovLH(mFieldOfView, mAspectRatio, mNearPlaneDistance, mFarPlaneDistance)
			: XMMatrixPerspectiveFovRH(mFieldOfView, mAspectRatio, mNearPlaneDistance, mFarPlaneDistance);
		XMStoreFloat4x4(&mProjectionMatrix, projectionMatrix);
	}

	void ER_Camera::ApplyRotation(CXMMATRIX transform)
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

		mRotationMatrix = transform;
	}

	void ER_Camera::ApplyRotation(const XMFLOAT4X4& transform)
	{
		XMMATRIX transformMatrix = XMLoadFloat4x4(&transform);
		ApplyRotation(transformMatrix);
	}

	XMMATRIX ER_Camera::RotationTransformMatrix() const
	{
		return mRotationMatrix;
	}

	float ER_Camera::GetCameraFarShadowCascadeDistance (int index) const
	{
		assert(index < (sizeof(cameraCascadeDistances) / sizeof(cameraCascadeDistances[0])));
		return cameraCascadeDistances[index];
	}
	float ER_Camera::GetCameraNearShadowCascadeDistance (int index) const
	{
		assert(index < (sizeof(cameraCascadeDistances) / sizeof(cameraCascadeDistances[0])));
		if (index == 0)
			return mNearPlaneDistance;
		else
			return cameraCascadeDistances[index - 1];
	}
}