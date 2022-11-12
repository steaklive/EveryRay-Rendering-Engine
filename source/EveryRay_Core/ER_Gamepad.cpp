#include "stdafx.h"

#include "ER_Gamepad.h"
#include "ER_Core.h"
#include "ER_CoreTime.h"
#include "ER_CoreException.h"

#define INPUT_DEADZONE  ( 0.24f * FLOAT(0x7FFF) )  // Default to 24% of the +/- 32767 range.   This is a reasonable default value but can be altered if needed.

namespace EveryRay_Core
{
	RTTI_DEFINITIONS(ER_Gamepad)

	ER_Gamepad::ER_Gamepad(ER_Core& game)
		: ER_CoreComponent(game)
	{
#ifdef USE_XINPUT
		ZeroMemory(&mCurrentXInputControllerState, sizeof(XINPUT_CONTROLLER_STATE));
#endif
	}

	ER_Gamepad::~ER_Gamepad()
	{
	}

	void ER_Gamepad::Initialize()
	{
	}

	void ER_Gamepad::Update(const ER_CoreTime& gameTime)
	{
#ifdef USE_XINPUT
		mLastXInputControllerState = mCurrentXInputControllerState;
		mLastXInputControllerButtons = mCurrentXInputControllerButtons;

		DWORD dwResult;
		dwResult = XInputGetState(0, &mCurrentXInputControllerState.state);

		if (dwResult == ERROR_SUCCESS)
			mCurrentXInputControllerState.bConnected = true;
		else
			mCurrentXInputControllerState.bConnected = false;

		if (mCurrentXInputControllerState.bConnected)
		{
			mCurrentXInputControllerButtons = mCurrentXInputControllerState.state.Gamepad.wButtons;

			// Zero value if thumbsticks are within the dead zone 
			if (mCurrentXInputControllerState.state.Gamepad.sThumbLX < INPUT_DEADZONE &&
				mCurrentXInputControllerState.state.Gamepad.sThumbLX > -INPUT_DEADZONE &&
				mCurrentXInputControllerState.state.Gamepad.sThumbLY < INPUT_DEADZONE &&
				mCurrentXInputControllerState.state.Gamepad.sThumbLY > -INPUT_DEADZONE)
			{
				mCurrentXInputControllerState.state.Gamepad.sThumbLX = 0;
				mCurrentXInputControllerState.state.Gamepad.sThumbLY = 0;
			}

			if (mCurrentXInputControllerState.state.Gamepad.sThumbRX < INPUT_DEADZONE &&
				mCurrentXInputControllerState.state.Gamepad.sThumbRX > -INPUT_DEADZONE &&
				mCurrentXInputControllerState.state.Gamepad.sThumbRY < INPUT_DEADZONE &&
				mCurrentXInputControllerState.state.Gamepad.sThumbRY > -INPUT_DEADZONE)
			{
				mCurrentXInputControllerState.state.Gamepad.sThumbRX = 0;
				mCurrentXInputControllerState.state.Gamepad.sThumbRY = 0;
			}
		}
#endif
	}

	SHORT ER_Gamepad::GetLeftThumbX() const
	{
#ifdef USE_XINPUT
		if (!mCurrentXInputControllerState.bConnected)
			return 0;

		return mCurrentXInputControllerState.state.Gamepad.sThumbLX;
#endif
		return 0;
	}

	SHORT ER_Gamepad::GetLeftThumbY() const
	{
#ifdef USE_XINPUT
		if (!mCurrentXInputControllerState.bConnected)
			return 0;

		return mCurrentXInputControllerState.state.Gamepad.sThumbLY;
#endif

		return 0;
	}

	SHORT ER_Gamepad::GetRightThumbX() const
	{
#ifdef USE_XINPUT
		if (!mCurrentXInputControllerState.bConnected)
			return 0;

		return mCurrentXInputControllerState.state.Gamepad.sThumbRX;
#endif
		return 0;
	}

	SHORT ER_Gamepad::GetRightThumbY() const
	{
#ifdef USE_XINPUT
		if (!mCurrentXInputControllerState.bConnected)
			return 0;

		return mCurrentXInputControllerState.state.Gamepad.sThumbRY;
#endif

		return 0;
	}

	BYTE ER_Gamepad::GetLeftTriggerValue() const
	{
#ifdef USE_XINPUT
		if (!mCurrentXInputControllerState.bConnected)
			return 0;

		return mCurrentXInputControllerState.state.Gamepad.bLeftTrigger;
#endif

		return 0;
	}

	BYTE ER_Gamepad::GetRightTriggerValue() const
	{
#ifdef USE_XINPUT
		if (!mCurrentXInputControllerState.bConnected)
			return 0;

		return mCurrentXInputControllerState.state.Gamepad.bRightTrigger;
#endif

		return 0;
	}

	BYTE ER_Gamepad::GetTriggersMaxValue() const
	{
#ifdef USE_XINPUT
		if (!mCurrentXInputControllerState.bConnected)
			return 0;

		return 255;
#endif
		return 0;
	}

	bool ER_Gamepad::IsButtonUp(GamepadButtons button) const
	{
#ifdef USE_XINPUT
		if (!mCurrentXInputControllerState.bConnected)
			return false;

		switch (button)
		{
		case GamepadButtons::GamepadDPadUp:
			return !(mCurrentXInputControllerButtons & XINPUT_GAMEPAD_DPAD_UP);
			break;
		case GamepadButtons::GamepadDPadDown:
			return !(mCurrentXInputControllerButtons & XINPUT_GAMEPAD_DPAD_DOWN);
			break;
		case GamepadButtons::GamepadDPadLeft:
			return !(mCurrentXInputControllerButtons & XINPUT_GAMEPAD_DPAD_LEFT);
			break;
		case GamepadButtons::GamepadDPadRight:
			return !(mCurrentXInputControllerButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
			break;
		case GamepadButtons::GamepadStartButton:
			return !(mCurrentXInputControllerButtons & XINPUT_GAMEPAD_START);
			break;
		case GamepadButtons::GamepadBackButton:
			return !(mCurrentXInputControllerButtons & XINPUT_GAMEPAD_BACK);
			break;
		case GamepadButtons::GamepadLThumbClick:
			return !(mCurrentXInputControllerButtons & XINPUT_GAMEPAD_LEFT_THUMB);
			break;
		case GamepadButtons::GamepadRThumbClick:
			return !(mCurrentXInputControllerButtons & XINPUT_GAMEPAD_RIGHT_THUMB);
			break;
		case GamepadButtons::GamepadLShoulder:
			return !(mCurrentXInputControllerButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
			break;
		case GamepadButtons::GamepadRShoulder:
			return !(mCurrentXInputControllerButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
			break;
		case GamepadButtons::GamepadAButton:
			return !(mCurrentXInputControllerButtons & XINPUT_GAMEPAD_A);
			break;
		case GamepadButtons::GamepadBButton:
			return !(mCurrentXInputControllerButtons & XINPUT_GAMEPAD_B);
			break;
		case GamepadButtons::GamepadXButton:
			return !(mCurrentXInputControllerButtons & XINPUT_GAMEPAD_X);
			break;
		case GamepadButtons::GamepadYButton:
			return !(mCurrentXInputControllerButtons & XINPUT_GAMEPAD_Y);
			break;
		default:
			return false;
		}
#endif
		return false;
	}

	bool ER_Gamepad::WasButtonUp(GamepadButtons button) const
	{
#ifdef USE_XINPUT
		if (!mCurrentXInputControllerState.bConnected)
			return false;

		switch (button)
		{
		case GamepadButtons::GamepadDPadUp:
			return !(mLastXInputControllerButtons & XINPUT_GAMEPAD_DPAD_UP);
			break;
		case GamepadButtons::GamepadDPadDown:
			return !(mLastXInputControllerButtons & XINPUT_GAMEPAD_DPAD_DOWN);
			break;
		case GamepadButtons::GamepadDPadLeft:
			return !(mLastXInputControllerButtons & XINPUT_GAMEPAD_DPAD_LEFT);
			break;
		case GamepadButtons::GamepadDPadRight:
			return !(mLastXInputControllerButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
			break;
		case GamepadButtons::GamepadStartButton:
			return !(mLastXInputControllerButtons & XINPUT_GAMEPAD_START);
			break;
		case GamepadButtons::GamepadBackButton:
			return !(mLastXInputControllerButtons & XINPUT_GAMEPAD_BACK);
			break;
		case GamepadButtons::GamepadLThumbClick:
			return !(mLastXInputControllerButtons & XINPUT_GAMEPAD_LEFT_THUMB);
			break;
		case GamepadButtons::GamepadRThumbClick:
			return !(mLastXInputControllerButtons & XINPUT_GAMEPAD_RIGHT_THUMB);
			break;
		case GamepadButtons::GamepadLShoulder:
			return !(mLastXInputControllerButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
			break;
		case GamepadButtons::GamepadRShoulder:
			return !(mLastXInputControllerButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
			break;
		case GamepadButtons::GamepadAButton:
			return !(mLastXInputControllerButtons & XINPUT_GAMEPAD_A);
			break;
		case GamepadButtons::GamepadBButton:
			return !(mLastXInputControllerButtons & XINPUT_GAMEPAD_B);
			break;
		case GamepadButtons::GamepadXButton:
			return !(mLastXInputControllerButtons & XINPUT_GAMEPAD_X);
			break;
		case GamepadButtons::GamepadYButton:
			return !(mLastXInputControllerButtons & XINPUT_GAMEPAD_Y);
			break;
		default:
			return false;
		}
#endif
		return false;
	}

	bool ER_Gamepad::IsButtonDown(GamepadButtons button) const
	{
#ifdef USE_XINPUT
		if (!mCurrentXInputControllerState.bConnected)
			return false;

		switch (button)
		{
		case GamepadButtons::GamepadDPadUp:
			return mCurrentXInputControllerButtons & XINPUT_GAMEPAD_DPAD_UP;
			break;	
		case GamepadButtons::GamepadDPadDown:
			return mCurrentXInputControllerButtons & XINPUT_GAMEPAD_DPAD_DOWN;
			break;	
		case GamepadButtons::GamepadDPadLeft:
			return mCurrentXInputControllerButtons & XINPUT_GAMEPAD_DPAD_LEFT;
			break;
		case GamepadButtons::GamepadDPadRight:
			return mCurrentXInputControllerButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
			break;	
		case GamepadButtons::GamepadStartButton:
			return mCurrentXInputControllerButtons & XINPUT_GAMEPAD_START;
			break;
		case GamepadButtons::GamepadBackButton:
			return mCurrentXInputControllerButtons & XINPUT_GAMEPAD_BACK;
			break;
		case GamepadButtons::GamepadLThumbClick:
			return mCurrentXInputControllerButtons & XINPUT_GAMEPAD_LEFT_THUMB;
			break;
		case GamepadButtons::GamepadRThumbClick:
			return mCurrentXInputControllerButtons & XINPUT_GAMEPAD_RIGHT_THUMB;
			break;
		case GamepadButtons::GamepadLShoulder:
			return mCurrentXInputControllerButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
			break;
		case GamepadButtons::GamepadRShoulder:
			return mCurrentXInputControllerButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
			break;
		case GamepadButtons::GamepadAButton:
			return mCurrentXInputControllerButtons & XINPUT_GAMEPAD_A;
			break;
		case GamepadButtons::GamepadBButton:
			return mCurrentXInputControllerButtons & XINPUT_GAMEPAD_B;
			break;
		case GamepadButtons::GamepadXButton:
			return mCurrentXInputControllerButtons & XINPUT_GAMEPAD_X;
			break;
		case GamepadButtons::GamepadYButton:
			return mCurrentXInputControllerButtons & XINPUT_GAMEPAD_Y;
			break;
		default:
			return false;
		}
#endif
		return false;
	}

	bool ER_Gamepad::WasButtonDown(GamepadButtons button) const
	{
#ifdef USE_XINPUT
		if (!mCurrentXInputControllerState.bConnected)
			return false;

		switch (button)
		{
		case GamepadButtons::GamepadDPadUp:
			return mLastXInputControllerButtons & XINPUT_GAMEPAD_DPAD_UP;
			break;
		case GamepadButtons::GamepadDPadDown:
			return mLastXInputControllerButtons & XINPUT_GAMEPAD_DPAD_DOWN;
			break;
		case GamepadButtons::GamepadDPadLeft:
			return mLastXInputControllerButtons & XINPUT_GAMEPAD_DPAD_LEFT;
			break;
		case GamepadButtons::GamepadDPadRight:
			return mLastXInputControllerButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
			break;
		case GamepadButtons::GamepadStartButton:
			return mLastXInputControllerButtons & XINPUT_GAMEPAD_START;
			break;
		case GamepadButtons::GamepadBackButton:
			return mLastXInputControllerButtons & XINPUT_GAMEPAD_BACK;
			break;
		case GamepadButtons::GamepadLThumbClick:
			return mLastXInputControllerButtons & XINPUT_GAMEPAD_LEFT_THUMB;
			break;
		case GamepadButtons::GamepadRThumbClick:
			return mLastXInputControllerButtons & XINPUT_GAMEPAD_RIGHT_THUMB;
			break;
		case GamepadButtons::GamepadLShoulder:
			return mLastXInputControllerButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
			break;
		case GamepadButtons::GamepadRShoulder:
			return mLastXInputControllerButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
			break;
		case GamepadButtons::GamepadAButton:
			return mLastXInputControllerButtons & XINPUT_GAMEPAD_A;
			break;
		case GamepadButtons::GamepadBButton:
			return mLastXInputControllerButtons & XINPUT_GAMEPAD_B;
			break;
		case GamepadButtons::GamepadXButton:
			return mLastXInputControllerButtons & XINPUT_GAMEPAD_X;
			break;
		case GamepadButtons::GamepadYButton:
			return mLastXInputControllerButtons & XINPUT_GAMEPAD_Y;
			break;
		default:
			return false;
		}
#endif
		return false;
	}

	bool ER_Gamepad::WasButtonPressedThisFrame(GamepadButtons button) const
	{
		return (IsButtonDown(button) && WasButtonUp(button));
	}

	bool ER_Gamepad::WasButtonReleasedThisFrame(GamepadButtons button) const
	{
		return (IsButtonUp(button) && WasButtonDown(button));
	}
}