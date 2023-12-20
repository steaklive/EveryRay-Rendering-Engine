

#include "ER_Ray.h"

namespace EveryRay_Core
{
	ER_Ray::ER_Ray(FXMVECTOR position, FXMVECTOR direction)
		: mPosition(), mDirection()
	{
		XMStoreFloat3(&mPosition, position);
		XMStoreFloat3(&mDirection, direction);
	}

	ER_Ray::ER_Ray(const XMFLOAT3& position, const XMFLOAT3& direction)
		: mPosition(position), mDirection(direction)
	{
	}

	const XMFLOAT3& ER_Ray::Position() const
	{
		return mPosition;
	}

	const XMFLOAT3& ER_Ray::Direction() const
	{
		return mDirection;
	}

	XMVECTOR ER_Ray::PositionVector() const
	{
		return XMLoadFloat3(&mPosition);
	}

	XMVECTOR ER_Ray::DirectionVector() const
	{
		return XMLoadFloat3(&mDirection);
	}

	void ER_Ray::SetPosition(FLOAT x, FLOAT y, FLOAT z)
	{
		XMVECTOR position = XMVectorSet(x, y, z, 1.0f);
		SetPosition(position);
	}

	void ER_Ray::SetPosition(FXMVECTOR position)
	{
		XMStoreFloat3(&mPosition, position);
	}

	void ER_Ray::SetPosition(const XMFLOAT3& position)
	{
		mPosition = position;
	}

	void ER_Ray::SetDirection(FLOAT x, FLOAT y, FLOAT z)
	{
		XMVECTOR direction = XMVectorSet(x, y, z, 0.0f);
		SetDirection(direction);
	}

	void ER_Ray::SetDirection(FXMVECTOR direction)
	{
		XMStoreFloat3(&mDirection, direction);
	}

	void ER_Ray::SetDirection(const XMFLOAT3& direction)
	{
		mDirection = direction;
	}
}