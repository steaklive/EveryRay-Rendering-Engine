#pragma once

#include "ER_CoreComponent.h"

namespace EveryRay_Core
{
	class ER_CoreTime;

	enum GamepadButtons
	{
		GamepadDPadUp = 0,
		GamepadDPadDown,
		GamepadDPadLeft,
		GamepadDPadRight,
		GamepadStartButton,
		GamepadBackButton,
		GamepadLThumbClick,
		GamepadRThumbClick,
		GamepadLShoulder,
		GamepadRShoulder,
		GamepadAButton,
		GamepadBButton,
		GamepadXButton,
		GamepadYButton
	};

#ifdef USE_XINPUT
	struct XINPUT_CONTROLLER_STATE
	{
		XINPUT_STATE state;
		bool bConnected;
	};
#endif

	class ER_Gamepad : public ER_CoreComponent
	{
		RTTI_DECLARATIONS(ER_Gamepad, ER_CoreComponent)

	public:
		ER_Gamepad(ER_Core& game);
		~ER_Gamepad();

		virtual void Initialize() override;
		virtual void Update(const ER_CoreTime& gameTime) override;

		SHORT GetLeftThumbX() const;
		SHORT GetLeftThumbY() const;
		SHORT GetRightThumbX() const;
		SHORT GetRightThumbY() const;
		BYTE GetLeftTriggerValue() const;
		BYTE GetRightTriggerValue() const;
		BYTE GetTriggersMaxValue() const;

		bool IsButtonDown(GamepadButtons button) const;
		bool IsButtonUp(GamepadButtons button) const;
		bool WasButtonDown(GamepadButtons button) const;
		bool WasButtonUp(GamepadButtons button) const;
		bool WasButtonPressedThisFrame(GamepadButtons button) const;
		bool WasButtonReleasedThisFrame(GamepadButtons button) const;

	private:
#ifdef USE_XINPUT
		XINPUT_CONTROLLER_STATE mCurrentXInputControllerState;
		XINPUT_CONTROLLER_STATE mLastXInputControllerState;
		WORD mCurrentXInputControllerButtons;
		WORD mLastXInputControllerButtons;
#endif
	};
}