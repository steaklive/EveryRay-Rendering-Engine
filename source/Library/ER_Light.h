#pragma once

#include "Common.h"
#include "ER_CoreComponent.h"

namespace Library
{
	class ER_Light : public ER_CoreComponent
	{
		RTTI_DECLARATIONS(ER_Light, ER_CoreComponent)

	public:
		ER_Light(ER_Core& game);
		~ER_Light();

		const XMCOLOR& Color() const;
		XMVECTOR ColorVector() const;

		void SetColor(FLOAT r, FLOAT g, FLOAT b, FLOAT a);
		void SetColor(XMCOLOR color);
		void SetColor(FXMVECTOR color);

	protected:
		XMCOLOR mColor;
	};
}