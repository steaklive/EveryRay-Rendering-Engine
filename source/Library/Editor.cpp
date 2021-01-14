#include "stdafx.h"

#include "Editor.h"
#include "Game.h"
#include "GameTime.h"
#include "VectorHelper.h"
#include "MatrixHelper.h"
#include "RenderingObject.h"
#include "Utility.h"
#include "Scene.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

namespace Library
{
	RTTI_DEFINITIONS(Editor)
	static int selectedObjectIndex = -1;
	
	Editor::Editor(Game& game)
		: GameComponent(game)
	{
	}

	Editor::~Editor()
	{
	}

	void Editor::Initialize()
	{

	}

	void Editor::LoadScene(Scene* scene)
	{
		mScene = scene;
	}

	void Editor::Update(const GameTime& gameTime)
	{
		if (Utility::IsEditorMode) {
			ImGui::Begin("Scene Editor");
			ImGui::Checkbox("Enable light editor", &Utility::IsLightEditor);
			ImGui::Separator();

			//skybox
			ImGui::Checkbox("Enable custom skybox colors", &mUseCustomSkyboxColor);
			if (mUseCustomSkyboxColor) {
				ImGui::ColorEdit4("Bottom color", bottomColorSky);
				ImGui::ColorEdit4("Top color", topColorSky);
			}
			ImGui::Separator();

			//objects
			ImGui::TextColored(ImVec4(0.12f, 0.78f, 0.44f, 1), "Scene objects");
			if (ImGui::CollapsingHeader("LOD Properties"))
			{
				ImGui::SliderFloat("LOD #0 distance", &Utility::DistancesLOD[0], 0.0f, 100.0f);
				ImGui::SliderFloat("LOD #1 distance", &Utility::DistancesLOD[1], Utility::DistancesLOD[0], 250.0f);
				ImGui::SliderFloat("LOD #2 distance", &Utility::DistancesLOD[2], Utility::DistancesLOD[1], 500.0f);
				//add more if needed
			}
			if (ImGui::Button("Save transforms")) {
				mScene->SaveRenderingObjectsTransforms();
			}

			int objectIndex = 0;
			for (auto& object : mScene->objects) {
				editorObjectsNames[objectIndex] = object.first.c_str();
				objectIndex++;
			}

			ImGui::PushItemWidth(-1);
			if (ImGui::Button("Deselect")) {
				selectedObjectIndex = -1;
			}
			ImGui::ListBox("##empty", &selectedObjectIndex, editorObjectsNames, mScene->objects.size());

			for (size_t i = 0; i < mScene->objects.size(); i++)
			{
				if (i == selectedObjectIndex)
					mScene->objects[editorObjectsNames[selectedObjectIndex]]->Selected(true);
				else
					mScene->objects[editorObjectsNames[i]]->Selected(false);
			}

			ImGui::End();
		}

	}

}