

#include "ER_Editor.h"
#include "ER_CameraFPS.h"
#include "ER_Core.h"
#include "ER_CoreTime.h"
#include "ER_VectorHelper.h"
#include "ER_MatrixHelper.h"
#include "ER_RenderingObject.h"
#include "ER_Utility.h"
#include "ER_Scene.h"
#include "ER_Skybox.h"
#include "ER_PointLight.h"
#include "ER_MatrixHelper.h"
#include "ER_Wind.h"
#include "ER_DirectionalLight.h"

namespace EveryRay_Core
{
	RTTI_DEFINITIONS(ER_Editor)

	static int selectedObjectIndex = -1;
	static int selectedPointLightIndex = -1;
	static const int maxRenderingObjectsListHeight = 15;
	
	static float cameraFov = 60.0f;
	static float cameraMovementRate = 10.0f;
	static float nearPlaneDist = 0.5f;
	static float farPlaneDist = 100000.0f;

	ER_Editor::ER_Editor(ER_Core& game)
		: ER_CoreComponent(game)
	{
		for (int i = 0; i < MAX_OBJECTS_IN_EDITOR_COUNT; i++)
			mEditorObjectsNames[i] = (char*)malloc(sizeof(char) * MAX_NAME_CHAR_LENGTH);

		for (int i = 0; i < MAX_POINT_LIGHTS_IN_EDITOR_COUNT; i++)
			mEditorPointLightsNames[i] = (char*)malloc(sizeof(char) * MAX_NAME_CHAR_LENGTH);

		mCamera = (ER_CameraFPS*)(GetCore()->GetServices().FindService(ER_Camera::TypeIdClass()));
	}

	ER_Editor::~ER_Editor()
	{
		for (int i = 0; i < MAX_OBJECTS_IN_EDITOR_COUNT; i++)
			free(mEditorObjectsNames[i]);

		for (int i = 0; i < MAX_POINT_LIGHTS_IN_EDITOR_COUNT; i++)
			free(mEditorPointLightsNames[i]);
	}

	void ER_Editor::Initialize(ER_Scene* scene)
	{
		mScene = scene;
	}

	void ER_Editor::Update(const ER_CoreTime& gameTime)
	{
		if (ER_Utility::IsEditorMode) 
		{
			static ImGuizmo::OPERATION currentGizmoOperation(ImGuizmo::ROTATE);
			static ImGuizmo::MODE currentGizmoMode(ImGuizmo::WORLD);
			static bool useSnap = false;
			static float snap[3] = { 1.f, 1.f, 1.f };
			static float bounds[] = { -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
			static float boundsSnap[] = { 0.1f, 0.1f, 0.1f };
			static bool boundSizing = false;
			static bool boundSizingSnap = false;

			float cameraViewMatrix[16];
			float cameraProjectionMatrix[16];
			ER_MatrixHelper::SetFloatArray(mCamera->ViewMatrix(), cameraViewMatrix);
			ER_MatrixHelper::SetFloatArray(mCamera->ProjectionMatrix(), cameraProjectionMatrix);

			float matrixTranslation[3], matrixRotation[3], matrixScale[3];

			ImGui::Begin("Scene Editor");

			if (ImGui::CollapsingHeader("Camera"))
			{
				assert(mCamera);
				ImGui::Text("Position: (%.1f,%.1f,%.1f)", mCamera->Position().x, mCamera->Position().y, mCamera->Position().z);
				if (ImGui::Button("Reset Position"))
					mCamera->SetPosition(XMFLOAT3(0, 0, 0));
				ImGui::SameLine();
				if (ImGui::Button("Reset Direction"))
					mCamera->SetDirection(XMFLOAT3(0, 0, 1));
				ImGui::SliderFloat("Speed", &cameraMovementRate, 0.0f, 2000.0f);
				mCamera->SetMovementRate(cameraMovementRate);
				ImGui::SliderFloat("FOV", &cameraFov, 1.0f, 90.0f);
				mCamera->SetFOV(cameraFov * XM_PI / 180.0f);
				ImGui::SliderFloat("Near Plane", &nearPlaneDist, 0.5f, 150.0f);
				mCamera->SetNearPlaneDistance(nearPlaneDist);
				ImGui::SliderFloat("Far Plane", &farPlaneDist, 150.0f, 200000.0f);
				mCamera->SetFarPlaneDistance(farPlaneDist);
				ImGui::Checkbox("CPU frustum culling", &ER_Utility::IsMainCameraCPUFrustumCulling);
			}

			if (ImGui::CollapsingHeader("Environment - Lights"))
			{
				ImGui::Checkbox("Enable sun editor", &ER_Utility::IsSunLightEditor);
				if (ER_Utility::IsSunLightEditor)
				{
					ER_Utility::DisableAllEditors(); ER_Utility::IsSunLightEditor = true;
					
					for (int i = 0; i < static_cast<int>(GetCore()->GetLevel()->mPointLights.size()); i++)
						GetCore()->GetLevel()->mPointLights[i]->SetSelectedInEditor(false);

					selectedPointLightIndex = -1;

					ER_DirectionalLight* sun = GetCore()->GetLevel()->mDirectionalLight;
					ImGui::ColorEdit3("Color", sun->mColor);
					ImGui::ColorEdit3("Ambient Color", sun->mAmbientColor);
					ImGui::SliderFloat("Intensity", &sun->mLightIntensity, 0.0f, 50.0f);

					currentGizmoOperation = ImGuizmo::ROTATE;
					ImGuizmo::DecomposeMatrixToComponents(sun->mObjectTransformMatrix, matrixTranslation, matrixRotation, matrixScale);
					ImGui::InputFloat3("Rotate", matrixRotation, 3);
					ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, sun->mObjectTransformMatrix);

					ImGui::Checkbox("Render Sun On Sky", &sun->mDrawSunOnSky);
					if (sun->mDrawSunOnSky) 
					{
						ImGui::SliderFloat("Sun On Sky Exponent", &sun->mSunOnSkyExponent, 1.0f, 10000.0f);
						ImGui::SliderFloat("Sun On Sky Brightness", &sun->mSunOnSkyBrightness, 0.0f, 10.0f);
					}

					ImGuiIO& io = ImGui::GetIO();
					ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
					ImGuizmo::Manipulate(cameraViewMatrix, cameraProjectionMatrix, currentGizmoOperation, currentGizmoMode, sun->mObjectTransformMatrix, NULL, useSnap ? &snap[0] : NULL, boundSizing ? bounds : NULL, boundSizingSnap ? boundsSnap : NULL);

					sun->ApplyTransform(sun->mObjectTransformMatrix);
				}

				//point lights
				ImGui::Separator();
				{
					int objectIndex = 0;
					int objectsSize = static_cast<int>(GetCore()->GetLevel()->mPointLights.size());
					for (auto& object : GetCore()->GetLevel()->mPointLights)
					{
						if (objectIndex >= MAX_POINT_LIGHTS_IN_EDITOR_COUNT)
							continue;

						std::string name = "Point Light #" + std::to_string(objectIndex);
						strcpy(mEditorPointLightsNames[objectIndex], name.c_str());
						objectIndex++;
					}

					ImGui::ListBox("Point Lights", &selectedPointLightIndex, mEditorPointLightsNames, static_cast<int>(GetCore()->GetLevel()->mPointLights.size()));
					if (ImGui::Button("Save Lights Data"))
						mScene->SavePointLightsData();
					ImGui::SameLine();
					if (ImGui::Button("Deselect Light"))
						selectedPointLightIndex = -1;

					for (int i = 0; i < objectsSize; i++)
						GetCore()->GetLevel()->mPointLights[i]->SetSelectedInEditor(i == selectedPointLightIndex);

					if (selectedPointLightIndex >= 0)
					{
						ER_Utility::DisableAllEditors();

						ER_PointLight* light = GetCore()->GetLevel()->mPointLights[selectedPointLightIndex];

						ImGui::ColorEdit3("Color", light->mEditorColor);
						ImGui::SliderFloat("Intensity", &light->mEditorIntensity, 0.0f, 1000.0f);
						light->SetColor(XMFLOAT4(light->mEditorColor[0], light->mEditorColor[1], light->mEditorColor[2], light->mEditorIntensity));

						ImGui::SliderFloat("Radius", &light->mRadius, 0.0f, 100.0f);

						light->mEditorCurrentTransformMatrix[12] = light->GetPosition().x;
						light->mEditorCurrentTransformMatrix[13] = light->GetPosition().y;
						light->mEditorCurrentTransformMatrix[14] = light->GetPosition().z;

						currentGizmoOperation = ImGuizmo::TRANSLATE;
						currentGizmoMode = ImGuizmo::WORLD;

						if (ImGui::IsKeyPressed(84))
							currentGizmoOperation = ImGuizmo::TRANSLATE;

						ImGuizmo::DecomposeMatrixToComponents(light->mEditorCurrentTransformMatrix, matrixTranslation, matrixRotation, matrixScale);
						ImGui::InputFloat3("Translate", matrixTranslation, 3);
						ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, light->mEditorCurrentTransformMatrix);

						if (ImGui::Button("Move camera to"))
						{
							XMFLOAT3 newCameraPos = light->GetPosition();
							mCamera->SetPosition(newCameraPos);
						}

						ImGuiIO& io = ImGui::GetIO();
						ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
						ImGuizmo::Manipulate(cameraViewMatrix, cameraProjectionMatrix, currentGizmoOperation, currentGizmoMode, light->mEditorCurrentTransformMatrix,
							NULL, useSnap ? &snap[0] : NULL, boundSizing ? bounds : NULL, boundSizingSnap ? boundsSnap : NULL);

						light->SetPosition(XMFLOAT3(light->mEditorCurrentTransformMatrix[12], light->mEditorCurrentTransformMatrix[13], light->mEditorCurrentTransformMatrix[14]));
					}
				}
			}

			if (ImGui::CollapsingHeader("Environment - Sky"))
			{
				ER_Skybox* sky = GetCore()->GetLevel()->mSkybox;
				{
					ImGui::ColorEdit4("Bottom color", sky->mBottomColorEditor);
					ImGui::ColorEdit4("Top color", sky->mTopColorEditor);
				}
				ImGui::SliderFloat("Sky Min Height", &sky->mSkyMinHeightEditor, -25.0f, 25.0f);
				ImGui::SliderFloat("Sky Max Height", &sky->mSkyMaxHeightEditor, -25.0f, 25.0f);

				ImGui::Separator();
			}

			if (ImGui::CollapsingHeader("Environment - Wind"))
			{
				ImGui::Checkbox("Enable wind editor", &ER_Utility::IsWindEditor);

				if (ER_Utility::IsWindEditor)
				{
					ER_Wind* wind = GetCore()->GetLevel()->mWind;
					ER_Utility::DisableAllEditors(); ER_Utility::IsWindEditor = true;

					ImGui::SliderFloat("Wind strength", &wind->mWindStrength, 0.0f, 10.0f);
					ImGui::SliderFloat("Wind gust distance", &wind->mWindGustDistance, 0.0f, 10.0f);
					ImGui::SliderFloat("Wind frequency", &wind->mWindFrequency, 0.0f, 10.0f);

					ImGuizmo::DecomposeMatrixToComponents(wind->mObjectTransformMatrix, matrixTranslation, matrixRotation, matrixScale);
					ImGui::InputFloat3("Rotate", matrixRotation, 3);
					ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, wind->mObjectTransformMatrix);

					ImGuiIO& io = ImGui::GetIO();
					ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
					ImGuizmo::Manipulate(cameraViewMatrix, cameraProjectionMatrix, currentGizmoOperation, currentGizmoMode, wind->mObjectTransformMatrix, NULL, useSnap ? &snap[0] : NULL, boundSizing ? bounds : NULL, boundSizingSnap ? boundsSnap : NULL);
				
					wind->ApplyTransform(wind->mObjectTransformMatrix);
				}
			}

			if (ImGui::CollapsingHeader("Rendering Objects"))
			{
				ImGui::Checkbox("Stop drawing", &ER_Utility::StopDrawingRenderingObjects);
				ImGui::Checkbox("Enable wireframe", &ER_Utility::IsWireframe);
				ImGui::Checkbox("Enable object editor", &ER_Utility::IsRenderingObjectEditor);
				if (ER_Utility::IsRenderingObjectEditor)
				{
					ER_Utility::DisableAllEditors(); ER_Utility::IsRenderingObjectEditor = true;
				}

				if (ImGui::CollapsingHeader("Global LOD Properties"))
				{
					ImGui::SliderFloat("LOD #0 distance", &ER_Utility::DistancesLOD[0], 0.0f, 300.0f);
					ImGui::SliderFloat("LOD #1 distance", &ER_Utility::DistancesLOD[1], ER_Utility::DistancesLOD[0], 1000.0f);
					ImGui::SliderFloat("LOD #2 distance", &ER_Utility::DistancesLOD[2], ER_Utility::DistancesLOD[1], 5000.0f);
					//add more if needed
				}
				if (ImGui::Button("Save Object's Transform"))
					mScene->SaveRenderingObjectsData();
				ImGui::SameLine();
				if (ImGui::Button("Deselect Object"))
					selectedObjectIndex = -1;

				int objectIndex = 0;
				int objectsSize = 0;
				for (auto& object : mScene->objects)
				{
					if (object.second->IsAvailableInEditor() && objectIndex < MAX_OBJECTS_IN_EDITOR_COUNT)
					{
						strcpy(mEditorObjectsNames[objectIndex], object.first.c_str());
						objectIndex++;
					}
				}
				objectsSize = objectIndex;

				ImGui::PushItemWidth(-1);
				ImGui::ListBox("##empty", &selectedObjectIndex, mEditorObjectsNames, objectsSize, maxRenderingObjectsListHeight);

				for (int i = 0; i < objectsSize; i++)
				{
					auto renderingObject = mScene->FindRenderingObjectByName(mEditorObjectsNames[i]);
					if (renderingObject)
						renderingObject->SetSelected(i == selectedObjectIndex);
				}
			}

			ImGui::End();
		}

	}

}