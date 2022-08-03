#include "stdafx.h"

#include "ER_PointLight.h"
#include "ER_VectorHelper.h"

namespace Library
{
	RTTI_DEFINITIONS(ER_PointLight)

	const float ER_PointLight::DefaultRadius = 10.0f;

	ER_PointLight::ER_PointLight(ER_Core& game)
		: ER_Light(game), mPosition(ER_Vector3Helper::Zero), mRadius(DefaultRadius)
	{
	}

	ER_PointLight::~ER_PointLight()
	{
	}

	XMFLOAT3& ER_PointLight::Position()
	{
		return mPosition;
	}

	XMVECTOR ER_PointLight::PositionVector() const
	{
		return XMLoadFloat3(&mPosition);
	}

	float ER_PointLight::Radius() const
	{
		return mRadius;
	}

	void ER_PointLight::SetPosition(FLOAT x, FLOAT y, FLOAT z)
	{
		XMVECTOR position = XMVectorSet(x, y, z, 1.0f);
		SetPosition(position);
	}

	void ER_PointLight::SetPosition(FXMVECTOR position)
	{
		XMStoreFloat3(&mPosition, position);
	}

	void ER_PointLight::SetPosition(const XMFLOAT3& position)
	{
		mPosition = position;
	}

	void ER_PointLight::SetRadius(float value)
	{
		mRadius = value;
	}
}