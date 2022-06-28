#pragma once

#include "GameComponent.h"

namespace Library
{
	class GameTime;

	enum MouseButtons
	{
		MouseButtonsLeft = 0,
		MouseButtonsRight = 1,
		MouseButtonsMiddle = 2,
		MouseButtonsX1 = 3
	};

	class ER_Mouse : public GameComponent
	{
		RTTI_DECLARATIONS(ER_Mouse, GameComponent)

	public:
		ER_Mouse(Game& game, LPDIRECTINPUT8 directInput);
		~ER_Mouse();

		LPDIMOUSESTATE CurrentState();
		LPDIMOUSESTATE LastState();

		virtual void Initialize() override;
		virtual void Update(const GameTime& gameTime) override;

		long X() const;
		long Y() const;
		long Wheel() const;

		bool IsButtonUp(MouseButtons button) const;
		bool IsButtonDown(MouseButtons button) const;
		bool WasButtonUp(MouseButtons button) const;
		bool WasButtonDown(MouseButtons button) const;
		bool WasButtonPressedThisFrame(MouseButtons button) const;
		bool WasButtonReleasedThisFrame(MouseButtons button) const;
		bool IsButtonHeldDown(MouseButtons button) const;

	private:
		ER_Mouse();

		LPDIRECTINPUT8 mDirectInput;
		LPDIRECTINPUTDEVICE8 mDevice;
		DIMOUSESTATE mCurrentState;
		DIMOUSESTATE mLastState;

		long mX;
		long mY;
		long mWheel;
	};
}