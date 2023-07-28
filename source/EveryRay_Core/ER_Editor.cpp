#include "stdafx.h"

#include "ER_Editor.h"
#include "ER_Core.h"
#include "ER_CoreTime.h"
#include "ER_VectorHelper.h"
#include "ER_MatrixHelper.h"
#include "ER_RenderingObject.h"
#include "ER_Utility.h"
#include "ER_Scene.h"

namespace EveryRay_Core
{
	RTTI_DEFINITIONS(ER_Editor)
	static int selectedObjectIndex = -1;
	
	ER_Editor::ER_Editor(ER_Core& game)
		: ER_CoreComponent(game)
	{
	}

	ER_Editor::~ER_Editor()
	{
	}

	void ER_Editor::Initialize()
	{

	}

	void ER_Editor::LoadScene(ER_Scene* scene)
	{
		mScene = scene;
	}

	void ER_Editor::Update(const ER_CoreTime& gameTime)
	{
		if (ER_Utility::IsEditorMode) {
			ImGui::Begin("Scene Editor");
			ImGui::Checkbox("Enable light editor", &ER_Utility::IsLightEditor);
			ImGui::Separator();

			//skybox
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

			//objects
			ImGui::TextColored(ImVec4(0.12f, 0.78f, 0.44f, 1), "Scene objects");
			ImGui::Checkbox("Stop drawing objects", &ER_Utility::StopDrawingRenderingObjects);
			if (ImGui::CollapsingHeader("Global LOD Properties"))
			{
				ImGui::SliderFloat("LOD #0 distance", &ER_Utility::DistancesLOD[0], 0.0f, 100.0f);
				ImGui::SliderFloat("LOD #1 distance", &ER_Utility::DistancesLOD[1], ER_Utility::DistancesLOD[0], 250.0f);
				ImGui::SliderFloat("LOD #2 distance", &ER_Utility::DistancesLOD[2], ER_Utility::DistancesLOD[1], 1000.0f);
				//add more if needed
			}
			if (ImGui::Button("Save transforms")) {
				mScene->SaveRenderingObjectsTransforms();
			}

			int objectIndex = 0;
			int objectsSize = 0;
			for (auto& object : mScene->objects) {
				if (object.second->IsAvailableInEditor())
				{
					editorObjectsNames[objectIndex] = object.first.c_str();
					objectIndex++;
				}
			}
			objectsSize = objectIndex;

			ImGui::PushItemWidth(-1);
			if (ImGui::Button("Deselect")) {
				selectedObjectIndex = -1;
			}
			ImGui::ListBox("##empty", &selectedObjectIndex, editorObjectsNames, objectsSize);

			for (size_t i = 0; i < objectsSize; i++)
			{
				if (i == selectedObjectIndex)
				{
					auto renderingObject = mScene->FindRenderingObjectByName(editorObjectsNames[selectedObjectIndex]);
					if (renderingObject)
						renderingObject->SetSelected(true);
				}
				else
				{
					auto renderingObject = mScene->FindRenderingObjectByName(editorObjectsNames[i]);
					if (renderingObject)
						renderingObject->SetSelected(false);
				}
			}

			ImGui::End();
		}

	}

}