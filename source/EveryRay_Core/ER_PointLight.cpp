

#include "ER_PointLight.h"
#include "ER_Core.h"
#include "ER_VectorHelper.h"
#include "ER_Utility.h"
#include "ER_Camera.h"

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

	void ER_PointLight::Update(const ER_CoreTime& time)
	{
		mEditorIntensity = GetColor().w;
		mEditorColor[0] = GetColor().x;
		mEditorColor[1] = GetColor().y;
		mEditorColor[2] = GetColor().z;
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
}