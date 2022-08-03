#pragma once

#include "Common.h"
#include "ER_Light.h"

namespace Library
{
	class ER_PointLight : public ER_Light
	{
		RTTI_DECLARATIONS(ER_PointLight, ER_Light)

	public:
		ER_PointLight(ER_Core& game);
		virtual ~ER_PointLight();

		XMFLOAT3& Position();
		XMVECTOR PositionVector() const;
		FLOAT Radius() const;

		virtual void SetPosition(FLOAT x, FLOAT y, FLOAT z);
		virtual void SetPosition(FXMVECTOR position);
		virtual void SetPosition(const XMFLOAT3& position);
		virtual void SetRadius(float value);

		static const float DefaultRadius;

	protected:
		XMFLOAT3 mPosition;
		float mRadius;
	};
}