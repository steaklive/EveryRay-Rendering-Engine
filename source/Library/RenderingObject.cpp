#include "stdafx.h"
#include <iostream>

#include "RenderingObject.h"
#include "GameException.h"
#include "Model.h"
#include "Mesh.h"
#include "Game.h"
#include "MatrixHelper.h"
#include "Utility.h"

namespace Rendering
{
	RenderingObject::RenderingObject(std::string pName, Game& pGame, Camera& pCamera, std::unique_ptr<Model> pModel, bool availableInEditor, bool isInstanced)
		:
		GameComponent(pGame),
		mCamera(pCamera),
		mModel(std::move(pModel)),
		mMeshesReflectionFactors(0),
		mName(pName),
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

		mMeshesCount.push_back(0); // main LOD
		mMeshVertices.push_back({}); // main LOD
		mMeshAllVertices.push_back({}); // main LOD

		mMeshesCount[0] = mModel->Meshes().size();
		for (size_t i = 0; i < mMeshesCount[0]; i++)
		{
			mMeshVertices[0].push_back(mModel->Meshes().at(i)->Vertices());
			mMeshesTextureBuffers.push_back(TextureData());
			mMeshesReflectionFactors.push_back(0.0f);

			mCustomAlbedoTextures.push_back("");
			mCustomNormalTextures.push_back("");
			mCustomRoughnessTextures.push_back("");
			mCustomMetalnessTextures.push_back("");
		}

		for (size_t i = 0; i < mMeshVertices[0].size(); i++)
		{
			for (size_t j = 0; j < mMeshVertices[0][i].size(); j++)
			{
				mMeshAllVertices[0].push_back(mMeshVertices[0][i][j]);
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

		for (auto lod : mMeshesRenderBuffers)
			for (auto meshRenderBuffer : lod)
				DeletePointerCollection(meshRenderBuffer.second);
		mMeshesRenderBuffers.clear();

		for (auto meshesInstanceBuffersLOD : mMeshesInstanceBuffers)
			DeletePointerCollection(meshesInstanceBuffersLOD);
		mMeshesInstanceBuffers.clear();

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

		for (size_t i = 0; i < mMeshesCount[0]; i++)
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
		assert(meshIndex < mMeshesCount[0]);
		
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

	//from custom collections
	void RenderingObject::LoadCustomMeshTextures(int meshIndex)
	{
		assert(meshIndex < mMeshesCount[0]);
		std::string errorMessage = mModel->GetFileName() + " of mesh index: " + std::to_string(meshIndex);

		if (!mCustomAlbedoTextures[meshIndex].empty() && mCustomAlbedoTextures[meshIndex].back() != '\\')
			LoadTexture(TextureType::TextureTypeDifffuse, Utility::GetFilePath(Utility::ToWideString(mCustomAlbedoTextures[meshIndex])), meshIndex);

		if (!mCustomNormalTextures[meshIndex].empty() && mCustomNormalTextures[meshIndex].back() != '\\')
			LoadTexture(TextureType::TextureTypeNormalMap, Utility::GetFilePath(Utility::ToWideString(mCustomNormalTextures[meshIndex])), meshIndex);

		if (!mCustomRoughnessTextures[meshIndex].empty() && mCustomRoughnessTextures[meshIndex].back() != '\\')
			LoadTexture(TextureType::TextureTypeDisplacementMap, Utility::GetFilePath(Utility::ToWideString(mCustomRoughnessTextures[meshIndex])), meshIndex);

		if (!mCustomMetalnessTextures[meshIndex].empty() && mCustomMetalnessTextures[meshIndex].back() != '\\')
			LoadTexture(TextureType::TextureTypeEmissive, Utility::GetFilePath(Utility::ToWideString(mCustomMetalnessTextures[meshIndex])), meshIndex);

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

		bool failed = false;
		if (ddsLoader)
		{
			if (FAILED(DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), path.c_str(), nullptr, resource)))
			{
				std::string status = "Failed to load DDS Texture" + texType;
				status += errorMessage;
				std::cout << status;
				failed = true;
			}
		}
		else if (tgaLoader)
		{
			TGATextureLoader* loader = new TGATextureLoader();
			if (!loader->Initialize(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), path.c_str(), resource))
			{
				std::string status = "Failed to load TGA Texture" + texType;
				status += errorMessage;
				std::cout << status;
				failed = true;
			}
			loader->Shutdown();
		}
		else if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), path.c_str(), nullptr, resource)))
		{
			std::string status = "Failed to load WIC Texture" + texType;
			status += errorMessage;
			std::cout << status;
			failed = true;
		}

		if (failed) {
			switch (type)
			{
			case TextureType::TextureTypeDifffuse:
				LoadTexture(type, Utility::GetFilePath(L"content\\textures\\emptyDiffuseMap.png"), meshIndex);
				break;
			case TextureType::TextureTypeNormalMap:
				LoadTexture(type, Utility::GetFilePath(L"content\\textures\\emptyNormalMap.png"), meshIndex);
				break;
			case TextureType::TextureTypeEmissive:
				LoadTexture(type, Utility::GetFilePath(L"content\\textures\\emptyMetallicMap.png"), meshIndex);
				break;
			case TextureType::TextureTypeDisplacementMap:
				LoadTexture(type, Utility::GetFilePath(L"content\\textures\\emptyRoughness.png"), meshIndex);
				break;
			}
		}
	}

	void RenderingObject::LoadRenderBuffers(int lod)
	{
		assert(lod < GetLODCount());
		assert(mModel);
		if (lod > 0)
			assert(mModelLODs[lod - 1]);
		assert(mGame->Direct3DDevice() != nullptr);

		mMeshesRenderBuffers.push_back({});
		assert(mMeshesRenderBuffers.size() - 1 == lod);

		for (auto material : mMaterials)
		{
			mMeshesRenderBuffers[lod].insert(std::pair<std::string, std::vector<RenderBufferData*>>(material.first, std::vector<RenderBufferData*>()));
			for (size_t i = 0; i < mMeshesCount[lod]; i++)
			{
				mMeshesRenderBuffers[lod][material.first].push_back(new RenderBufferData());
				if (lod == 0) {
					material.second->CreateVertexBuffer(mGame->Direct3DDevice(), *mModel->Meshes()[i], &(mMeshesRenderBuffers[lod][material.first][i]->VertexBuffer));
					mModel->Meshes()[i]->CreateIndexBuffer(&(mMeshesRenderBuffers[lod][material.first][i]->IndexBuffer));
					mMeshesRenderBuffers[lod][material.first][i]->IndicesCount = mModel->Meshes()[i]->Indices().size();
				}
				else
				{
					material.second->CreateVertexBuffer(mGame->Direct3DDevice(), *mModelLODs[lod - 1]->Meshes()[i], &(mMeshesRenderBuffers[lod][material.first][i]->VertexBuffer));
					mModelLODs[lod - 1]->Meshes()[i]->CreateIndexBuffer(&(mMeshesRenderBuffers[lod][material.first][i]->IndexBuffer));
					mMeshesRenderBuffers[lod][material.first][i]->IndicesCount = mModelLODs[lod - 1]->Meshes()[i]->Indices().size();
				}
				
				mMeshesRenderBuffers[lod][material.first][i]->Stride = mMaterials[material.first]->VertexSize();
				mMeshesRenderBuffers[lod][material.first][i]->Offset = 0;
			}
		}

	}
	
	void RenderingObject::Draw(std::string materialName, bool toDepth, int meshIndex) {
		for (int lod = 0; lod < GetLODCount(); lod++)
			DrawLOD(materialName, toDepth, meshIndex, lod);
	}

	void RenderingObject::DrawLOD(std::string materialName, bool toDepth, int meshIndex, int lod)
	{
		if (mMaterials.find(materialName) == mMaterials.end())
			return;

		if (mIsRendered && !mIsCulled)
		{
			if (!mMaterials.size() || mMeshesRenderBuffers[lod].size() == 0)
				return;
			
			bool isSpecificMesh = (meshIndex != -1);
			for (int i = (isSpecificMesh) ? meshIndex : 0; i < ((isSpecificMesh) ? 1 : mMeshesCount[lod]); i++)
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
					ID3D11Buffer* vertexBuffers[2] = { mMeshesRenderBuffers[lod][materialName][i]->VertexBuffer, mMeshesInstanceBuffers[lod][i]->InstanceBuffer };
					UINT strides[2] = { mMeshesRenderBuffers[lod][materialName][i]->Stride, mMeshesInstanceBuffers[lod][i]->Stride };
					UINT offsets[2] = { mMeshesRenderBuffers[lod][materialName][i]->Offset, mMeshesInstanceBuffers[lod][i]->Offset };

					context->IASetVertexBuffers(0, 2, vertexBuffers, strides, offsets);
					context->IASetIndexBuffer(mMeshesRenderBuffers[lod][materialName][i]->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
				}
				else
				{
					UINT stride = mMeshesRenderBuffers[lod][materialName][i]->Stride;
					UINT offset = mMeshesRenderBuffers[lod][materialName][i]->Offset;
					context->IASetVertexBuffers(0, 1, &(mMeshesRenderBuffers[lod][materialName][i]->VertexBuffer), &stride, &offset);
					context->IASetIndexBuffer(mMeshesRenderBuffers[lod][materialName][i]->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
				}

				for (auto listener : MeshMaterialVariablesUpdateEvent->GetListeners())
					listener(i);

				pass->Apply(0, context);

				if (mIsInstanced)
				{
					if (mInstanceCountToRender[lod] > 0)
						context->DrawIndexedInstanced(mMeshesRenderBuffers[lod][materialName][i]->IndicesCount, mInstanceCountToRender[lod], 0, 0, 0);
					else continue;
				}
				else
					context->DrawIndexed(mMeshesRenderBuffers[lod][materialName][i]->IndicesCount, 0, 0);
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
	void RenderingObject::LoadInstanceBuffers(std::vector<InstancingMaterial::InstancedData>& pInstanceData, std::string materialName, int lod)
	{
		assert(mModel != nullptr);
		assert(mGame->Direct3DDevice() != nullptr);

		mMeshesInstanceBuffers.push_back({});
		assert(lod == mMeshesInstanceBuffers.size() - 1);

		mInstanceCount.push_back(pInstanceData.size());
		assert(lod == mInstanceCount.size() - 1);

		for (size_t i = 0; i < mMeshesCount[lod]; i++)
		{
			mMeshesInstanceBuffers[lod].push_back(new InstanceBufferData());
			static_cast<InstancingMaterial*>(mMaterials[materialName])->CreateInstanceBuffer(mGame->Direct3DDevice(), pInstanceData, &(mMeshesInstanceBuffers[lod][i]->InstanceBuffer));
			mMeshesInstanceBuffers[lod][i]->Stride = sizeof(InstancingMaterial::InstancedData);
		}
	}
	// legacy instancing code [DEPRECATED]
	void RenderingObject::UpdateInstanceData(std::vector<InstancingMaterial::InstancedData> pInstanceData, std::string materialName, int lod)
	{
		assert(lod < mMeshesInstanceBuffers.size());
		assert(lod < mInstanceCount.size());

		for (size_t i = 0; i < mMeshesCount[lod]; i++)
		{
			static_cast<InstancingMaterial*>(mMaterials[materialName])->CreateInstanceBuffer(mGame->Direct3DDevice(), pInstanceData, &(mMeshesInstanceBuffers[lod][i]->InstanceBuffer));
			
			// dynamically update instance buffer
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
			
			mGame->Direct3DDeviceContext()->Map(mMeshesInstanceBuffers[lod][i]->InstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			memcpy(mappedResource.pData, &pInstanceData[0], sizeof(pInstanceData[0]) * mInstanceCount[lod]);
			mGame->Direct3DDeviceContext()->Unmap(mMeshesInstanceBuffers[lod][i]->InstanceBuffer, 0);

		}
	}

	// new instancing code
	void RenderingObject::LoadInstanceBuffers(int lod)
	{
		assert(mModel != nullptr);
		assert(mGame->Direct3DDevice() != nullptr);
		assert(mIsInstanced == true);

		mInstanceData.push_back({});
		assert(lod < mInstanceData.size());

		mInstanceCount.push_back(mInstanceData[lod].size());
		assert(lod == mInstanceCount.size() - 1);
		
		mInstanceCountToRender.push_back(mInstanceData.size());
		assert(lod == mInstanceCountToRender.size() - 1);

		mMeshesInstanceBuffers.push_back({});
		assert(lod == mMeshesInstanceBuffers.size() - 1);

		mInstancesNames.push_back({});
		assert(lod == mInstancesNames.size() - 1);

		//adding extra instance data until we reach MAX_INSTANCE_COUNT 
		for (int i = mInstanceData[lod].size(); i < MAX_INSTANCE_COUNT; i++)
			AddInstanceData(XMMatrixIdentity(), lod);

		for (int i = 0; i < mInstanceCount[lod]; i++)
			mInstancesNames[lod].push_back(mName + " LOD " + std::to_string(lod) + " #" + std::to_string(i));

		//mMeshesInstanceBuffers.clear();
		for (size_t i = 0; i < mMeshesCount[lod]; i++)
		{
			mMeshesInstanceBuffers[lod].push_back(new InstanceBufferData());
			CreateInstanceBuffer(mGame->Direct3DDevice(), &mInstanceData[lod][0], mInstanceCount[lod], &(mMeshesInstanceBuffers[lod][i]->InstanceBuffer));
			mMeshesInstanceBuffers[lod][i]->Stride = sizeof(InstancedData);
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
	void RenderingObject::UpdateInstanceBuffer(std::vector<InstancedData>& instanceData, int lod)
	{
		assert(lod < mMeshesInstanceBuffers.size());

		for (size_t i = 0; i < mMeshesCount[lod]; i++)
		{
			//CreateInstanceBuffer(instanceData);
			mInstanceCountToRender[lod] = instanceData.size();

			// dynamically update instance buffer
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));

			mGame->Direct3DDeviceContext()->Map(mMeshesInstanceBuffers[lod][i]->InstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			memcpy(mappedResource.pData, (instanceData.size() != 0) ? &instanceData[0] : NULL, InstanceSize() * mInstanceCountToRender[lod]);
			mGame->Direct3DDeviceContext()->Unmap(mMeshesInstanceBuffers[lod][i]->InstanceBuffer, 0);

		}
	}

	UINT RenderingObject::InstanceSize() const
	{
		return sizeof(InstancedData);
	}

	//Updates

	void RenderingObject::Update(const GameTime & time)
	{
		for (int lod = 0; lod < GetLODCount(); lod++) {
			if (mIsSelected && mIsInstanced && mInstanceCount[lod] && Utility::IsEditorMode)
			{
				ShowInstancesListUI(lod);
				MatrixHelper::GetFloatArray(mInstanceData[lod][mSelectedInstancedObjectIndex].World, mCurrentObjectTransformMatrix);
			}
		}

		if (mIsSelected)
			UpdateGizmos();

		for (int lod = 0; lod < GetLODCount(); lod++) {
			if (/*mIsSelected &&*/ mIsInstanced)
			{
				mInstanceData[lod][mSelectedInstancedObjectIndex].World = XMFLOAT4X4(mCurrentObjectTransformMatrix);
				if (!Utility::IsCameraCulling)
					UpdateInstanceBuffer(mInstanceData[lod], lod);
				else {
					if (mPostCullingInstanceDataPerLOD[lod].size() > 0)
						UpdateInstanceBuffer(mPostCullingInstanceDataPerLOD[lod], lod);
				}
			}
		}

		if (mAvailableInEditorMode && mEnableAABBDebug && Utility::IsEditorMode && mIsSelected)
		{
			mDebugAABB->SetPosition(XMFLOAT3(mMatrixTranslation[0],mMatrixTranslation[1],mMatrixTranslation[2]));
			mDebugAABB->SetScale(XMFLOAT3(mMatrixScale[0], mMatrixScale[1], mMatrixScale[2]));
			mDebugAABB->SetRotationMatrix(XMMatrixRotationRollPitchYaw(XMConvertToRadians(mMatrixRotation[0]), XMConvertToRadians(mMatrixRotation[1]), XMConvertToRadians(mMatrixRotation[2])));
			mDebugAABB->Update(time);
		}

		if (GetLODCount() > 1)
			UpdateLODs();
	}
	
	void RenderingObject::UpdateGizmos()
	{
		if (!mAvailableInEditorMode)
			return;

		MatrixHelper::GetFloatArray(mCamera.ViewMatrix4X4(), mCameraViewMatrix);
		MatrixHelper::GetFloatArray(mCamera.ProjectionMatrix4X4(), mCameraProjectionMatrix);

		UpdateGizmoTransform(mCameraViewMatrix, mCameraProjectionMatrix, mCurrentObjectTransformMatrix);

		XMFLOAT4X4 mat(mCurrentObjectTransformMatrix);
		mTransformationMatrix = XMLoadFloat4x4(&mat);
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
			std::string name = mName;
			if (mIsCulled)
				name += " (Culled)";

			ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.24f, 1), name.c_str());
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
	
	void RenderingObject::ShowInstancesListUI(int lod)
	{
		assert(mInstanceCount.size() != 0);
		assert(mInstanceCount[lod] != 0);

		std::string title = mName + "LOD " +std::to_string(lod) + " instances:";
		ImGui::Begin(title.c_str());

		for (int i = 0; i < mInstanceCount[lod]; i++)
			listbox_items[i] = mInstancesNames[lod][i].c_str();

		ImGui::PushItemWidth(-1);
		ImGui::ListBox("##empty", &mSelectedInstancedObjectIndex, listbox_items, mInstanceCount[lod], 15);
		ImGui::End();
	}
	
	void RenderingObject::LoadLOD(std::unique_ptr<Model> pModel)
	{
		//assert();
		mMeshesCount.push_back(pModel->Meshes().size());
		mModelLODs.push_back(std::move(pModel));
		mMeshVertices.push_back({});
		mMeshAllVertices.push_back({}); 

		int lodIndex = mMeshesCount.size() - 1;

		for (size_t i = 0; i < mMeshesCount[lodIndex]; i++)
			mMeshVertices[lodIndex].push_back(mModelLODs[lodIndex - 1]->Meshes().at(i)->Vertices());

		for (size_t i = 0; i < mMeshVertices[lodIndex].size(); i++)
		{
			for (size_t j = 0; j < mMeshVertices[lodIndex][i].size(); j++)
			{
				mMeshAllVertices[lodIndex].push_back(mMeshVertices[lodIndex][i][j]);
			}
		}

		LoadRenderBuffers(lodIndex);
	}

	void RenderingObject::ResetInstanceData(int count, bool clear, int lod)
	{
		assert(lod < GetLODCount());

		mInstancesNames[lod].clear();
		mInstanceCount[lod] = count;
		mInstanceCountToRender[lod] = count;

		for (int i = 0; i < mInstanceCount[lod]; i++)
			mInstancesNames[lod].push_back(mName + " LOD " + std::to_string(lod) + " #" + std::to_string(i));

		if (clear)
			mInstanceData[lod].clear();
		
	}
	void RenderingObject::AddInstanceData(XMMATRIX worldMatrix, int lod)
	{
		if (lod == -1) {
			for (int lod = 0; lod < GetLODCount(); lod++)
				mInstanceData[lod].push_back(InstancedData(worldMatrix));
			return;
		}

		assert(lod < mInstanceData.size());
		mInstanceData[lod].push_back(InstancedData(worldMatrix));
	}

	void RenderingObject::UpdateLODs()
	{
		if (mIsInstanced) {
			if (!Utility::IsCameraCulling && mInstanceData.size() == 0)
				return;
			if (Utility::IsCameraCulling && mPostCullingInstanceDataPerLOD.size() == 0)
				return;

			mTempInstanceDataPerLOD.clear();
			for (int lod = 0; lod < GetLODCount(); lod++)
				mTempInstanceDataPerLOD.push_back({});

			//traverse through original or culled instance data (sort of "read-only") to rebalance LOD's instance buffers
			int length = (Utility::IsCameraCulling) ? mPostCullingInstanceDataPerLOD[0].size() : mInstanceData[0].size();
			for (int i = 0; i < length; i++)
			{
				XMFLOAT3 pos;
				XMMATRIX mat = (Utility::IsCameraCulling) ? XMLoadFloat4x4(&mPostCullingInstanceDataPerLOD[0][i].World) : XMLoadFloat4x4(&mInstanceData[0][i].World);
				MatrixHelper::GetTranslation(mat, pos);

				float distanceToCameraSqr =
					(mCamera.Position().x - pos.x) * (mCamera.Position().x - pos.x) +
					(mCamera.Position().y - pos.y) * (mCamera.Position().y - pos.y) +
					(mCamera.Position().z - pos.z) * (mCamera.Position().z - pos.z);

				XMMATRIX newMat;
				if (distanceToCameraSqr <= Utility::DistancesLOD[0] * Utility::DistancesLOD[0]) {
					mTempInstanceDataPerLOD[0].push_back((Utility::IsCameraCulling) ? mPostCullingInstanceDataPerLOD[0][i].World : mInstanceData[0][i].World);
				}
				else if (Utility::DistancesLOD[0] * Utility::DistancesLOD[0] < distanceToCameraSqr && distanceToCameraSqr <= Utility::DistancesLOD[1] * Utility::DistancesLOD[1]) {
					mTempInstanceDataPerLOD[1].push_back((Utility::IsCameraCulling) ? mPostCullingInstanceDataPerLOD[0][i].World : mInstanceData[0][i].World);
				}
				else if (Utility::DistancesLOD[1] * Utility::DistancesLOD[1] < distanceToCameraSqr && distanceToCameraSqr <= Utility::DistancesLOD[2] * Utility::DistancesLOD[2]) {
					mTempInstanceDataPerLOD[2].push_back((Utility::IsCameraCulling) ? mPostCullingInstanceDataPerLOD[0][i].World : mInstanceData[0][i].World);
				}
			}

			for (int i = 0; i < GetLODCount(); i++)
				UpdateInstanceBuffer(mTempInstanceDataPerLOD[i], i);
		}
		//TODO add LOD support for non-instanced objects
	}
}

