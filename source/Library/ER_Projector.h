#pragma once
#include "Common.h"

namespace Library
{
	class Game;
	class ER_CoreTime;

	class ER_Projector
	{
	public:
		ER_Projector(Game& game);
		ER_Projector(Game& game, float fieldOfView, float aspectRatio, float nearPlaneDistance, float farPlaneDistance);

		~ER_Projector();

		const XMFLOAT3& Position() const;
		const XMFLOAT3& Direction() const;
		const XMFLOAT3& Up() const;
		const XMFLOAT3& Right() const;

		XMVECTOR PositionVector() const;
		XMVECTOR DirectionVector() const;
		XMVECTOR UpVector() const;
		XMVECTOR RightVector() const;

		float AspectRatio() const;
		float FieldOfView() const;
		float NearPlaneDistance() const;
		float FarPlaneDistance() const;

		XMMATRIX ViewMatrix() const;
		XMMATRIX ProjectionMatrix() const;
		XMMATRIX ViewProjectionMatrix() const;

		void SetPosition(FLOAT x, FLOAT y, FLOAT z);
		void SetPosition(FXMVECTOR position);
		void SetPosition(const XMFLOAT3& position);

		void Reset();
		void Initialize();
		void Update();
		void UpdateViewMatrix();
		void SetViewMatrix(XMFLOAT3 pos, XMFLOAT3 dir, XMFLOAT3 up);
		void UpdateProjectionMatrix();
		void SetProjectionMatrix(CXMMATRIX matrix);
		void ApplyRotation(CXMMATRIX transform);
		void ApplyRotation(const XMFLOAT4X4& transform);
		void ApplyTransform(CXMMATRIX transform);

		static const float DefaultFieldOfView;
		static const float DefaultAspectRatio;
		static const float DefaultNearPlaneDistance;
		static const float DefaultFarPlaneDistance;

		//ortho
		static const float DefaultScreenHeight;
		static const float DefaultScreenWidth;

	private:
		float mFieldOfView;
		float mAspectRatio;
		float mNearPlaneDistance;
		float mFarPlaneDistance;

		XMFLOAT3 mPosition;
		XMFLOAT3 mDirection;
		XMFLOAT3 mUp;
		XMFLOAT3 mRight;

		XMFLOAT4X4 mViewMatrix;
		XMFLOAT4X4 mProjectionMatrix;
	};
}