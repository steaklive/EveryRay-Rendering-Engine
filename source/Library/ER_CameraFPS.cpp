#include "stdafx.h"

#include "ER_CameraFPS.h"
#include "Game.h"
#include "GameTime.h"
#include "ER_Keyboard.h"
#include "Mouse.h"
#include "VectorHelper.h"

#include "imgui.h"

namespace Library
{
	RTTI_DEFINITIONS(ER_CameraFPS)

	const float ER_CameraFPS::DefaultRotationRate = XMConvertToRadians(1.0f);
	const float ER_CameraFPS::DefaultMovementRate = 10.0f;
	const float ER_CameraFPS::DefaultMouseSensitivity = 50.0f;

	ER_CameraFPS::ER_CameraFPS(Game& game)
		: ER_Camera(game), mKeyboard(nullptr), mMouse(nullptr),
		mMouseSensitivity(DefaultMouseSensitivity), mRotationRate(DefaultRotationRate), mMovementRate(DefaultMovementRate)
	{
	}

	ER_CameraFPS::ER_CameraFPS(Game& game, float fieldOfView, float aspectRatio, float nearPlaneDistance, float farPlaneDistance)
		: ER_Camera(game, fieldOfView, aspectRatio, nearPlaneDistance, farPlaneDistance), mKeyboard(nullptr), mMouse(nullptr),
		mMouseSensitivity(DefaultMouseSensitivity), mRotationRate(DefaultRotationRate), mMovementRate(DefaultMovementRate)

	{
	}

	ER_CameraFPS::~ER_CameraFPS()
	{
		mKeyboard = nullptr;
		mMouse = nullptr;
	}

	const ER_Keyboard& ER_CameraFPS::GetKeyboard() const
	{
		return *mKeyboard;
	}

	void ER_CameraFPS::SetKeyboard(ER_Keyboard& keyboard)
	{
		mKeyboard = &keyboard;
	}

	const Mouse& ER_CameraFPS::GetMouse() const
	{
		return *mMouse;
	}

	void ER_CameraFPS::SetMouse(Mouse& mouse)
	{
		mMouse = &mouse;
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
		mKeyboard = (ER_Keyboard*)mGame->Services().GetService(ER_Keyboard::TypeIdClass());
		mMouse = (Mouse*)mGame->Services().GetService(Mouse::TypeIdClass());

		ER_Camera::Initialize();
	}

	void ER_CameraFPS::Update(const GameTime& gameTime)
	{
		XMFLOAT3 movementAmount = Vector3Helper::Zero;
		if (mKeyboard != nullptr )
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

		ImGuiIO& io = ImGui::GetIO(); (void)io;

		XMFLOAT2 rotationAmount = Vector2Helper::Zero;
		if ((mMouse != nullptr) && (mMouse->IsButtonHeldDown(MouseButtonsRight)) && (!io.WantCaptureMouse))
		{
			LPDIMOUSESTATE mouseState = mMouse->CurrentState();
			rotationAmount.x = -mouseState->lX * mMouseSensitivity;
			rotationAmount.y = -mouseState->lY * mMouseSensitivity;
		}

		float elapsedTime = (float)gameTime.ElapsedGameTime();
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