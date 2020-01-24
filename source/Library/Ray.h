#pragma once
#include "Common.h"

namespace Library
{
	class Ray
	{
	public:
		Ray(FXMVECTOR position, FXMVECTOR direction);
		Ray(const XMFLOAT3& position, const XMFLOAT3& direction);

		const XMFLOAT3& Position() const;
		const XMFLOAT3& Direction() const;

		XMVECTOR PositionVector() const;
		XMVECTOR DirectionVector() const;

		virtual void SetPosition(float x, float y, float z);
		virtual void SetPosition(FXMVECTOR position);
		virtual void SetPosition(const XMFLOAT3& position);

		virtual void SetDirection(float x, float y, float z);
		virtual void SetDirection(FXMVECTOR direction);
		virtual void SetDirection(const XMFLOAT3& direction);

	private:
		Ray();

		XMFLOAT3 mPosition;
		XMFLOAT3 mDirection;
	};
}
