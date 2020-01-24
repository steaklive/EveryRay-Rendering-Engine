#pragma once

#include "GameComponent.h"

namespace Library
{
	class Keyboard : public GameComponent
	{
		RTTI_DECLARATIONS(Keyboard, GameComponent)

	public:
		Keyboard(Game& game, LPDIRECTINPUT8 directInput);
		~Keyboard();

		const byte* const CurrentState() const;
		const byte* const LastState() const;

		virtual void Initialize() override;
		virtual void Update(const GameTime& gameTime) override;

		bool IsKeyUp(byte key) const;
		bool IsKeyDown(byte key) const;
		bool WasKeyUp(byte key) const;
		bool WasKeyDown(byte key) const;
		bool WasKeyPressedThisFrame(byte key) const;
		bool WasKeyReleasedThisFrame(byte key) const;
		bool IsKeyHeldDown(byte key) const;

	private:
		Keyboard();

		static const int KeyCount = 256;

		Keyboard(const Keyboard& rhs);

		LPDIRECTINPUT8 mDirectInput;
		LPDIRECTINPUTDEVICE8 mDevice;
		byte mCurrentState[KeyCount];
		byte mLastState[KeyCount];
	};
}
