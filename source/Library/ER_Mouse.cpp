#include "stdafx.h"

#include "ER_Mouse.h"
#include "Game.h"
#include "GameTime.h"
#include "GameException.h"

namespace Library
{
	RTTI_DEFINITIONS(ER_Mouse)

	ER_Mouse::ER_Mouse(Game& game, LPDIRECTINPUT8 directInput)
		: ER_CoreComponent(game), mDirectInput(directInput), mDevice(nullptr), mX(0), mY(0), mWheel(0)
	{
		assert(mDirectInput != nullptr);
		ZeroMemory(&mCurrentState, sizeof(mCurrentState));
		ZeroMemory(&mLastState, sizeof(mLastState));
	}

	ER_Mouse::~ER_Mouse()
	{
		if (mDevice != nullptr)
		{
			mDevice->Unacquire();
			mDevice->Release();
			mDevice = nullptr;
		}
	}

	LPDIMOUSESTATE ER_Mouse::CurrentState()
	{
		return &mCurrentState;
	}

	LPDIMOUSESTATE ER_Mouse::LastState()
	{
		return &mLastState;
	}

	long ER_Mouse::X() const
	{
		return mX;
	}

	long ER_Mouse::Y() const
	{
		return mY;
	}

	long ER_Mouse::Wheel() const
	{
		return mWheel;
	}

	void ER_Mouse::Initialize()
	{
		if (FAILED(mDirectInput->CreateDevice(GUID_SysMouse, &mDevice, nullptr)))
		{
			throw GameException("IDIRECTINPUT8::CreateDevice() failed");
		}

		if (FAILED(mDevice->SetDataFormat(&c_dfDIMouse)))
		{
			throw GameException("IDIRECTINPUTDEVICE8::SetDataFormat() failed");
		}

		if (FAILED(mDevice->SetCooperativeLevel(mGame->WindowHandle(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE)))
		{
			throw GameException("IDIRECTINPUTDEVICE8::SetCooperativeLevel() failed");
		}

		mDevice->Acquire();
	}

	void ER_Mouse::Update(const GameTime& gameTime)
	{
		if (mDevice != nullptr)
		{
			memcpy(&mLastState, &mCurrentState, sizeof(mCurrentState));

			if (FAILED(mDevice->GetDeviceState(sizeof(mCurrentState), &mCurrentState)))
			{
				// Try to reaqcuire the device
				if (SUCCEEDED(mDevice->Acquire()))
				{
					if (FAILED(mDevice->GetDeviceState(sizeof(mCurrentState), &mCurrentState)))
					{
						return;
					}
				}
			}

			// Accumulate positions
			mX += mCurrentState.lX;
			mY += mCurrentState.lY;
			mWheel += mCurrentState.lZ;
		}
	}

	bool ER_Mouse::IsButtonUp(MouseButtons button) const
	{
		return ((mCurrentState.rgbButtons[button] & 0x80) == 0);
	}

	bool ER_Mouse::IsButtonDown(MouseButtons button) const
	{
		return ((mCurrentState.rgbButtons[button] & 0x80) != 0);
	}

	bool ER_Mouse::WasButtonUp(MouseButtons button) const
	{
		return ((mLastState.rgbButtons[button] & 0x80) == 0);
	}

	bool ER_Mouse::WasButtonDown(MouseButtons button) const
	{
		return ((mLastState.rgbButtons[button] & 0x80) != 0);
	}

	bool ER_Mouse::WasButtonPressedThisFrame(MouseButtons button) const
	{
		return (IsButtonDown(button) && WasButtonUp(button));
	}

	bool ER_Mouse::WasButtonReleasedThisFrame(MouseButtons button) const
	{
		return (IsButtonUp(button) && WasButtonDown(button));
	}

	bool ER_Mouse::IsButtonHeldDown(MouseButtons button) const
	{
		return (IsButtonDown(button) && WasButtonDown(button));
	}
}