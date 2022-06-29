#pragma once

#include "Common.h"

namespace Library
{
	class Game;
	class GameTime;

	class ER_CoreComponent : public RTTI
	{
		RTTI_DECLARATIONS(ER_CoreComponent, RTTI)

	public:
		ER_CoreComponent();
		ER_CoreComponent(Game& game);

		virtual ~ER_CoreComponent();

		Game* GetGame();
		void SetGame(Game& game);
		bool Enabled() const;
		void SetEnabled(bool enabled);

		virtual void Initialize();
		virtual void Update(const GameTime& gameTime);
		
	protected:
		Game * mGame;
		bool mEnabled;

	private:
		ER_CoreComponent(const ER_CoreComponent& rhs);
		ER_CoreComponent& operator=(const ER_CoreComponent& rhs);

	};
}
