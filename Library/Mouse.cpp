#include "stdafx.h"

#include "Mouse.h"
#include "Game.h"
#include "GameTime.h"
#include "GameException.h"

namespace Library
{
	RTTI_DEFINITIONS(Mouse)

		Mouse::Mouse(Game& game, LPDIRECTINPUT8 directInput)
		: GameComponent(game), mDirectInput(directInput), mDevice(nullptr), mX(0), mY(0), mWheel(0)
	{
		assert(mDirectInput != nullptr);
		ZeroMemory(&mCurrentState, sizeof(mCurrentState));
		ZeroMemory(&mLastState, sizeof(mLastState));
	}

	Mouse::~Mouse()
	{
		if (mDevice != nullptr)
		{
			mDevice->Unacquire();
			mDevice->Release();
			mDevice = nullptr;
		}
	}

	LPDIMOUSESTATE Mouse::CurrentState()
	{
		return &mCurrentState;
	}

	LPDIMOUSESTATE Mouse::LastState()
	{
		return &mLastState;
	}

	long Mouse::X() const
	{
		return mX;
	}

	long Mouse::Y() const
	{
		return mY;
	}

	long Mouse::Wheel() const
	{
		return mWheel;
	}

	void Mouse::Initialize()
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

	void Mouse::Update(const GameTime& gameTime)
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

	bool Mouse::IsButtonUp(MouseButtons button) const
	{
		return ((mCurrentState.rgbButtons[button] & 0x80) == 0);
	}

	bool Mouse::IsButtonDown(MouseButtons button) const
	{
		return ((mCurrentState.rgbButtons[button] & 0x80) != 0);
	}

	bool Mouse::WasButtonUp(MouseButtons button) const
	{
		return ((mLastState.rgbButtons[button] & 0x80) == 0);
	}

	bool Mouse::WasButtonDown(MouseButtons button) const
	{
		return ((mLastState.rgbButtons[button] & 0x80) != 0);
	}

	bool Mouse::WasButtonPressedThisFrame(MouseButtons button) const
	{
		return (IsButtonDown(button) && WasButtonUp(button));
	}

	bool Mouse::WasButtonReleasedThisFrame(MouseButtons button) const
	{
		return (IsButtonUp(button) && WasButtonDown(button));
	}

	bool Mouse::IsButtonHeldDown(MouseButtons button) const
	{
		return (IsButtonDown(button) && WasButtonDown(button));
	}
}