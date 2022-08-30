#pragma once

#include "Common.h"
#include "ER_CoreComponent.h"

namespace EveryRay_Core
{
	class ER_Light : public ER_CoreComponent
	{
		RTTI_DECLARATIONS(ER_Light, ER_CoreComponent)

	public:
		ER_Light(ER_Core& game);
		~ER_Light();

		const XMFLOAT4& GetColor() const { return mColor; }
		void SetColor(XMFLOAT4& color);
	protected:
		XMFLOAT4 mColor;
	};
}