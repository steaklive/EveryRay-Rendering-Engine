#pragma once

#include "Common.h"
#include "ER_Light.h"

namespace EveryRay_Core
{
	class ER_PointLight : public ER_Light
	{
		RTTI_DECLARATIONS(ER_PointLight, ER_Light)
	public:
		ER_PointLight(ER_Core& game);
		ER_PointLight(ER_Core& game, const XMFLOAT3& pos, float radius);
		virtual ~ER_PointLight();

		void SetPosition(FLOAT x, FLOAT y, FLOAT z);
		void SetPosition(const XMFLOAT3& position);
		const XMFLOAT3& GetPosition() const { return mPosition; }
		XMVECTOR PositionVector() const;

		void SetRadius(float value);
		float GetRadius() const { return mRadius; }
	protected:

		XMFLOAT3 mPosition;
		float mRadius;
	};
}