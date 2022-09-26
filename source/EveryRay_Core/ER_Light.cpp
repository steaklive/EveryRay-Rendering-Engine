#include "stdafx.h"

#include "ER_ColorHelper.h"
#include "ER_Light.h"

namespace EveryRay_Core
{
	RTTI_DEFINITIONS(ER_Light)

	ER_Light::ER_Light(ER_Core& game) : ER_CoreComponent(game), mColor(reinterpret_cast<const float*>(&ER_ColorHelper::White))
	{}

	ER_Light::~ER_Light() {}

	void ER_Light::SetColor(XMFLOAT4& color)
	{
		mColor = color;
	}
}