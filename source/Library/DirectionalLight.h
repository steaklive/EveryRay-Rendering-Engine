#pragma once

#include "Common.h"
#include "Light.h"
#include "ProxyModel.h"

namespace Library
{
	class DirectionalLight : public Light
	{
		RTTI_DECLARATIONS(DirectionalLight, Light)

	public:
		DirectionalLight(Game& game);
		DirectionalLight(Game& game, Camera& camera);
		virtual ~DirectionalLight();

		const XMFLOAT3& Direction() const;
		const XMFLOAT3& Up() const;
		const XMFLOAT3& Right() const;

		XMVECTOR DirectionVector() const;
		XMVECTOR UpVector() const;
		XMVECTOR RightVector() const;

		const XMMATRIX& LightMatrix(const XMFLOAT3& position) const;

		void ApplyRotation(CXMMATRIX transform);
		void ApplyRotation(const XMFLOAT4X4& transform);
		void ApplyTransform(const float* transform);
		void DrawProxyModel(const GameTime& time);
		void UpdateProxyModel(const GameTime& time, XMFLOAT4X4 viewMatrix, XMFLOAT4X4 projectionMatrix);
		void UpdateGizmoTransform(const float *cameraView, float *cameraProjection, float* matrix);
		XMFLOAT3 GetDirectionalLightColor() { return XMFLOAT3(mSunColor); }
		XMFLOAT3 GetAmbientLightColor() { return XMFLOAT3(mAmbientColor); }

	protected:
		XMFLOAT3 mDirection;
		XMFLOAT3 mUp;
		XMFLOAT3 mRight;

	private:
		void UpdateTransformArray(CXMMATRIX transform);

		ProxyModel* mProxyModel;

		float mObjectTransformMatrix[16] =
		{
			1.f, 0.f, 0.f, 0.f,
			0.f, 1.f, 0.f, 0.f,
			0.f, 0.f, 1.f, 0.f,
			0.f, 0.f, 0.f, 1.f
		};
		float mMatrixTranslation[3], mMatrixRotation[3], mMatrixScale[3];
		float mCameraViewMatrix[16];
		float mCameraProjectionMatrix[16];

		XMMATRIX mPseudoTranslation = XMMatrixIdentity(); //from proxy model

		float mSunColor[3] = { 1.0f, 0.95f, 0.863f };
		float mAmbientColor[3] = { 0.08f, 0.08f, 0.08f };
	};
}
