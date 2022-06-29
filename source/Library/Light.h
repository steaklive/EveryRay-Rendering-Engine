#pragma once

#include "Common.h"
#include "ER_CoreComponent.h"

namespace Library
{
	class Light : public ER_CoreComponent
	{
		RTTI_DECLARATIONS(Light, ER_CoreComponent)

	public:
		Light(Game& game);
		~Light();

		const XMCOLOR& Color() const;
		XMVECTOR ColorVector() const;

		void SetColor(FLOAT r, FLOAT g, FLOAT b, FLOAT a);
		void SetColor(XMCOLOR color);
		void SetColor(FXMVECTOR color);

	protected:
		XMCOLOR mColor;
	};
}