#pragma once

#include "ER_Camera.h"

namespace Library
{
	class ER_Keyboard;
	class ER_Mouse;

	class ER_CameraFPS : public ER_Camera
	{
		RTTI_DECLARATIONS(ER_CameraFPS, ER_Camera)

	public:
		ER_CameraFPS(Game& game);
		ER_CameraFPS(Game& game, float fieldOfView, float aspectRatio, float nearPlaneDistance, float farPlaneDistance);

		virtual ~ER_CameraFPS();

		const ER_Keyboard& GetKeyboard() const;
		void SetKeyboard(ER_Keyboard& keyboard);

		const ER_Mouse& GetMouse() const;
		void SetMouse(ER_Mouse& mouse);

		float& MouseSensitivity();
		float& RotationRate();
		float& MovementRate();
		void SetMovementRate(float value);


		virtual void Initialize() override;
		virtual void Update(const ER_CoreTime& gameTime) override;

		static const float DefaultMouseSensitivity;
		static const float DefaultRotationRate;
		static const float DefaultMovementRate;

	protected:
		float mMouseSensitivity;
		float mRotationRate;
		float mMovementRate;

		ER_Keyboard* mKeyboard;
		ER_Mouse* mMouse;

	private:
		ER_CameraFPS(const ER_CameraFPS& rhs);
		ER_CameraFPS& operator=(const ER_CameraFPS& rhs);
	};
}
