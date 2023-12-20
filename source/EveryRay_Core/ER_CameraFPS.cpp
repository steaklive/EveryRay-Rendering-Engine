#include "ER_CameraFPS.h"
#include "ER_Core.h"
#include "ER_CoreTime.h"
#include "ER_Keyboard.h"
#include "ER_Gamepad.h"
#include "ER_Mouse.h"
#include "ER_VectorHelper.h"

namespace EveryRay_Core
{
	RTTI_DEFINITIONS(ER_CameraFPS)

	const float ER_CameraFPS::DefaultRotationRate = XMConvertToRadians(1.0f);
	const float ER_CameraFPS::DefaultMovementRate = 10.0f;
	const float ER_CameraFPS::DefaultMouseSensitivity = 50.0f;
	const float ER_CameraFPS::DefaultGamepadSensitivity = 0.005f;

	ER_CameraFPS::ER_CameraFPS(ER_Core& game)
		: ER_Camera(game), mKeyboard(nullptr), mMouse(nullptr),
		mMouseSensitivity(DefaultMouseSensitivity), mRotationRate(DefaultRotationRate), mMovementRate(DefaultMovementRate),
		mGamepad(nullptr), mGamepadSensitivity(DefaultGamepadSensitivity)
	{
	}

	ER_CameraFPS::ER_CameraFPS(ER_Core& game, float fieldOfView, float aspectRatio, float nearPlaneDistance, float farPlaneDistance)
		: ER_Camera(game, fieldOfView, aspectRatio, nearPlaneDistance, farPlaneDistance), mKeyboard(nullptr), mMouse(nullptr),
		mMouseSensitivity(DefaultMouseSensitivity), mRotationRate(DefaultRotationRate), mMovementRate(DefaultMovementRate),
		mGamepad(nullptr), mGamepadSensitivity(DefaultGamepadSensitivity)
	{
	}

	ER_CameraFPS::~ER_CameraFPS()
	{
	}

	float&ER_CameraFPS::MouseSensitivity()
	{
		return mMouseSensitivity;
	}


	float& ER_CameraFPS::RotationRate()
	{
		return mRotationRate;
	}

	float& ER_CameraFPS::MovementRate()
	{
		return mMovementRate;
	}

	void ER_CameraFPS::SetMovementRate(float value)
	{
		mMovementRate = value;
	}

	void ER_CameraFPS::Initialize()
	{
		mKeyboard = (ER_Keyboard*)mCore->GetServices().FindService(ER_Keyboard::TypeIdClass());
		mMouse = (ER_Mouse*)mCore->GetServices().FindService(ER_Mouse::TypeIdClass());
		mGamepad = (ER_Gamepad*)mCore->GetServices().FindService(ER_Gamepad::TypeIdClass());

		ER_Camera::Initialize();
	}

	void ER_CameraFPS::Update(const ER_CoreTime& gameTime)
	{
		XMFLOAT3 movementAmount = ER_Vector3Helper::Zero;
		if (mKeyboard)
		{
			if (mKeyboard->IsKeyDown(DIK_W))
			{
				movementAmount.y = 1.0f;
			}

			if (mKeyboard->IsKeyDown(DIK_S) )
			{
				movementAmount.y = -1.0f;
			}

			if (mKeyboard->IsKeyDown(DIK_A))
			{
				movementAmount.x = -1.0f;
			}

			if (mKeyboard->IsKeyDown(DIK_D))
			{
				movementAmount.x = 1.0f;
			}

			if (mKeyboard->IsKeyDown(DIK_E))
			{
				movementAmount.z = 1.0f;
			}

			if (mKeyboard->IsKeyDown(DIK_Q))
			{
				movementAmount.z = -1.0f;
			}
		}
		if (mGamepad)
		{
			if (mGamepad->GetTriggersMaxValue() > 0 && (mGamepad->GetLeftThumbX() != 0 || mGamepad->GetLeftThumbY() != 0))
			{
				float speed = mGamepadSensitivity * mGamepadMovementSpeedFactor;

				if (mGamepad->GetLeftTriggerValue() > 0 && mGamepad->GetRightTriggerValue() == 0)
					speed *= ER_Lerp(mGamepadMovementSlowdownFactor, 1.0f, (1.0f - static_cast<float>(mGamepad->GetLeftTriggerValue()) / static_cast<float>(mGamepad->GetTriggersMaxValue())));
				else if (mGamepad->GetRightTriggerValue() > 0 && mGamepad->GetLeftTriggerValue() == 0)
					speed *= mGamepadMovementSpeedupFactor * (static_cast<float>(mGamepad->GetRightTriggerValue()) / static_cast<float>(mGamepad->GetTriggersMaxValue()));

				movementAmount.x = mGamepad->GetLeftThumbX() * speed;
				movementAmount.y = mGamepad->GetLeftThumbY() * speed;
			}
		}

		ImGuiIO& io = ImGui::GetIO(); (void)io;

		XMFLOAT2 rotationAmount = ER_Vector2Helper::Zero;
		if (mMouse && mMouse->IsButtonHeldDown(MouseButtonsRight) && !io.WantCaptureMouse)
		{
			LPDIMOUSESTATE mouseState = mMouse->CurrentState();
			rotationAmount.x = -mouseState->lX * mMouseSensitivity;
			rotationAmount.y = -mouseState->lY * mMouseSensitivity;
		}
		if (mGamepad && (mGamepad->GetRightThumbX() != 0 || mGamepad->GetRightThumbY() != 0))
		{
			rotationAmount.x = -mGamepad->GetRightThumbX() * mGamepadSensitivity;
			rotationAmount.y = mGamepad->GetRightThumbY() * mGamepadSensitivity;
		}

		float elapsedTime = (float)gameTime.ElapsedCoreTime();
		XMVECTOR rotationVector = XMLoadFloat2(&rotationAmount) * mRotationRate * elapsedTime;
		XMVECTOR right = XMLoadFloat3(&mRight);

		XMMATRIX pitchMatrix = XMMatrixRotationAxis(right, XMVectorGetY(rotationVector));
		XMMATRIX yawMatrix = XMMatrixRotationY(XMVectorGetX(rotationVector));

		ApplyRotation(XMMatrixMultiply(pitchMatrix, yawMatrix));

		XMVECTOR position = XMLoadFloat3(&mPosition);
		XMVECTOR movement = XMLoadFloat3(&movementAmount) * mMovementRate * elapsedTime;

		XMVECTOR strafe = right * XMVectorGetX(movement);
		position += strafe;

		XMVECTOR forward = XMLoadFloat3(&mDirection) * XMVectorGetY(movement);
		position += forward;

		XMVECTOR downup = XMLoadFloat3(&mUp) * XMVectorGetZ(movement);
		position += downup;

		XMStoreFloat3(&mPosition, position);

		ER_Camera::Update(gameTime);
	}
}