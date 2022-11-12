#pragma once

#include "ER_Camera.h"

namespace EveryRay_Core
{
	class ER_Keyboard;
	class ER_Mouse;
	class ER_Gamepad;

	class ER_CameraFPS : public ER_Camera
	{
		RTTI_DECLARATIONS(ER_CameraFPS, ER_Camera)

	public:
		ER_CameraFPS(ER_Core& game);
		ER_CameraFPS(ER_Core& game, float fieldOfView, float aspectRatio, float nearPlaneDistance, float farPlaneDistance);

		virtual ~ER_CameraFPS();

		float& MouseSensitivity();
		float& RotationRate();
		float& MovementRate();
		void SetMovementRate(float value);

		virtual void Initialize() override;
		virtual void Update(const ER_CoreTime& gameTime) override;

		static const float DefaultMouseSensitivity;
		static const float DefaultGamepadSensitivity;
		static const float DefaultRotationRate;
		static const float DefaultMovementRate;

	protected:
		float mMouseSensitivity;
		float mGamepadSensitivity;
		float mGamepadMovementSpeedFactor = 0.01f;
		float mGamepadMovementSlowdownFactor = 0.1f;
		float mGamepadMovementSpeedupFactor = 10.0f;
		float mRotationRate;
		float mMovementRate;

		ER_Gamepad* mGamepad = nullptr;
		ER_Keyboard* mKeyboard = nullptr;
		ER_Mouse* mMouse = nullptr;

	private:
		ER_CameraFPS(const ER_CameraFPS& rhs);
		ER_CameraFPS& operator=(const ER_CameraFPS& rhs);
	};
}
