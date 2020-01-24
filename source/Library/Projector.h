#pragma once
#include "GameComponent.h"

namespace Library
{
	class GameTime;

	class Projector : public GameComponent
	{
		RTTI_DECLARATIONS(Projector, GameComponent)

	public:
		Projector(Game& game);
		Projector(Game& game, float fieldOfView, float aspectRatio, float nearPlaneDistance, float farPlaneDistance);

		virtual ~Projector();

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

		virtual void SetPosition(FLOAT x, FLOAT y, FLOAT z);
		virtual void SetPosition(FXMVECTOR position);
		virtual void SetPosition(const XMFLOAT3& position);

		virtual void Reset();
		virtual void Initialize() override;
		virtual void Update(const GameTime& gameTime) override;
		virtual void UpdateViewMatrix();
		virtual void UpdateProjectionMatrix();
		virtual void SetProjectionMatrix(CXMMATRIX matrix);
		virtual void ApplyRotation(CXMMATRIX transform);
		virtual void ApplyRotation(const XMFLOAT4X4& transform);

		static const float DefaultFieldOfView;
		static const float DefaultAspectRatio;
		static const float DefaultNearPlaneDistance;
		static const float DefaultFarPlaneDistance;

		//ortho
		static const float DefaultScreenHeight;
		static const float DefaultScreenWidth;

	protected:
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

	private:
		Projector(const Projector& rhs);
		Projector& operator=(const Projector& rhs);
	};
}