#pragma once

#include "Common.h"
#include "GameComponent.h"

namespace Library
{
	class Light : public GameComponent
	{
		RTTI_DECLARATIONS(Light, GameComponent)

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