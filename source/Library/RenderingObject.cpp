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
	RenderingObject::RenderingObject(std::string pName, Game& pGame, Camera& pCamera, std::unique_ptr<Model> pModel, bool availableInEditor, bool isInstanced)
		:
		GameComponent(pGame),
		mCamera(pCamera),
		mModel(std::move(pModel)),
		mMeshesInstanceBuffers(0, nullptr),
		mMeshesReflectionFactors(0),
		/*mMaterials(0, nullptr),*/
		//mMeshesRenderBuffers(0, std::vector<RenderBufferData*>(0, nullptr)),
		mMeshVertices(0),
		mName(pName),
		mInstanceCount(0),
		mDebugAABB(nullptr),
		mAvailableInEditorMode(availableInEditor),
		mTransformationMatrix(XMMatrixIdentity()),
		mIsInstanced(isInstanced)
	{
		if (!mModel)
		{
			std::string message = "Failed to create a RenderingObject from a model: ";
			message.append(pName);
			throw GameException(message.c_str());
		}

		mMeshesCount = mModel->Meshes().size();
		for (size_t i = 0; i < mMeshesCount; i++)
		{
			mMeshVertices.push_back(mModel->Meshes().at(i)->Vertices());
			mMeshesTextureBuffers.push_back(TextureData());
			mMeshesReflectionFactors.push_back(0.0f);
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

		XMFLOAT4X4 transform = XMFLOAT4X4( mCurrentObjectTransformMatrix );
		mTransformationMatrix = XMLoadFloat4x4(&transform);
	}

	RenderingObject::~RenderingObject()
	{
		DeleteObject(MeshMaterialVariablesUpdateEvent);

		for (auto object : mMaterials)
			DeleteObject(object.second);
		mMaterials.clear();

		for (auto meshRenderBuffer : mMeshesRenderBuffers)
			DeletePointerCollection(meshRenderBuffer.second);
		mMeshesRenderBuffers.clear();

		DeletePointerCollection(mMeshesInstanceBuffers);
		mMeshesTextureBuffers.clear();

		DeleteObject(mDebugAABB);
	}

	void RenderingObject::LoadMaterial(Material* pMaterial, Effect* pEffect, std::string materialName)
	{
		assert(mModel != nullptr);
		assert(pMaterial != nullptr);
		assert(pEffect != nullptr);

		mMaterials.insert(std::pair<std::string, Material*>(materialName, pMaterial));
		mMaterials[materialName]->Initialize(pEffect);
	}

	void RenderingObject::LoadAssignedMeshTextures()
	{
		auto pathBuilder = [&](std::wstring relativePath)
		{
			std::string fullPath;
			Utility::GetDirectory(mModel->GetFileName(), fullPath);
			fullPath += "/";
			std::wstring resultPath;
			Utility::ToWideString(fullPath, resultPath);
			resultPath += relativePath;
			return resultPath;
		};

		for (size_t i = 0; i < mMeshesCount; i++)
		{
			//if (mModel->Meshes()[i]->GetMaterial()->Textures().size() == 0) 
			//	continue;

			std::vector<std::wstring>* texturesAlbedo = mModel->Meshes()[i]->GetMaterial()->GetTexturesByType(TextureType::TextureTypeDifffuse);
			if (texturesAlbedo != nullptr)
			{
				if (texturesAlbedo->size() != 0)
				{
					std::wstring result = pathBuilder(texturesAlbedo->at(0));
					LoadTexture(TextureType::TextureTypeDifffuse, result, i);
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
					std::wstring result = pathBuilder(texturesNormal->at(0));
					LoadTexture(TextureType::TextureTypeNormalMap, result, i);
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
					std::wstring result = pathBuilder(texturesSpec->at(0));
					LoadTexture(TextureType::TextureTypeSpecularMap, result, i);
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
					std::wstring result = pathBuilder(texturesRoughness->at(0));
					LoadTexture(TextureType::TextureTypeDisplacementMap, result, i);
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
					std::wstring result = pathBuilder(texturesMetallic->at(0));
					LoadTexture(TextureType::TextureTypeEmissive, result, i);
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

		for (auto material : mMaterials)
		{
			mMeshesRenderBuffers.insert(std::pair<std::string, std::vector<RenderBufferData*>>(material.first, std::vector<RenderBufferData*>()));
			for (size_t i = 0; i < mMeshesCount; i++)
			{
				mMeshesRenderBuffers[material.first].push_back(new RenderBufferData());
				material.second->CreateVertexBuffer(mGame->Direct3DDevice(), *mModel->Meshes()[i], &(mMeshesRenderBuffers[material.first][i]->VertexBuffer));
				mModel->Meshes()[i]->CreateIndexBuffer(&(mMeshesRenderBuffers[material.first][i]->IndexBuffer));
				mMeshesRenderBuffers[material.first][i]->IndicesCount = mModel->Meshes()[i]->Indices().size();
				mMeshesRenderBuffers[material.first][i]->Stride = mMaterials[material.first]->VertexSize();
				mMeshesRenderBuffers[material.first][i]->Offset = 0;
			}
		}

	}
	
	void RenderingObject::Draw(std::string materialName, bool toDepth, int meshIndex)
	{
		if (mMaterials.find(materialName) == mMaterials.end())
			return;

		if (mIsRendered)
		{
			if (!mMaterials.size() || mMeshesRenderBuffers.size() == 0)
				return;
			
			bool isSpecificMesh = (meshIndex != -1);
			for (int i = (isSpecificMesh) ? meshIndex : 0; i < ((isSpecificMesh) ? 1 : mMeshesCount); i++)
			{
				ID3D11DeviceContext* context = mGame->Direct3DDeviceContext();
				if (mWireframeMode)
					context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
				else
					context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				Pass* pass = mMaterials[materialName]->CurrentTechnique()->Passes().at(0);
				ID3D11InputLayout* inputLayout = mMaterials[materialName]->InputLayouts().at(pass);
				context->IASetInputLayout(inputLayout);

				if (mIsInstanced)
				{
					ID3D11Buffer* vertexBuffers[2] = { mMeshesRenderBuffers[materialName][i]->VertexBuffer, mMeshesInstanceBuffers[i]->InstanceBuffer };
					UINT strides[2] = { mMeshesRenderBuffers[materialName][i]->Stride, mMeshesInstanceBuffers[i]->Stride };
					UINT offsets[2] = { mMeshesRenderBuffers[materialName][i]->Offset, mMeshesInstanceBuffers[i]->Offset };

					context->IASetVertexBuffers(0, 2, vertexBuffers, strides, offsets);
					context->IASetIndexBuffer(mMeshesRenderBuffers[materialName][i]->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
				}
				else
				{
					UINT stride = mMeshesRenderBuffers[materialName][i]->Stride;
					UINT offset = mMeshesRenderBuffers[materialName][i]->Offset;
					context->IASetVertexBuffers(0, 1, &(mMeshesRenderBuffers[materialName][i]->VertexBuffer), &stride, &offset);
					context->IASetIndexBuffer(mMeshesRenderBuffers[materialName][i]->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
				}

				for (auto listener : MeshMaterialVariablesUpdateEvent->GetListeners())
					listener(i);

				pass->Apply(0, context);

				if (mIsInstanced)
					context->DrawIndexedInstanced(mMeshesRenderBuffers[materialName][i]->IndicesCount, mInstanceCountToRender, 0, 0, 0);
				else
					context->DrawIndexed(mMeshesRenderBuffers[materialName][i]->IndicesCount, 0, 0);
			}


			if (!toDepth && mAvailableInEditorMode && mIsSelected)
				DrawAABB();
		}
	}

	void RenderingObject::DrawAABB()
	{
		if (mAvailableInEditorMode && mEnableAABBDebug && Utility::IsEditorMode)
			mDebugAABB->Draw();
	}


	// legacy instancing code [DEPRECATED]
	void RenderingObject::LoadInstanceBuffers(std::vector<InstancingMaterial::InstancedData>& pInstanceData, std::string materialName)
	{
		assert(mModel != nullptr);
		assert(mGame->Direct3DDevice() != nullptr);

		mInstanceCount = pInstanceData.size();
		for (size_t i = 0; i < mMeshesCount; i++)
		{
			mMeshesInstanceBuffers.push_back(new InstanceBufferData());
			static_cast<InstancingMaterial*>(mMaterials[materialName])->CreateInstanceBuffer(mGame->Direct3DDevice(), pInstanceData, &(mMeshesInstanceBuffers[i]->InstanceBuffer));
			mMeshesInstanceBuffers[i]->Stride = sizeof(InstancingMaterial::InstancedData);
		}
	}
	// legacy instancing code [DEPRECATED]
	void RenderingObject::UpdateInstanceData(std::vector<InstancingMaterial::InstancedData> pInstanceData, std::string materialName)
	{
		for (size_t i = 0; i < mMeshesCount; i++)
		{
			static_cast<InstancingMaterial*>(mMaterials[materialName])->CreateInstanceBuffer(mGame->Direct3DDevice(), pInstanceData, &(mMeshesInstanceBuffers[i]->InstanceBuffer));
			
			// dynamically update instance buffer
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
			
			mGame->Direct3DDeviceContext()->Map(mMeshesInstanceBuffers[i]->InstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			memcpy(mappedResource.pData, &pInstanceData[0], sizeof(pInstanceData[0]) * mInstanceCount);
			mGame->Direct3DDeviceContext()->Unmap(mMeshesInstanceBuffers[i]->InstanceBuffer, 0);

		}
	}

	// new instancing code
	void RenderingObject::LoadInstanceBuffers()
	{
		assert(mModel != nullptr);
		assert(mGame->Direct3DDevice() != nullptr);
		//assert(mInstanceData.size() != 0);
		assert(mIsInstanced == true);

		//adding extra instance data until we reach MAX_INSTANCE_COUNT 
		for (int i = mInstanceData.size(); i < MAX_INSTANCE_COUNT; i++)
			AddInstanceData(XMMatrixIdentity());

		mInstanceCount = mInstanceData.size();
		for (int i = 0; i < mInstanceCount; i++)
			mInstancesNames.push_back(mName + " " + std::to_string(i));

		//mMeshesInstanceBuffers.clear();
		for (size_t i = 0; i < mMeshesCount; i++)
		{
			mMeshesInstanceBuffers.push_back(new InstanceBufferData());
			CreateInstanceBuffer(mGame->Direct3DDevice(), &mInstanceData[0], mInstanceCount, &(mMeshesInstanceBuffers[i]->InstanceBuffer));
			mMeshesInstanceBuffers[i]->Stride = sizeof(InstancedData);
		}
	}
	// new instancing code
	void RenderingObject::CreateInstanceBuffer(ID3D11Device* device, InstancedData* instanceData, UINT instanceCount, ID3D11Buffer** instanceBuffer)
	{
		D3D11_BUFFER_DESC instanceBufferDesc;
		ZeroMemory(&instanceBufferDesc, sizeof(instanceBufferDesc));
		instanceBufferDesc.ByteWidth = InstanceSize()/* *instanceCount*/ * MAX_INSTANCE_COUNT;
		instanceBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		instanceBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		instanceBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA instanceSubResourceData;
		ZeroMemory(&instanceSubResourceData, sizeof(instanceSubResourceData));
		instanceSubResourceData.pSysMem = instanceData;
		if (FAILED(device->CreateBuffer(&instanceBufferDesc, &instanceSubResourceData, instanceBuffer)))
		{
			throw GameException("ID3D11Device::CreateBuffer() failed while creating InstanceBuffer in RenderObject.");
		}
	}
	// new instancing code
	void RenderingObject::UpdateInstanceBuffer(std::vector<InstancedData>& instanceData)
	{
		for (size_t i = 0; i < mMeshesCount; i++)
		{
			//CreateInstanceBuffer(instanceData);
			mInstanceCountToRender = instanceData.size();

			// dynamically update instance buffer
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));

			mGame->Direct3DDeviceContext()->Map(mMeshesInstanceBuffers[i]->InstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			memcpy(mappedResource.pData, &instanceData[0], sizeof(instanceData[0]) * mInstanceCountToRender);
			mGame->Direct3DDeviceContext()->Unmap(mMeshesInstanceBuffers[i]->InstanceBuffer, 0);

		}
	}

	UINT RenderingObject::InstanceSize() const
	{
		return sizeof(InstancedData);
	}

	//Updates

	void RenderingObject::Update(const GameTime & time)
	{
		if (mIsSelected && mIsInstanced && mInstanceCount && Utility::IsEditorMode) 
		{
			ShowInstancesListUI();
			MatrixHelper::GetFloatArray(mInstanceData[mSelectedInstancedObjectIndex].World, mCurrentObjectTransformMatrix);
		}

		if (mIsSelected)
			UpdateGizmos();

		if (mIsSelected && mIsInstanced)
		{
			mInstanceData[mSelectedInstancedObjectIndex].World = XMFLOAT4X4(mCurrentObjectTransformMatrix);
			if (!Utility::IsCameraCulling) //otherwise camera will load proper instance data
				UpdateInstanceBuffer(mInstanceData);
		}

		if (mAvailableInEditorMode && mEnableAABBDebug && Utility::IsEditorMode && mIsSelected)
		{
			mDebugAABB->SetPosition(XMFLOAT3(mMatrixTranslation[0],mMatrixTranslation[1],mMatrixTranslation[2]));
			mDebugAABB->SetScale(XMFLOAT3(mMatrixScale[0], mMatrixScale[1], mMatrixScale[2]));
			mDebugAABB->SetRotationMatrix(XMMatrixRotationRollPitchYaw(XMConvertToRadians(mMatrixRotation[0]), XMConvertToRadians(mMatrixRotation[1]), XMConvertToRadians(mMatrixRotation[2])));
			mDebugAABB->Update(time);
		}

	}
	
	void RenderingObject::UpdateGizmos()
	{
		if (!mAvailableInEditorMode)
			return;

		MatrixHelper::GetFloatArray(mCamera.ViewMatrix4X4(), mCameraViewMatrix);
		MatrixHelper::GetFloatArray(mCamera.ProjectionMatrix4X4(), mCameraProjectionMatrix);

		UpdateGizmoTransform(mCameraViewMatrix, mCameraProjectionMatrix, mCurrentObjectTransformMatrix);
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

			Utility::IsLightEditor = false;

			ImGui::Begin("Object Editor");
			ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.24f, 1), mName.c_str());
			ImGui::Separator();
			if (!mIsInstanced)
				ImGui::Checkbox("Visible", &mIsRendered);
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
	
	void RenderingObject::ShowInstancesListUI()
	{
		assert(mInstanceCount != 0);

		std::string title = mName + " instances:";
		ImGui::Begin(title.c_str());

		for (int i = 0; i < mInstanceCount; i++)
			listbox_items[i] = mInstancesNames[i].c_str();

		ImGui::PushItemWidth(-1);
		ImGui::ListBox("##empty", &mSelectedInstancedObjectIndex, listbox_items, mInstanceCount, 15);
		ImGui::End();
	}
	
	// temp for testing
	// TODO move to Utility
	void RenderingObject::CalculateInstanceObjectsRandomDistribution(int count)
	{
		mInstanceCount = count;
		mInstanceCountToRender = count;
		const float size = 100.0f;
		for (size_t i = 0; i < count; i++)
		{
			// random position
			float x = (rand() / (float)(RAND_MAX)) * size - size / 2;
			float y = 0.0f;
			float z = (rand() / (float)(RAND_MAX)) * size - size / 2;

			float scale = 0.5f + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / (1.0f - 0.5f)));

			mInstanceData.push_back(InstancedData(XMMatrixScaling(scale, scale, scale) *  XMMatrixTranslation(x, y, z)));
			//mDynamicObjectInstancesPositions.push_back(XMFLOAT3(x, y, z));

		}
	}

	void RenderingObject::ResetInstanceData(int count, bool clear)
	{
		mInstanceCount = count;
		mInstanceCountToRender = count;

		mInstancesNames.clear();
		for (int i = 0; i < mInstanceCount; i++)
			mInstancesNames.push_back(mName + " " + std::to_string(i));

		if (clear)
			mInstanceData.clear();
	}
	void RenderingObject::AddInstanceData(XMMATRIX worldMatrix)
	{
		mInstanceData.push_back(InstancedData(worldMatrix));
	}
}

