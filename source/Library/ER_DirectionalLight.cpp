#include "stdafx.h"

#include "ER_DirectionalLight.h"
#include "ER_VectorHelper.h"
#include "ER_Utility.h"
#include "ER_MatrixHelper.h"

#include "imgui.h"
#include "ImGuizmo.h"
namespace Library
{
	RTTI_DEFINITIONS(ER_DirectionalLight)

	ER_DirectionalLight::ER_DirectionalLight(ER_Core& game) : ER_Light(game),
		mDirection(ER_Vector3Helper::Forward),
		mUp(ER_Vector3Helper::Up), mRight(ER_Vector3Helper::Right), mProxyModel(nullptr)
	{
	}

	ER_DirectionalLight::ER_DirectionalLight(ER_Core& game, ER_Camera& camera) : ER_Light(game),
		mDirection(ER_Vector3Helper::Forward),
		mUp(ER_Vector3Helper::Up), mRight(ER_Vector3Helper::Right), mProxyModel(nullptr)
	{
		//directional gizmo model
		mProxyModel = new ER_DebugProxyObject(*mCore, camera, ER_Utility::GetFilePath("content\\models\\proxy\\proxy_direction_arrow.obj"), 1.0f);
		mProxyModel->Initialize();
		mProxyModel->SetPosition(0.0f, 50.0, 0.0f);
		mPseudoTranslation = XMMatrixTranslation(mProxyModel->Position().x, mProxyModel->Position().y ,mProxyModel->Position().z);
	}

	ER_DirectionalLight::~ER_DirectionalLight()
	{
		DeleteObject(mProxyModel);

		RotationUpdateEvent->RemoveAllListeners();
		DeleteObject(RotationUpdateEvent);
	}

	const XMFLOAT3& ER_DirectionalLight::Direction() const
	{
		return mDirection;
	}

	const XMFLOAT3& ER_DirectionalLight::Up() const
	{
		return mUp;
	}

	const XMFLOAT3& ER_DirectionalLight::Right() const
	{
		return mRight;
	}

	XMVECTOR ER_DirectionalLight::DirectionVector() const
	{
		return XMLoadFloat3(&mDirection);
	}

	XMVECTOR ER_DirectionalLight::UpVector() const
	{
		return XMLoadFloat3(&mUp);
	}

	XMVECTOR ER_DirectionalLight::RightVector() const
	{
		return XMLoadFloat3(&mRight);
	}

	void ER_DirectionalLight::ApplyRotation(CXMMATRIX transform)
	{
		mTransformMatrix = transform;

		XMVECTOR direction = XMLoadFloat3(&mDirection);
		XMVECTOR up = XMLoadFloat3(&mUp);

		direction = XMVector3TransformNormal(direction, transform);
		direction = XMVector3Normalize(direction);
		
		up = XMVector3TransformNormal(up, transform);
		up = XMVector3Normalize(up);

		XMVECTOR right = XMVector3Cross(direction, up);
		up = XMVector3Cross(right, direction);

		XMStoreFloat3(&mDirection, direction);
		XMStoreFloat3(&mUp, up);
		XMStoreFloat3(&mRight, right);

		if (mProxyModel)
			mProxyModel->ApplyRotation(transform);

		UpdateTransformArray(transform * mPseudoTranslation);
	}

	void ER_DirectionalLight::ApplyRotation(const XMFLOAT4X4& transform)
	{
		XMMATRIX transformMatrix = XMLoadFloat4x4(&transform);
		ApplyRotation(transformMatrix);
	}

	void ER_DirectionalLight::ApplyTransform(const float* transform)
	{
		XMFLOAT4X4 transformMatrixFloat = XMFLOAT4X4(transform);
		XMMATRIX transformMatrix = XMLoadFloat4x4(&transformMatrixFloat);
		
		XMVECTOR direction = XMVECTOR{ 0.0f, 0.0, -1.0f };
		XMVECTOR up = XMVECTOR{ 0.0f, 1.0, 0.0f };

		direction = XMVector3TransformNormal(direction, transformMatrix);
		direction = XMVector3Normalize(direction);

		up = XMVector3TransformNormal(up, transformMatrix);
		up = XMVector3Normalize(up);

		XMVECTOR right = XMVector3Cross(direction, up);
		up = XMVector3Cross(right, direction);

		XMStoreFloat3(&mDirection, direction);
		XMStoreFloat3(&mUp, up);
		XMStoreFloat3(&mRight, right);

		mTransformMatrix = transformMatrix;

		if (mProxyModel)
			mProxyModel->ApplyTransform(transformMatrix);

		for (auto listener : RotationUpdateEvent->GetListeners())
			listener();
	}

	void ER_DirectionalLight::DrawProxyModel(const ER_CoreTime & time)
	{
		if (mProxyModel)
			mProxyModel->Draw(time);
	}

	void ER_DirectionalLight::UpdateProxyModel(const ER_CoreTime & time, XMFLOAT4X4 viewMatrix, XMFLOAT4X4 projectionMatrix)
	{
		if (mProxyModel)
		{
			mProxyModel->Update(time);

			ER_MatrixHelper::GetFloatArray(viewMatrix, mCameraViewMatrix);
			ER_MatrixHelper::GetFloatArray(projectionMatrix, mCameraProjectionMatrix);

			UpdateGizmoTransform(mCameraViewMatrix, mCameraProjectionMatrix, mObjectTransformMatrix);
		}
	}

	void ER_DirectionalLight::UpdateGizmoTransform(const float *cameraView, float *cameraProjection, float* matrix)
	{
		static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::ROTATE);
		static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
		static bool useSnap = false;
		static float snap[3] = { 1.f, 1.f, 1.f };
		static float bounds[] = { -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
		static float boundsSnap[] = { 0.1f, 0.1f, 0.1f };
		static bool boundSizing = false;
		static bool boundSizingSnap = false;

		if (ER_Utility::IsEditorMode && ER_Utility::IsLightEditor) {
			ImGui::Begin("Directional Light Editor");

			ImGui::ColorEdit3("Sun Color", mSunColor);
			ImGui::ColorEdit3("Ambient Color", mAmbientColor);

			ImGuizmo::DecomposeMatrixToComponents(matrix, mMatrixTranslation, mMatrixRotation, mMatrixScale);
			ImGui::InputFloat3("Rotate", mMatrixRotation, 3);
			ImGuizmo::RecomposeMatrixFromComponents(mMatrixTranslation, mMatrixRotation, mMatrixScale, matrix);

			ImGui::Checkbox("Render Sun", &mDrawSun);
			if (mDrawSun) {
				ImGui::SliderFloat("Sun Exponent", &mSunExponent, 1.0f, 10000.0f);
				ImGui::SliderFloat("Sun Brightness", &mSunBrightness, 0.0f, 10.0f);
				ImGui::SliderFloat("Sun Intensity", &mDirectionalLightIntensity, 0.0f, 50.0f);
			}
			ImGui::End();

			ImGuiIO& io = ImGui::GetIO();
			ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
			ImGuizmo::Manipulate(cameraView, cameraProjection, mCurrentGizmoOperation, mCurrentGizmoMode, matrix, NULL, useSnap ? &snap[0] : NULL, boundSizing ? bounds : NULL, boundSizingSnap ? boundsSnap : NULL);
		
			ApplyTransform(matrix);
		}
	}

	void ER_DirectionalLight::UpdateTransformArray(CXMMATRIX transform)
	{
		XMFLOAT4X4 transformMatrix;
		XMStoreFloat4x4(&transformMatrix, transform);

		ER_MatrixHelper::GetFloatArray(transformMatrix, mObjectTransformMatrix);
	}

	const XMMATRIX& ER_DirectionalLight::LightMatrix(const XMFLOAT3& mPosition) const
	{
		XMVECTOR eyePosition = XMLoadFloat3(&mPosition);
		XMVECTOR direction = XMLoadFloat3(&mDirection);
		XMVECTOR upDirection = XMLoadFloat3(&mUp);

		XMMATRIX viewMatrix = XMMatrixLookToRH(eyePosition, direction, upDirection);
		return viewMatrix;
	}
}