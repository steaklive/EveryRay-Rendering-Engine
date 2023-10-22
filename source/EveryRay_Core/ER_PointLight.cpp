#include "stdafx.h"

#include "ER_PointLight.h"
#include "ER_Core.h"
#include "ER_VectorHelper.h"

namespace EveryRay_Core
{
	RTTI_DEFINITIONS(ER_PointLight)

	ER_PointLight::ER_PointLight(ER_Core& game)
		: ER_Light(game), mPosition(ER_Vector3Helper::Zero), mRadius(0.0)
	{
	}

	ER_PointLight::ER_PointLight(ER_Core& game, const XMFLOAT3& pos, float radius)
		: ER_Light(game), mPosition(pos), mRadius(radius)
	{
	}

	ER_PointLight::~ER_PointLight()
	{
	}

	XMVECTOR ER_PointLight::PositionVector() const
	{
		return XMLoadFloat3(&mPosition);
	}

	void ER_PointLight::SetPosition(FLOAT x, FLOAT y, FLOAT z)
	{
		SetPosition(XMFLOAT3(x,y,z));
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