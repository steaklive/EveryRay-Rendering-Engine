#include "stdafx.h"

#include "ER_Keyboard.h"
#include "Game.h"
#include "GameTime.h"
#include "GameException.h"

namespace Library
{
	RTTI_DEFINITIONS(ER_Keyboard)

	ER_Keyboard::ER_Keyboard(Game& game, LPDIRECTINPUT8 directInput)
		: GameComponent(game), mDirectInput(directInput), mDevice(nullptr)
	{
		assert(mDirectInput != nullptr);
		ZeroMemory(mCurrentState, sizeof(mCurrentState));
		ZeroMemory(mLastState, sizeof(mLastState));
	}

	ER_Keyboard::~ER_Keyboard()
	{
		if (mDevice != nullptr)
		{
			mDevice->Unacquire();
			mDevice->Release();
			mDevice = nullptr;
		}
	}

	const byte* const ER_Keyboard::CurrentState() const
	{
		return mCurrentState;
	}

	const byte* const ER_Keyboard::LastState() const
	{
		return mLastState;
	}

	void ER_Keyboard::Initialize()
	{
		if (FAILED(mDirectInput->CreateDevice(GUID_SysKeyboard, &mDevice, nullptr)))
		{
			throw GameException("IDIRECTINPUT8::CreateDevice() failed");
		}

		if (FAILED(mDevice->SetDataFormat(&c_dfDIKeyboard)))
		{
			throw GameException("IDIRECTINPUTDEVICE8::SetDataFormat() failed");
		}

		if (FAILED(mDevice->SetCooperativeLevel(mGame->WindowHandle(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE)))
		{
			throw GameException("IDIRECTINPUTDEVICE8::SetCooperativeLevel() failed");
		}

		mDevice->Acquire();
	}

	void ER_Keyboard::Update(const GameTime& gameTime)
	{
		if (mDevice != nullptr)
		{
			memcpy(mLastState, mCurrentState, sizeof(mCurrentState));

			if (FAILED(mDevice->GetDeviceState(sizeof(mCurrentState), (LPVOID)mCurrentState)))
			{
				// Try to reaqcuire the device
				if (SUCCEEDED(mDevice->Acquire()))
				{
					mDevice->GetDeviceState(sizeof(mCurrentState), (LPVOID)mCurrentState);
				}
			}
		}
	}

	bool ER_Keyboard::IsKeyUp(byte key) const
	{
		return ((mCurrentState[key] & 0x80) == 0);
	}

	bool ER_Keyboard::IsKeyDown(byte key) const
	{
		return ((mCurrentState[key] & 0x80) != 0);
	}

	bool ER_Keyboard::WasKeyUp(byte key) const
	{
		return ((mLastState[key] & 0x80) == 0);
	}

	bool ER_Keyboard::WasKeyDown(byte key) const
	{
		return ((mLastState[key] & 0x80) != 0);
	}

	bool ER_Keyboard::WasKeyPressedThisFrame(byte key) const
	{
		return (IsKeyDown(key) && WasKeyUp(key));
	}

	bool ER_Keyboard::WasKeyReleasedThisFrame(byte key) const
	{
		return (IsKeyUp(key) && WasKeyDown(key));
	}

	bool ER_Keyboard::IsKeyHeldDown(byte key) const
	{
		return (IsKeyDown(key) && WasKeyDown(key));
	}
}