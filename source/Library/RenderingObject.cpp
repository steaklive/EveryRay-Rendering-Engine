#include "stdafx.h"

#include "RenderingObject.h"
#include "GameException.h"
#include "Model.h"
#include "Mesh.h"
#include "Game.h"
#include "MatrixHelper.h"
#include "Utility.h"

#include "TGATextureLoader.h"
#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>

#include "imgui.h"
#include "ImGuizmo.h"

namespace Rendering
{
	RenderingObject::RenderingObject(std::string pName, Game& pGame, Camera& pCamera, std::unique_ptr<Model> pModel, bool availableInEditor )
		:
		GameComponent(pGame),
		mCamera(pCamera),
		mModel(std::move(pModel)),
		mMeshesRenderBuffers(0, std::vector<RenderBufferData*>(0, nullptr)),
		mMeshesInstanceBuffers(0, nullptr),
		mMaterials(0, nullptr),
		mMeshVertices(0),
		mName(pName),
		mInstanceCount(0),
		mDebugAABB(nullptr),
		mAvailableInEditorMode(availableInEditor)
	{
		if (!mModel)
			throw GameException("Failed to create a RenderingObject from a model");

		mMeshesCount = mModel->Meshes().size();
		for (size_t i = 0; i < mMeshesCount; i++)
		{
			mMeshVertices.push_back(mModel->Meshes().at(i)->Vertices());
			mMeshesTextureBuffers.push_back(TextureData());
		}

		for (size_t i = 0; i < mMeshVertices.size(); i++)
		{
			for (size_t j = 0; j < mMeshVertices[i].size(); j++)
			{
				mMeshAllVertices.push_back(mMeshVertices[i][j]);
			}
		}

		LoadAssignedMeshTextures();
		mAABB = mModel->GenerateAABB();

		if (mAvailableInEditorMode) {
			mDebugAABB = new RenderableAABB(*mGame, mCamera);
			mDebugAABB->Initialize();
			mDebugAABB->InitializeGeometry(mAABB, XMMatrixScaling(1, 1, 1));
			mDebugAABB->SetPosition(XMFLOAT3(0,0,0));
		}
		
	}

	RenderingObject::~RenderingObject()
	{
		DeleteObject(MeshMaterialVariablesUpdateEvent);
		DeletePointerCollection(mMaterials);
		for (size_t i = 0; i < mMeshesRenderBuffers.size(); i++)
		{
			DeletePointerCollection(mMeshesRenderBuffers[i]);
		}
		DeletePointerCollection(mMeshesInstanceBuffers);
		mMeshesTextureBuffers.clear();

		DeleteObject(mDebugAABB);
	}

	void RenderingObject::LoadMaterial(Material* pMaterial, Effect* pEffect)
	{
		assert(mModel != nullptr);

		mMaterials.push_back(pMaterial);
		mMaterials.back()->Initialize(pEffect);
	}

	void RenderingObject::LoadAssignedMeshTextures()
	{
		for (size_t i = 0; i < mMeshesCount; i++)
		{
			if (mModel->Meshes()[i]->GetMaterial()->Textures().size() == 0) 
				continue;

			std::vector<std::wstring>* texturesAlbedo = mModel->Meshes()[i]->GetMaterial()->GetTexturesByType(TextureType::TextureTypeDifffuse);
			if (texturesAlbedo != nullptr)
			{
				if (texturesAlbedo->size() != 0)
				{
					std::wstring textureRelativePath = texturesAlbedo->at(0);
					std::string fullPath;
					Utility::GetDirectory(mModel->GetFileName(), fullPath);
					fullPath += "/";
					std::wstring resultPath;
					Utility::ToWideString(fullPath, resultPath);
					resultPath += textureRelativePath;
					LoadTexture(TextureType::TextureTypeDifffuse, resultPath, i);
				}
				else
					LoadTexture(TextureType::TextureTypeDifffuse, Utility::GetFilePath(L"content\\textures\\emptyDiffuseMap.png"), i);
			}
			else
				LoadTexture(TextureType::TextureTypeDifffuse, Utility::GetFilePath(L"content\\textures\\emptyDiffuseMap.png"), i);

			std::vector<std::wstring>* texturesNormal = mModel->Meshes()[i]->GetMaterial()->GetTexturesByType(TextureType::TextureTypeNormalMap);
			if (texturesNormal != nullptr )
			{
				if(texturesNormal->size() != 0)
				{
					std::wstring textureRelativePath = texturesNormal->at(0);
					std::string fullPath;
					Utility::GetDirectory(mModel->GetFileName(), fullPath);
					fullPath += "/";
					std::wstring resultPath;
					Utility::ToWideString(fullPath, resultPath);
					resultPath += textureRelativePath;
					LoadTexture(TextureType::TextureTypeNormalMap, resultPath, i);
				}
				else
					LoadTexture(TextureType::TextureTypeNormalMap, Utility::GetFilePath(L"content\\textures\\emptyNormalMap.jpg"), i);
			}
			else
				LoadTexture(TextureType::TextureTypeNormalMap, Utility::GetFilePath(L"content\\textures\\emptyNormalMap.jpg"), i);

			std::vector<std::wstring>* texturesSpec = mModel->Meshes()[i]->GetMaterial()->GetTexturesByType(TextureType::TextureTypeSpecularMap);
			if (texturesSpec != nullptr)
			{
				if (texturesSpec->size() != 0)
				{
					std::wstring textureRelativePath = texturesSpec->at(0);
					std::string fullPath;
					Utility::GetDirectory(mModel->GetFileName(), fullPath);
					fullPath += "/";
					std::wstring resultPath;
					Utility::ToWideString(fullPath, resultPath);
					resultPath += textureRelativePath;
					LoadTexture(TextureType::TextureTypeSpecularMap, resultPath, i);
				}
				else
					LoadTexture(TextureType::TextureTypeSpecularMap, Utility::GetFilePath(L"content\\textures\\emptySpecularMap.png"), i);
			}
			else
				LoadTexture(TextureType::TextureTypeSpecularMap, Utility::GetFilePath(L"content\\textures\\emptySpecularMap.png"), i);

			std::vector<std::wstring>* texturesRoughness = mModel->Meshes()[i]->GetMaterial()->GetTexturesByType(TextureType::TextureTypeDisplacementMap);
			if (texturesRoughness != nullptr)
			{
				if (texturesRoughness->size() != 0)
				{
					std::wstring textureRelativePath = texturesRoughness->at(0);
					std::string fullPath;
					Utility::GetDirectory(mModel->GetFileName(), fullPath);
					fullPath += "/";
					std::wstring resultPath;
					Utility::ToWideString(fullPath, resultPath);
					resultPath += textureRelativePath;
					LoadTexture(TextureType::TextureTypeDisplacementMap, resultPath, i);
				}
				else
					LoadTexture(TextureType::TextureTypeDisplacementMap, Utility::GetFilePath(L"content\\textures\\emptyRoughnessMap.png"), i);
			}
			else
				LoadTexture(TextureType::TextureTypeDisplacementMap, Utility::GetFilePath(L"content\\textures\\emptyRoughnessMap.png"), i);
			
			std::vector<std::wstring>* texturesMetallic = mModel->Meshes()[i]->GetMaterial()->GetTexturesByType(TextureType::TextureTypeEmissive);
			if (texturesMetallic != nullptr)
			{
				if (texturesMetallic->size() != 0)
				{
					std::wstring textureRelativePath = texturesMetallic->at(0);
					std::string fullPath;
					Utility::GetDirectory(mModel->GetFileName(), fullPath);
					fullPath += "/";
					std::wstring resultPath;
					Utility::ToWideString(fullPath, resultPath);
					resultPath += textureRelativePath;
					LoadTexture(TextureType::TextureTypeEmissive, resultPath, i);
				}
				else
					LoadTexture(TextureType::TextureTypeEmissive, Utility::GetFilePath(L"content\\textures\\emptyMetallicMap.png"), i);
			}
			else
				LoadTexture(TextureType::TextureTypeEmissive, Utility::GetFilePath(L"content\\textures\\emptyMetallicMap.png"), i);
		}
	}
	void RenderingObject::LoadCustomMeshTextures(int meshIndex, std::wstring albedoPath, std::wstring normalPath, std::wstring specularPath, std::wstring roughnessPath, std::wstring metallicPath, std::wstring extra1Path, std::wstring extra2Path, std::wstring extra3Path)
	{
		assert(meshIndex < mMeshesCount);
		
		std::string errorMessage = mModel->GetFileName() + " of mesh index: " + std::to_string(meshIndex);

		if (!albedoPath.empty() && albedoPath.back() != '\\')
			LoadTexture(TextureType::TextureTypeDifffuse, albedoPath, meshIndex);

		if (!normalPath.empty() && normalPath.back() != '\\')
			LoadTexture(TextureType::TextureTypeNormalMap, normalPath, meshIndex);

		if (!specularPath.empty() && specularPath.back() != '\\')
			LoadTexture(TextureType::TextureTypeSpecularMap, specularPath, meshIndex);
		
		if (!roughnessPath.empty() && roughnessPath.back() != '\\')
			LoadTexture(TextureType::TextureTypeDisplacementMap, roughnessPath, meshIndex);

		if (!metallicPath.empty() && metallicPath.back() != '\\')
			LoadTexture(TextureType::TextureTypeEmissive, metallicPath, meshIndex);

		//TODO
		//if (!extra1Path.empty())
		//TODO
		//if (!extra2Path.empty())
		//TODO
		//if (!extra3Path.empty())
	}
	void RenderingObject::UpdateGizmos()
	{
		if (!mAvailableInEditorMode)
			return;

		mCameraViewMatrix[0] = mCamera.ViewMatrix4X4()._11;
		mCameraViewMatrix[1] = mCamera.ViewMatrix4X4()._12;
		mCameraViewMatrix[2] = mCamera.ViewMatrix4X4()._13;
		mCameraViewMatrix[3] = mCamera.ViewMatrix4X4()._14;
		mCameraViewMatrix[4] = mCamera.ViewMatrix4X4()._21;
		mCameraViewMatrix[5] = mCamera.ViewMatrix4X4()._22;
		mCameraViewMatrix[6] = mCamera.ViewMatrix4X4()._23;
		mCameraViewMatrix[7] = mCamera.ViewMatrix4X4()._24;
		mCameraViewMatrix[8] = mCamera.ViewMatrix4X4()._31;
		mCameraViewMatrix[9] = mCamera.ViewMatrix4X4()._32;
		mCameraViewMatrix[10] = mCamera.ViewMatrix4X4()._33;
		mCameraViewMatrix[11] = mCamera.ViewMatrix4X4()._34;
		mCameraViewMatrix[12] = mCamera.ViewMatrix4X4()._41;
		mCameraViewMatrix[13] = mCamera.ViewMatrix4X4()._42;
		mCameraViewMatrix[14] = mCamera.ViewMatrix4X4()._43;
		mCameraViewMatrix[15] = mCamera.ViewMatrix4X4()._44;

		mCameraProjectionMatrix[0] = mCamera.ProjectionMatrix4X4()._11;
		mCameraProjectionMatrix[1] = mCamera.ProjectionMatrix4X4()._12;
		mCameraProjectionMatrix[2] = mCamera.ProjectionMatrix4X4()._13;
		mCameraProjectionMatrix[3] = mCamera.ProjectionMatrix4X4()._14;
		mCameraProjectionMatrix[4] = mCamera.ProjectionMatrix4X4()._21;
		mCameraProjectionMatrix[5] = mCamera.ProjectionMatrix4X4()._22;
		mCameraProjectionMatrix[6] = mCamera.ProjectionMatrix4X4()._23;
		mCameraProjectionMatrix[7] = mCamera.ProjectionMatrix4X4()._24;
		mCameraProjectionMatrix[8] = mCamera.ProjectionMatrix4X4()._31;
		mCameraProjectionMatrix[9] = mCamera.ProjectionMatrix4X4()._32;
		mCameraProjectionMatrix[10] = mCamera.ProjectionMatrix4X4()._33;
		mCameraProjectionMatrix[11] = mCamera.ProjectionMatrix4X4()._34;
		mCameraProjectionMatrix[12] = mCamera.ProjectionMatrix4X4()._41;
		mCameraProjectionMatrix[13] = mCamera.ProjectionMatrix4X4()._42;
		mCameraProjectionMatrix[14] = mCamera.ProjectionMatrix4X4()._43;
		mCameraProjectionMatrix[15] = mCamera.ProjectionMatrix4X4()._44;

		UpdateGizmoTransform(mCameraViewMatrix, mCameraProjectionMatrix, mObjectTransformMatrix);
	}
	void RenderingObject::Update(const GameTime & time)
	{
		if (mAvailableInEditorMode && mEnableAABBDebug && Utility::IsEditorMode)
		{
			mDebugAABB->SetPosition(XMFLOAT3(mMatrixTranslation[0],mMatrixTranslation[1],mMatrixTranslation[2]));
			mDebugAABB->SetScale(XMFLOAT3(mMatrixScale[0], mMatrixScale[1], mMatrixScale[2]));
			mDebugAABB->SetRotationMatrix(XMMatrixRotationRollPitchYaw(XMConvertToRadians(mMatrixRotation[0]), XMConvertToRadians(mMatrixRotation[1]), XMConvertToRadians(mMatrixRotation[2])));
			mDebugAABB->Update(time);
		}

	}
	void RenderingObject::LoadTexture(TextureType type, std::wstring path, int meshIndex)
	{
		const wchar_t* postfixDDS = L".dds";
		const wchar_t* postfixDDS_Capital = L".DDS";
		const wchar_t* postfixTGA = L".tga";
		const wchar_t* postfixTGA_Capital = L".TGA";

		bool ddsLoader = (path.substr(path.length() - 4) == std::wstring(postfixDDS)) || (path.substr(path.length() - 4) == std::wstring(postfixDDS_Capital));
		bool tgaLoader = (path.substr(path.length() - 4) == std::wstring(postfixTGA)) || (path.substr(path.length() - 4) == std::wstring(postfixTGA_Capital));
		std::string errorMessage = mModel->GetFileName() + " of mesh index: " + std::to_string(meshIndex);

		std::string texType;
		ID3D11ShaderResourceView** resource;

		switch (type)
		{
		case TextureType::TextureTypeDifffuse:
			texType = "Albedo Texture";
			resource = &(mMeshesTextureBuffers[meshIndex].AlbedoMap);
			break;
		case TextureType::TextureTypeNormalMap:
			texType = "Normal Texture";
			resource = &(mMeshesTextureBuffers[meshIndex].NormalMap);
			break;
		case TextureType::TextureTypeSpecularMap:
			texType = "Specular Texture";
			resource = &(mMeshesTextureBuffers[meshIndex].SpecularMap);
			break;
		case TextureType::TextureTypeEmissive:
			texType = "Metallic Texture";
			resource = &(mMeshesTextureBuffers[meshIndex].MetallicMap);
			break;	
		case TextureType::TextureTypeDisplacementMap:
			texType = "Roughness Texture";
			resource = &(mMeshesTextureBuffers[meshIndex].RoughnessMap);
			break;
		}

		if (ddsLoader)
		{
			if (FAILED(DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), path.c_str(), nullptr, resource)))
			{
				std::string status = "Failed to load DDS Texture" + texType;
				status += errorMessage;
				throw GameException(status.c_str());
			}
		}
		else if (tgaLoader)
		{
			TGATextureLoader* loader = new TGATextureLoader();
			if (!loader->Initialize(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), path.c_str(), resource))
			{
				std::string status = "Failed to load TGA Texture" + texType;
				status += errorMessage;
				throw GameException(status.c_str());
			}
			loader->Shutdown();
		}
		else if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), path.c_str(), nullptr, resource)))
		{
			std::string status = "Failed to load WIC Texture" + texType;
			status += errorMessage;
			throw GameException(status.c_str());
		}
	}

	void RenderingObject::LoadRenderBuffers()
	{
		assert(mModel != nullptr);
		assert(mGame->Direct3DDevice() != nullptr);

		for (size_t materialIndex = 0; materialIndex < mMaterials.size(); materialIndex++)
		{
			mMeshesRenderBuffers.push_back(std::vector<RenderBufferData*>());
			for (size_t i = 0; i < mMeshesCount; i++)
			{
				mMeshesRenderBuffers[materialIndex].push_back(new RenderBufferData());
				mMaterials[materialIndex]->CreateVertexBuffer(mGame->Direct3DDevice(), *mModel->Meshes()[i], &(mMeshesRenderBuffers[materialIndex][i]->VertexBuffer));
				mModel->Meshes()[i]->CreateIndexBuffer(&(mMeshesRenderBuffers[materialIndex][i]->IndexBuffer));
				mMeshesRenderBuffers[materialIndex][i]->IndicesCount = mModel->Meshes()[i]->Indices().size();
				mMeshesRenderBuffers[materialIndex][i]->Stride = mMaterials[materialIndex]->VertexSize();
				mMeshesRenderBuffers[materialIndex][i]->Offset = 0;
			}
		}

	}
	void RenderingObject::LoadInstanceBuffers(std::vector<InstancingMaterial::InstancedData>& pInstanceData, int materialIndex)
	{
		assert(mModel != nullptr);
		assert(mGame->Direct3DDevice() != nullptr);

		mInstanceCount = pInstanceData.size();
		for (size_t i = 0; i < mMeshesCount; i++)
		{
			mMeshesInstanceBuffers.push_back(new InstanceBufferData());
			static_cast<InstancingMaterial*>(mMaterials[materialIndex])->CreateInstanceBuffer(mGame->Direct3DDevice(), pInstanceData, &(mMeshesInstanceBuffers[i]->InstanceBuffer));
			mMeshesInstanceBuffers[i]->Stride = sizeof(InstancingMaterial::InstancedData);
		}
	}
	
	void RenderingObject::Draw(int materialIndex, bool toDepth)
	{
		if (!mMaterials.size() || mMeshesRenderBuffers.size() == 0)
			return;

		for (size_t i = 0; i < mMeshesCount; i++)
		{
			ID3D11DeviceContext* context = mGame->Direct3DDeviceContext();
			if (mWireframeMode)
				context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
			else
				context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			Pass* pass = mMaterials[materialIndex]->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mMaterials[materialIndex]->InputLayouts().at(pass);
			context->IASetInputLayout(inputLayout);

			UINT stride = mMeshesRenderBuffers[materialIndex][i]->Stride;
			UINT offset = mMeshesRenderBuffers[materialIndex][i]->Offset;
			context->IASetVertexBuffers(0, 1, &(mMeshesRenderBuffers[materialIndex][i]->VertexBuffer), &stride, &offset);
			context->IASetIndexBuffer(mMeshesRenderBuffers[materialIndex][i]->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

			for (auto listener : MeshMaterialVariablesUpdateEvent->GetListeners())
				listener(i);

			pass->Apply(0, context);
			context->DrawIndexed(mMeshesRenderBuffers[materialIndex][i]->IndicesCount, 0, 0);
		}


		if (!toDepth && mAvailableInEditorMode)
			DrawAABB();
	}

	void RenderingObject::DrawAABB()
	{
		if (mAvailableInEditorMode && mEnableAABBDebug && Utility::IsEditorMode)
			mDebugAABB->Draw();
	}

	void RenderingObject::DrawInstanced(int materialIndex)
	{
		for (int i = 0; i < mMeshesCount; i++)
		{
			ID3D11DeviceContext* context = mGame->Direct3DDeviceContext();
			Pass* pass = mMaterials[materialIndex]->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mMaterials[materialIndex]->InputLayouts().at(pass);
			context->IASetInputLayout(inputLayout);

			ID3D11Buffer* vertexBuffers[2] = { mMeshesRenderBuffers[materialIndex][i]->VertexBuffer, mMeshesInstanceBuffers[i]->InstanceBuffer };
			UINT strides[2] = { mMeshesRenderBuffers[materialIndex][i]->Stride, mMeshesInstanceBuffers[i]->Stride };
			UINT offsets[2] = { mMeshesRenderBuffers[materialIndex][i]->Offset, mMeshesInstanceBuffers[i]->Offset };

			context->IASetVertexBuffers(0, 2, vertexBuffers, strides, offsets);
			context->IASetIndexBuffer(mMeshesRenderBuffers[materialIndex][i]->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

			for (auto listener : MeshMaterialVariablesUpdateEvent->GetListeners())
				listener(i);

			pass->Apply(0, context);

			context->DrawIndexedInstanced(mMeshesRenderBuffers[materialIndex][i]->IndicesCount, mInstanceCount, 0, 0, 0);
		}
	}

	const std::vector<XMFLOAT3>& RenderingObject::GetVertices()
	{
		return mMeshAllVertices;
	}

	void RenderingObject::UpdateInstanceData(std::vector<InstancingMaterial::InstancedData> pInstanceData, int materialIndex)
	{
		for (size_t i = 0; i < mMeshesCount; i++)
		{
			static_cast<InstancingMaterial*>(mMaterials[materialIndex])->CreateInstanceBuffer(mGame->Direct3DDevice(), pInstanceData, &(mMeshesInstanceBuffers[i]->InstanceBuffer));
			
			// dynamically update instance buffer
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
			
			mGame->Direct3DDeviceContext()->Map(mMeshesInstanceBuffers[i]->InstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			memcpy(mappedResource.pData, &pInstanceData[0], sizeof(pInstanceData[0]) * mInstanceCount);
			mGame->Direct3DDeviceContext()->Unmap(mMeshesInstanceBuffers[i]->InstanceBuffer, 0);

		}
	}

	void RenderingObject::UpdateGizmoTransform(const float *cameraView, float *cameraProjection, float* matrix)
	{
		static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
		static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
		static bool useSnap = false;
		static float snap[3] = { 1.f, 1.f, 1.f };
		static float bounds[] = { -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
		static float boundsSnap[] = { 0.1f, 0.1f, 0.1f };
		static bool boundSizing = false;
		static bool boundSizingSnap = false;

		if (Utility::IsEditorMode) {
			ImGui::Begin("Object Editor");
			ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.24f, 1), mName.c_str());
			ImGui::Separator();
			ImGui::Checkbox("Show AABB", &mEnableAABBDebug);
			ImGui::Checkbox("Wireframe", &mWireframeMode);

			if (ImGui::IsKeyPressed(84))
				mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
			if (ImGui::IsKeyPressed(89))
				mCurrentGizmoOperation = ImGuizmo::ROTATE;
			if (ImGui::IsKeyPressed(82)) // r Key
				mCurrentGizmoOperation = ImGuizmo::SCALE;

			if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
				mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
			ImGui::SameLine();
			if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
				mCurrentGizmoOperation = ImGuizmo::ROTATE;
			ImGui::SameLine();
			if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
				mCurrentGizmoOperation = ImGuizmo::SCALE;


			ImGuizmo::DecomposeMatrixToComponents(matrix, mMatrixTranslation, mMatrixRotation, mMatrixScale);
			ImGui::InputFloat3("Tr", mMatrixTranslation, 3);
			ImGui::InputFloat3("Rt", mMatrixRotation, 3);
			ImGui::InputFloat3("Sc", mMatrixScale, 3);
			ImGuizmo::RecomposeMatrixFromComponents(mMatrixTranslation, mMatrixRotation, mMatrixScale, matrix);
			ImGui::End();



			ImGuiIO& io = ImGui::GetIO();
			ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
			ImGuizmo::Manipulate(cameraView, cameraProjection, mCurrentGizmoOperation, mCurrentGizmoMode, matrix, NULL, useSnap ? &snap[0] : NULL, boundSizing ? bounds : NULL, boundSizingSnap ? boundsSnap : NULL);
		}
	}


}

