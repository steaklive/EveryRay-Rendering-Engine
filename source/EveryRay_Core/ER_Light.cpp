#include "stdafx.h"

#include "ER_ColorHelper.h"
#include "ER_Light.h"

namespace EveryRay_Core
{
	RTTI_DEFINITIONS(ER_Light)

	ER_Light::ER_Light(ER_Core& game) : ER_CoreComponent(game), mColor(reinterpret_cast<const float*>(&ER_ColorHelper::White))
	{}

	ER_Light::~ER_Light() {}

	const XMCOLOR& ER_Light::Color() const
	{
		return mColor;
	}

	XMVECTOR ER_Light::ColorVector() const
	{
		return XMLoadColor(&mColor);
	}

	void ER_Light::SetColor(FLOAT r, FLOAT g, FLOAT b, FLOAT a)
	{
		XMCOLOR color(r,g,b,a);
		SetColor(color);
	}

	void ER_Light::SetColor(XMCOLOR color)
	{
		mColor = color;
	}

	void ER_Light::SetColor(FXMVECTOR color)
	{
		XMStoreColor(&mColor, color);
	}
}