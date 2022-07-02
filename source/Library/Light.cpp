#include "stdafx.h"

#include "ER_ColorHelper.h"
#include "Light.h"

namespace Library
{
	RTTI_DEFINITIONS(Light)

	Light::Light(Game& game) : ER_CoreComponent(game), mColor(reinterpret_cast<const float*>(&ER_ColorHelper::White))
	{}

	Light::~Light() {}

	const XMCOLOR& Light::Color() const
	{
		return mColor;
	}

	XMVECTOR Light::ColorVector() const
	{
		return XMLoadColor(&mColor);
	}

	void Light::SetColor(FLOAT r, FLOAT g, FLOAT b, FLOAT a)
	{
		XMCOLOR color(r,g,b,a);
		SetColor(color);
	}

	void Light::SetColor(XMCOLOR color)
	{
		mColor = color;
	}

	void Light::SetColor(FXMVECTOR color)
	{
		XMStoreColor(&mColor, color);
	}
}