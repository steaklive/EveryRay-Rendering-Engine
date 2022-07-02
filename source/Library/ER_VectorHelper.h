#pragma once
#include "Common.h"

namespace Library
{
	class ER_Vector2Helper
	{
	public:
		static const XMFLOAT2 Zero;
		static const XMFLOAT2 One;

		static std::string ToString(const XMFLOAT2& vector);

	private:
		ER_Vector2Helper();
		ER_Vector2Helper(const ER_Vector2Helper& rhs);
		ER_Vector2Helper& operator=(const ER_Vector2Helper& rhs);
	};

	class ER_Vector3Helper
	{
	public:
		static const XMFLOAT3 Zero;
		static const XMFLOAT3 One;
		static const XMFLOAT3 Forward;
		static const XMFLOAT3 Backward;
		static const XMFLOAT3 Up;
		static const XMFLOAT3 Down;
		static const XMFLOAT3 Right;
		static const XMFLOAT3 Left;

		static std::string ToString(const XMFLOAT3& vector);

	private:
		ER_Vector3Helper();
		ER_Vector3Helper(const ER_Vector3Helper& rhs);
		ER_Vector3Helper& operator=(const ER_Vector3Helper& rhs);
	};

	class ER_Vector4Helper
	{
	public:
		static const XMFLOAT4 Zero;
		static const XMFLOAT4 One;

		static std::string ToString(const XMFLOAT4& vector);

	private:
		ER_Vector4Helper();
		ER_Vector4Helper(const ER_Vector3Helper& rhs);
		ER_Vector4Helper& operator=(const ER_Vector3Helper& rhs);
	};
}