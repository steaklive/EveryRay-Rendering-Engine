#include "stdafx.h"

#include "ER_Editor.h"
#include "ER_Core.h"
#include "ER_CoreTime.h"
#include "ER_VectorHelper.h"
#include "ER_MatrixHelper.h"
#include "ER_RenderingObject.h"
#include "ER_Utility.h"
#include "ER_Scene.h"
#include "ER_PointLight.h"

namespace EveryRay_Core
{
	RTTI_DEFINITIONS(ER_Editor)
	static int selectedObjectIndex = -1;
	static int selectedPointLightIndex = -1;
	
	ER_Editor::ER_Editor(ER_Core& game)
		: ER_CoreComponent(game)
	{
		memset(editorObjectsNames, 0, sizeof(char*) * MAX_OBJECTS_IN_EDITOR_COUNT);
		memset(editorPointLightsNames, 0, sizeof(char*) * MAX_POINT_LIGHTS_IN_EDITOR_COUNT);
	}

	ER_Editor::~ER_Editor()
	{
	}

	void ER_Editor::Initialize(ER_Scene* scene)
	{
		mScene = scene;
	}

	void ER_Editor::Update(const ER_CoreTime& gameTime)
	{
		if (ER_Utility::IsEditorMode) 
		{
			ImGui::Begin("Scene Editor");

			if (ImGui::CollapsingHeader("Lights"))
			{
				ImGui::Checkbox("Enable sun editor", &ER_Utility::IsSunLightEditor);
				if (ER_Utility::IsSunLightEditor)
				{
					for (int i = 0; i < static_cast<int>(GetCore()->GetLevel()->mPointLights.size()); i++)
						GetCore()->GetLevel()->mPointLights[i]->SetSelectedInEditor(false);

					selectedPointLightIndex = -1;
				}

				{
					int objectIndex = 0;
					int objectsSize = static_cast<int>(GetCore()->GetLevel()->mPointLights.size());
					for (auto& object : GetCore()->GetLevel()->mPointLights)
					{
						if (objectIndex >= MAX_POINT_LIGHTS_IN_EDITOR_COUNT)
							continue;

						std::string name = "Point Light #" + std::to_string(objectIndex);
						editorPointLightsNames[objectIndex] = strdup(name.c_str());;
						objectIndex++;
					}

					ImGui::ListBox("Point Lights", &selectedPointLightIndex, editorPointLightsNames, static_cast<int>(GetCore()->GetLevel()->mPointLights.size()));
					if (ImGui::Button("Save Lights Data"))
						mScene->SavePointLightsData();
					ImGui::SameLine();
					if (ImGui::Button("Deselect Light"))
						selectedPointLightIndex = -1;

					for (int i = 0; i < objectsSize; i++)
						GetCore()->GetLevel()->mPointLights[i]->SetSelectedInEditor(i == selectedPointLightIndex);

					// disable other editors if we selected a point light
					if (selectedPointLightIndex >= 0)
						ER_Utility::DisableAllEditors();
				}
			}

			//skybox
			if (ImGui::CollapsingHeader("Sky"))
			{
				ImGui::Checkbox("Custom sky colors", &mUseCustomSkyboxColor);
				if (mUseCustomSkyboxColor)
				{
					ImGui::ColorEdit4("Bottom color", mBottomColorSky);
					ImGui::ColorEdit4("Top color", mTopColorSky);
				}
				ImGui::SliderFloat("Sky Min Height", &mSkyMinHeight, -25.0f, 25.0f);
				ImGui::SliderFloat("Sky Max Height", &mSkyMaxHeight, -25.0f, 25.0f);

				ImGui::Separator();
			}

			//rendering objects
			if (ImGui::CollapsingHeader("Rendering Objects"))
			{
				ImGui::Checkbox("Stop drawing", &ER_Utility::StopDrawingRenderingObjects);
				ImGui::Checkbox("Enable wireframe", &ER_Utility::IsWireframe);

				if (ImGui::CollapsingHeader("Global LOD Properties"))
				{
					ImGui::SliderFloat("LOD #0 distance", &ER_Utility::DistancesLOD[0], 0.0f, 300.0f);
					ImGui::SliderFloat("LOD #1 distance", &ER_Utility::DistancesLOD[1], ER_Utility::DistancesLOD[0], 1000.0f);
					ImGui::SliderFloat("LOD #2 distance", &ER_Utility::DistancesLOD[2], ER_Utility::DistancesLOD[1], 5000.0f);
					//add more if needed
				}
				if (ImGui::Button("Save Object's Transform"))
					mScene->SaveRenderingObjectsTransforms();
				ImGui::SameLine();
				if (ImGui::Button("Deselect Object"))
					selectedObjectIndex = -1;

				int objectIndex = 0;
				int objectsSize = 0;
				for (auto& object : mScene->objects)
				{
					if (object.second->IsAvailableInEditor() && objectIndex < MAX_OBJECTS_IN_EDITOR_COUNT)
					{
						editorObjectsNames[objectIndex] = object.first.c_str();
						objectIndex++;
					}
				}
				objectsSize = objectIndex;

				ImGui::PushItemWidth(-1);
				ImGui::ListBox("##empty", &selectedObjectIndex, editorObjectsNames, objectsSize);

				for (int i = 0; i < objectsSize; i++)
				{
					auto renderingObject = mScene->FindRenderingObjectByName(editorObjectsNames[i]);
					if (renderingObject)
						renderingObject->SetSelected(i == selectedObjectIndex);
				}
			}

			ImGui::End();
		}

	}

}