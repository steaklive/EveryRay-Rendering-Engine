#pragma once
#include "Common.h"

namespace Library
{
	class ER_Ray
	{
	public:
		ER_Ray(FXMVECTOR position, FXMVECTOR direction);
		ER_Ray(const XMFLOAT3& position, const XMFLOAT3& direction);

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
		ER_Ray();

		XMFLOAT3 mPosition;
		XMFLOAT3 mDirection;
	};
}
