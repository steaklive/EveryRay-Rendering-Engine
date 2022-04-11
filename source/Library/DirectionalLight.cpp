#include "stdafx.h"

#include "DirectionalLight.h"
#include "VectorHelper.h"
#include "Utility.h"
#include "MatrixHelper.h"

#include "imgui.h"
#include "ImGuizmo.h"
namespace Library
{
	RTTI_DEFINITIONS(DirectionalLight)

	DirectionalLight::DirectionalLight(Game& game) : Light(game),
		mDirection(Vector3Helper::Forward),
		mUp(Vector3Helper::Up), mRight(Vector3Helper::Right), mProxyModel(nullptr)
	{
	}

	DirectionalLight::DirectionalLight(Game& game, Camera& camera) : Light(game),
		mDirection(Vector3Helper::Forward),
		mUp(Vector3Helper::Up), mRight(Vector3Helper::Right), mProxyModel(nullptr)
	{
		//directional gizmo model
		mProxyModel = new ProxyModel(*mGame, camera, Utility::GetFilePath("content\\models\\proxy\\proxy_direction_arrow.obj"), 1.0f);
		mProxyModel->Initialize();
		mProxyModel->SetPosition(0.0f, 50.0, 0.0f);
		mPseudoTranslation = XMMatrixTranslation(mProxyModel->Position().x, mProxyModel->Position().y ,mProxyModel->Position().z);
	}

	DirectionalLight::~DirectionalLight()
	{
		DeleteObject(mProxyModel);

		RotationUpdateEvent->RemoveAllListeners();
		DeleteObject(RotationUpdateEvent);
	}

	const XMFLOAT3& DirectionalLight::Direction() const
	{
		return mDirection;
	}

	const XMFLOAT3& DirectionalLight::Up() const
	{
		return mUp;
	}

	const XMFLOAT3& DirectionalLight::Right() const
	{
		return mRight;
	}

	XMVECTOR DirectionalLight::DirectionVector() const
	{
		return XMLoadFloat3(&mDirection);
	}

	XMVECTOR DirectionalLight::UpVector() const
	{
		return XMLoadFloat3(&mUp);
	}

	XMVECTOR DirectionalLight::RightVector() const
	{
		return XMLoadFloat3(&mRight);
	}

	void DirectionalLight::ApplyRotation(CXMMATRIX transform)
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

	void DirectionalLight::ApplyRotation(const XMFLOAT4X4& transform)
	{
		XMMATRIX transformMatrix = XMLoadFloat4x4(&transform);
		ApplyRotation(transformMatrix);
	}

	void DirectionalLight::ApplyTransform(const float* transform)
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

	void DirectionalLight::DrawProxyModel(const GameTime & time)
	{
		if (mProxyModel)
			mProxyModel->Draw(time);
	}

	void DirectionalLight::UpdateProxyModel(const GameTime & time, XMFLOAT4X4 viewMatrix, XMFLOAT4X4 projectionMatrix)
	{
		if (mProxyModel)
		{
			mProxyModel->Update(time);

			MatrixHelper::GetFloatArray(viewMatrix, mCameraViewMatrix);
			MatrixHelper::GetFloatArray(projectionMatrix, mCameraProjectionMatrix);

			UpdateGizmoTransform(mCameraViewMatrix, mCameraProjectionMatrix, mObjectTransformMatrix);
		}
	}

	void DirectionalLight::UpdateGizmoTransform(const float *cameraView, float *cameraProjection, float* matrix)
	{
		static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::ROTATE);
		static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
		static bool useSnap = false;
		static float snap[3] = { 1.f, 1.f, 1.f };
		static float bounds[] = { -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
		static float boundsSnap[] = { 0.1f, 0.1f, 0.1f };
		static bool boundSizing = false;
		static bool boundSizingSnap = false;

		if (Utility::IsEditorMode && Utility::IsLightEditor) {
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

	void DirectionalLight::UpdateTransformArray(CXMMATRIX transform)
	{
		XMFLOAT4X4 transformMatrix;
		XMStoreFloat4x4(&transformMatrix, transform);

		MatrixHelper::GetFloatArray(transformMatrix, mObjectTransformMatrix);
	}

	const XMMATRIX& DirectionalLight::LightMatrix(const XMFLOAT3& mPosition) const
	{
		XMVECTOR eyePosition = XMLoadFloat3(&mPosition);
		XMVECTOR direction = XMLoadFloat3(&mDirection);
		XMVECTOR upDirection = XMLoadFloat3(&mUp);

		XMMATRIX viewMatrix = XMMatrixLookToRH(eyePosition, direction, upDirection);
		return viewMatrix;
	}
}