#include "stdafx.h"

#include <sstream>
#include "ER_VectorHelper.h"

namespace EveryRay_Core
{
	const XMFLOAT2 ER_Vector2Helper::Zero = XMFLOAT2(0.0f, 0.0f);
	const XMFLOAT2 ER_Vector2Helper::One = XMFLOAT2(1.0f, 1.0f);

	std::string ER_Vector2Helper::ToString(const XMFLOAT2& vector)
	{
		std::stringstream stream;

		stream << "{" << vector.x << ", " << vector.y << "}";

		return stream.str();
	}

	const XMFLOAT3 ER_Vector3Helper::Zero = XMFLOAT3(0.0f, 0.0f, 0.0f);
	const XMFLOAT3 ER_Vector3Helper::One = XMFLOAT3(1.0f, 1.0f, 1.0f);
	const XMFLOAT3 ER_Vector3Helper::Forward = XMFLOAT3(0.0f, 0.0f, -1.0f);
	const XMFLOAT3 ER_Vector3Helper::Backward = XMFLOAT3(0.0f, 0.0f, 1.0f);
	const XMFLOAT3 ER_Vector3Helper::Up = XMFLOAT3(0.0f, 1.0f, 0.0f);
	const XMFLOAT3 ER_Vector3Helper::Down = XMFLOAT3(0.0f, -1.0f, 0.0f);
	const XMFLOAT3 ER_Vector3Helper::Right = XMFLOAT3(1.0f, 0.0f, 0.0f);
	const XMFLOAT3 ER_Vector3Helper::Left = XMFLOAT3(-1.0f, 0.0f, 0.0f);

	std::string ER_Vector3Helper::ToString(const XMFLOAT3& vector)
	{
		std::stringstream stream;

		stream << "{" << vector.x << ", " << vector.y << ", " << vector.z << "}";

		return stream.str();
	}

	const XMFLOAT4 ER_Vector4Helper::Zero = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	const XMFLOAT4 ER_Vector4Helper::One = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

	std::string ER_Vector4Helper::ToString(const XMFLOAT4& vector)
	{
		std::stringstream stream;

		stream << "{" << vector.x << ", " << vector.y << ", " << vector.z << ", " << vector.w << "}";

		return stream.str();
	}
}