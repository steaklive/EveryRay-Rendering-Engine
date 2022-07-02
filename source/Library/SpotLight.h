#pragma once

#include "Common.h"
#include "PointLight.h"

namespace Library
{
	class SpotLight : public PointLight
	{
		RTTI_DECLARATIONS(SpotLight, PointLight)

	public:
		SpotLight(ER_Core& game);
		virtual ~SpotLight();

		const XMFLOAT3& Direction() const;
		const XMFLOAT3& Up() const;
		const XMFLOAT3& Right() const;

		XMVECTOR DirectionVector() const;
		XMVECTOR UpVector() const;
		XMVECTOR RightVector() const;

		float InnerAngle();
		void SetInnerAngle(float value);

		float OuterAngle();
		void SetOuterAngle(float value);

		void ApplyRotation(CXMMATRIX transform);
		void ApplyRotation(const XMFLOAT4X4& transform);

		static const float DefaultInnerAngle;
		static const float DefaultOuterAngle;

	protected:
		XMFLOAT3 mDirection;
		XMFLOAT3 mUp;
		XMFLOAT3 mRight;
		float mInnerAngle;
		float mOuterAngle;
	};
}