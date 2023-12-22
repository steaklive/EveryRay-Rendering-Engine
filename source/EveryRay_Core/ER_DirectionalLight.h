#pragma once

#include "Common.h"
#include "ER_Light.h"
#include "ER_DebugProxyObject.h"
#include "ER_GenericEvent.h"

#include "RHI/ER_RHI.h"

namespace EveryRay_Core
{
	class ER_DirectionalLight : public ER_Light
	{
		RTTI_DECLARATIONS(ER_DirectionalLight, ER_Light)
		
		using Delegate_RotationUpdate = std::function<void()>; 

	public:
		ER_DirectionalLight(ER_Core& game);
		ER_DirectionalLight(ER_Core& game, ER_Camera& camera);
		virtual ~ER_DirectionalLight();

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
		void UpdateGizmoTransform(const float *cameraView, float *cameraProjection, float* matrix);
		
		XMFLOAT3 GetColor() const { return XMFLOAT3(mColor); }
		void SetColor(const XMFLOAT3& color) {
			mColor[0] = color.x;
			mColor[1] = color.y;
			mColor[2] = color.z;
		}

		XMFLOAT3 GetAmbientLightColor() const { return XMFLOAT3(mAmbientColor); }
		void SetAmbientColor(const XMFLOAT3& color) {
			mAmbientColor[0] = color.x;
			mAmbientColor[1] = color.y;
			mAmbientColor[2] = color.z;
		}
		
		ER_GenericEvent<Delegate_RotationUpdate>* RotationUpdateEvent = new ER_GenericEvent<Delegate_RotationUpdate>();

		float mObjectTransformMatrix[16] =
		{
			1.f, 0.f, 0.f, 0.f,
			0.f, 1.f, 0.f, 0.f,
			0.f, 0.f, 1.f, 0.f,
			0.f, 0.f, 0.f, 1.f
		};

		float mColor[3] = { 1.0f, 0.95f, 0.863f };
		float mAmbientColor[3] = { 0.08f, 0.08f, 0.08f };

		bool mDrawSunOnSky = true;
		float mSunOnSkyExponent = 10000;
		float mSunOnSkyBrightness = 2.637f;
		float mLightIntensity = 4.0f;
	protected:
		XMFLOAT3 mDirection;
		XMFLOAT3 mUp;
		XMFLOAT3 mRight;
	private:
		void UpdateTransformArray(CXMMATRIX transform);

		ER_DebugProxyObject* mProxyModel = nullptr;
		float mProxyModelGizmoTranslationDelta = 10.0f;

		XMMATRIX mTransformMatrix = XMMatrixIdentity();
	};
}
