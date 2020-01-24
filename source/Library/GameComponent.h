#pragma once

#include "Common.h"

namespace Library
{
	class Game;
	class GameTime;

	class GameComponent : public RTTI
	{
		RTTI_DECLARATIONS(GameComponent, RTTI)

	public:
		GameComponent();
		GameComponent(Game& game);

		virtual ~GameComponent();

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
		GameComponent(const GameComponent& rhs);
		GameComponent& operator=(const GameComponent& rhs);

	};
}
