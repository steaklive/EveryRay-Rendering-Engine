#pragma once

#include "Common.h"
#include "ER_CoreComponent.h"
#include "ER_DebugProxyObject.h"

namespace EveryRay_Core
{
	class ER_Wind : public ER_CoreComponent
	{
		RTTI_DECLARATIONS(ER_Wind, ER_CoreComponent)
	public:
		ER_Wind(ER_Core& game);
		ER_Wind(ER_Core& game, ER_Camera& camera);
		virtual ~ER_Wind();

		const XMFLOAT3& Direction() const;
		const XMFLOAT3& Up() const;
		const XMFLOAT3& Right() const;

		XMVECTOR DirectionVector() const;
		XMVECTOR UpVector() const;
		XMVECTOR RightVector() const;

		const XMMATRIX& LightMatrix(const XMFLOAT3& position) const;
		const XMMATRIX& GetTransform() const { return mTransformMatrix; }

		void ApplyRotation(CXMMATRIX transform);
		void ApplyRotation(const XMFLOAT4X4& transform);
		void ApplyTransform(const float* transform);
		void DrawProxyModel(ER_RHI_GPUTexture* aRenderTarget, ER_RHI_GPUTexture* aDepth, const ER_CoreTime& time, ER_RHI_GPURootSignature* rs);
		void UpdateProxyModel(const ER_CoreTime& time, const XMFLOAT4X4& viewMatrix, const XMFLOAT4X4& projectionMatrix);
		void UpdateGizmoTransform(const float* cameraView, float* cameraProjection, float* matrix);

		void SetFrequency(float aValue) { mWindFrequency = aValue; }
		float GetFrequency() const { return mWindFrequency; }

		void SetGustDistance(float aValue) { mWindGustDistance = aValue; }
		float GetGustDistance() const { return mWindGustDistance; }

		void SetStrength(float aValue) { mWindStrength = aValue; }
		float GetStrength() const { return mWindStrength; }

	protected:
		XMFLOAT3 mDirection;
		XMFLOAT3 mUp;
		XMFLOAT3 mRight;

	private:
		void UpdateTransformArray(CXMMATRIX transform);

		ER_DebugProxyObject* mProxyModel = nullptr;

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

		float mProxyModelGizmoTranslationDelta = 10.0f;

		XMMATRIX mTransformMatrix = XMMatrixIdentity();

		float mWindStrength = 1.0f;
		float mWindGustDistance = 1.0f;
		float mWindFrequency = 1.0f;
	};
}
