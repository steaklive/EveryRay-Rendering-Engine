

#include "ER_Projector.h"
#include "ER_Core.h"
#include "ER_CoreTime.h"
#include "ER_VectorHelper.h"
#include "ER_MatrixHelper.h"

namespace EveryRay_Core
{
	const float ER_Projector::DefaultFieldOfView = XM_PIDIV4;
	const float ER_Projector::DefaultAspectRatio = 4.0f / 3.0f;
	const float ER_Projector::DefaultNearPlaneDistance = .5f;
	const float ER_Projector::DefaultFarPlaneDistance = 500.0f;
	const float ER_Projector::DefaultScreenWidth = 1024.0f;
	const float ER_Projector::DefaultScreenHeight = 1024.0f;

	ER_Projector::ER_Projector(ER_Core& game)
		: 
		mFieldOfView(DefaultFieldOfView), mAspectRatio(DefaultAspectRatio), mNearPlaneDistance(DefaultNearPlaneDistance), mFarPlaneDistance(DefaultFarPlaneDistance),
		mPosition(), mDirection(), mUp(), mRight(), mViewMatrix(), mProjectionMatrix()
	{
	}

	ER_Projector::ER_Projector(ER_Core& game, float fieldOfView, float aspectRatio, float nearPlaneDistance, float farPlaneDistance)
		:
		mFieldOfView(fieldOfView), mAspectRatio(aspectRatio), mNearPlaneDistance(nearPlaneDistance), mFarPlaneDistance(farPlaneDistance),
		mPosition(), mDirection(), mUp(), mRight(), mViewMatrix(), mProjectionMatrix()
	{
	}

	ER_Projector::~ER_Projector()
	{
	}

	const XMFLOAT3& ER_Projector::Position() const
	{
		return mPosition;
	}

	const XMFLOAT3& ER_Projector::Direction() const
	{
		return mDirection;
	}

	const XMFLOAT3& ER_Projector::Up() const
	{
		return mUp;
	}

	const XMFLOAT3& ER_Projector::Right() const
	{
		return mRight;
	}

	XMVECTOR ER_Projector::PositionVector() const
	{
		return XMLoadFloat3(&mPosition);
	}

	XMVECTOR ER_Projector::DirectionVector() const
	{
		return XMLoadFloat3(&mDirection);
	}

	XMVECTOR ER_Projector::UpVector() const
	{
		return XMLoadFloat3(&mUp);
	}

	XMVECTOR ER_Projector::RightVector() const
	{
		return XMLoadFloat3(&mRight);
	}

	float ER_Projector::AspectRatio() const
	{
		return mAspectRatio;
	}

	float ER_Projector::FieldOfView() const
	{
		return mFieldOfView;
	}

	float ER_Projector::NearPlaneDistance() const
	{
		return mNearPlaneDistance;
	}

	float ER_Projector::FarPlaneDistance() const
	{
		return mFarPlaneDistance;
	}

	XMMATRIX ER_Projector::ViewMatrix() const
	{
		return XMLoadFloat4x4(&mViewMatrix);
	}

	XMMATRIX ER_Projector::ProjectionMatrix() const
	{
		return XMLoadFloat4x4(&mProjectionMatrix);
	}

	XMMATRIX ER_Projector::ViewProjectionMatrix() const
	{
		XMMATRIX viewMatrix = XMLoadFloat4x4(&mViewMatrix);
		XMMATRIX projectionMatrix = XMLoadFloat4x4(&mProjectionMatrix);

		return XMMatrixMultiply(viewMatrix, projectionMatrix);
	}

	void ER_Projector::SetPosition(FLOAT x, FLOAT y, FLOAT z)
	{
		XMVECTOR position = XMVectorSet(x, y, z, 1.0f);
		SetPosition(position);
	}

	void ER_Projector::SetPosition(FXMVECTOR position)
	{
		XMStoreFloat3(&mPosition, position);
	}

	void ER_Projector::SetPosition(const XMFLOAT3& position)
	{
		mPosition = position;
	}

	void ER_Projector::Reset()
	{
		mPosition = ER_Vector3Helper::Zero;
		mDirection = ER_Vector3Helper::Forward;
		mUp = ER_Vector3Helper::Up;
		mRight = ER_Vector3Helper::Right;

		UpdateViewMatrix();
	}

	void ER_Projector::Initialize()
	{
		//UpdateProjectionMatrix();
		//SetProjectionMatrix();
		Reset();
	}

	void ER_Projector::Update()
	{
		UpdateViewMatrix();
	}

	void ER_Projector::UpdateViewMatrix()
	{
		XMVECTOR eyePosition = XMLoadFloat3(&mPosition);
		XMVECTOR direction = XMLoadFloat3(&mDirection);
		XMVECTOR upDirection = XMLoadFloat3(&mUp);

		XMMATRIX viewMatrix = XMMatrixLookToRH(eyePosition, direction, upDirection);
		XMStoreFloat4x4(&mViewMatrix, viewMatrix);
	}

	void ER_Projector::SetViewMatrix(const XMFLOAT3& pos, const XMFLOAT3& dir, const XMFLOAT3& up)
	{
		mPosition = pos;
		mDirection = dir;
		mUp = up;

		XMVECTOR eyePosition = XMLoadFloat3(&mPosition);
		XMVECTOR direction = XMLoadFloat3(&mDirection);
		XMVECTOR upDirection = XMLoadFloat3(&mUp);

		XMMATRIX viewMatrix = XMMatrixLookToRH(eyePosition, direction, upDirection);
		XMStoreFloat4x4(&mViewMatrix, viewMatrix);
	}

	void ER_Projector::SetProjectionMatrix(CXMMATRIX projectionMatrix)
	{
		//orthographic
		//XMMATRIX projectionMatrix = XMMatrixOrthographicLH(256.0f, 256.0f, mNearPlaneDistance, -mFarPlaneDistance);

		//perspective
		//XMMATRIX projectionMatrix = XMMatrixPerspectiveFovRH(mFieldOfView, mAspectRatio, mNearPlaneDistance, mFarPlaneDistance);

		XMStoreFloat4x4(&mProjectionMatrix, projectionMatrix);
	}

	void ER_Projector::UpdateProjectionMatrix()
	{
		//orthographic
		XMMATRIX projectionMatrix = XMMatrixOrthographicLH(256.0f, 256.0f, mNearPlaneDistance, -mFarPlaneDistance);
		
		//perspective
		//XMMATRIX projectionMatrix = XMMatrixPerspectiveFovRH(mFieldOfView, mAspectRatio, mNearPlaneDistance, mFarPlaneDistance);
		
		XMStoreFloat4x4(&mProjectionMatrix, projectionMatrix);
	}

	void ER_Projector::ApplyRotation(CXMMATRIX transform)
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

	void ER_Projector::ApplyRotation(const XMFLOAT4X4& transform)
	{
		XMMATRIX transformMatrix = XMLoadFloat4x4(&transform);
		ApplyRotation(transformMatrix);
	}

	void ER_Projector::ApplyTransform(CXMMATRIX transform)
	{
		XMVECTOR direction = XMVECTOR{ 0.0f, 0.0, -1.0f };
		XMVECTOR up = XMVECTOR{ 0.0f, 1.0, 0.0f };

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
}