

#include "ER_PointLight.h"
#include "ER_Core.h"
#include "ER_VectorHelper.h"
#include "ER_Utility.h"
#include "ER_Camera.h"

namespace EveryRay_Core
{
	RTTI_DEFINITIONS(ER_PointLight)

	ER_PointLight::ER_PointLight(ER_Core& game)
		: ER_Light(game), mPosition(ER_Vector3Helper::Zero), mRadius(0.0)
	{
	}

	ER_PointLight::ER_PointLight(ER_Core& game, const XMFLOAT3& pos, float radius)
		: ER_Light(game), mPosition(pos), mRadius(radius)
	{
	}

	ER_PointLight::~ER_PointLight()
	{
	}

	void ER_PointLight::Update(const ER_CoreTime& time)
	{
		mEditorIntensity = GetColor().w;
		mEditorColor[0] = GetColor().x;
		mEditorColor[1] = GetColor().y;
		mEditorColor[2] = GetColor().z;

		if (ER_Utility::IsEditorMode && !ER_Utility::IsSunLightEditor && mIsSelectedInEditor)
		{
			ImGui::Begin("Point Light Editor");
			ImGui::ColorEdit3("Color", mEditorColor);
			ImGui::SliderFloat("Intensity", &mEditorIntensity, 0.0f, 1000.0f);
			SetColor(XMFLOAT4(mEditorColor[0], mEditorColor[1], mEditorColor[2], mEditorIntensity));

			ImGui::SliderFloat("Radius", &mRadius, 0.0f, 100.0f);

			float cameraViewMat[16];
			float cameraProjMat[16];
			ER_MatrixHelper::SetFloatArray(((ER_Camera*)GetCore()->GetServices().FindService(ER_Camera::TypeIdClass()))->ViewMatrix4X4(), cameraViewMat);
			ER_MatrixHelper::SetFloatArray(((ER_Camera*)GetCore()->GetServices().FindService(ER_Camera::TypeIdClass()))->ProjectionMatrix4X4(), cameraProjMat);

			mEditorCurrentTransformMatrix[12] = GetPosition().x;
			mEditorCurrentTransformMatrix[13] = GetPosition().y;
			mEditorCurrentTransformMatrix[14] = GetPosition().z;

			static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
			static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
			static bool useSnap = false;
			static float snap[3] = { 1.f, 1.f, 1.f };
			static float bounds[] = { -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
			static float boundsSnap[] = { 0.1f, 0.1f, 0.1f };
			static bool boundSizing = false;
			static bool boundSizingSnap = false;

			if (ImGui::IsKeyPressed(84))
				mCurrentGizmoOperation = ImGuizmo::TRANSLATE;

			ImGuizmo::DecomposeMatrixToComponents(mEditorCurrentTransformMatrix, mEditorMatrixTranslation, mEditorMatrixRotation, mEditorMatrixScale);
			ImGui::InputFloat3("Translate", mEditorMatrixTranslation, 3);
			ImGuizmo::RecomposeMatrixFromComponents(mEditorMatrixTranslation, mEditorMatrixRotation, mEditorMatrixScale, mEditorCurrentTransformMatrix);

			ImGui::End();

			ImGuiIO& io = ImGui::GetIO();
			ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
			ImGuizmo::Manipulate(cameraViewMat, cameraProjMat, mCurrentGizmoOperation, mCurrentGizmoMode, mEditorCurrentTransformMatrix,
				NULL, useSnap ? &snap[0] : NULL, boundSizing ? bounds : NULL, boundSizingSnap ? boundsSnap : NULL);

			SetPosition(XMFLOAT3(mEditorCurrentTransformMatrix[12], mEditorCurrentTransformMatrix[13], mEditorCurrentTransformMatrix[14]));
		}
	}

	XMVECTOR ER_PointLight::PositionVector() const
	{
		return XMLoadFloat3(&mPosition);
	}

	void ER_PointLight::SetPosition(FLOAT x, FLOAT y, FLOAT z)
	{
		SetPosition(XMFLOAT3(x,y,z));
	}

	void ER_PointLight::SetPosition(const XMFLOAT3& position)
	{
		mPosition = position;
	}

	void ER_PointLight::SetRadius(float value)
	{
		mRadius = value;
	}
}