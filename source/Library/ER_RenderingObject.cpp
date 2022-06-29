#include "stdafx.h"
#include <iostream>

#include "ER_RenderingObject.h"
#include "ER_CoreComponent.h"
#include "GameException.h"
#include "Game.h"
#include "GameTime.h"
#include "ER_Model.h"
#include "ER_Mesh.h"
#include "Utility.h"
#include "ER_Illumination.h"
#include "ER_RenderableAABB.h"
#include "ER_Material.h"
#include "ER_Camera.h"
#include "MatrixHelper.h"
#include "ER_Terrain.h"

namespace Library
{
	static int currentSplatChannnel = (int)TerrainSplatChannels::NONE;

	ER_RenderingObject::ER_RenderingObject(const std::string& pName, int index, Game& pGame, ER_Camera& pCamera, std::unique_ptr<ER_Model> pModel, bool availableInEditor, bool isInstanced)
		:
		mGame(&pGame),
		mCamera(pCamera),
		mModel(std::move(pModel)),
		mMeshesReflectionFactors(0),
		mName(pName),
		mDebugGizmoAABB(nullptr),
		mAvailableInEditorMode(availableInEditor),
		mTransformationMatrix(XMMatrixIdentity()),
		mIsInstanced(isInstanced),
		mIndexInScene(index)
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
			mMeshVertices[0].push_back(mModel->GetMesh(i).Vertices());
			mMeshesTextureBuffers.push_back(TextureData());
			mMeshesReflectionFactors.push_back(0.0f);

			mCustomAlbedoTextures.push_back("");
			mCustomNormalTextures.push_back("");
			mCustomRoughnessTextures.push_back("");
			mCustomMetalnessTextures.push_back("");
			mCustomHeightTextures.push_back("");
			mCustomReflectionMaskTextures.push_back("");
		}

		for (size_t i = 0; i < mMeshVertices[0].size(); i++)
		{
			for (size_t j = 0; j < mMeshVertices[0][i].size(); j++)
			{
				mMeshAllVertices[0].push_back(mMeshVertices[0][i][j]);
			}
		}

		LoadAssignedMeshTextures();
		mLocalAABB = mModel->GenerateAABB();
		mGlobalAABB = mLocalAABB;

		if (mAvailableInEditorMode) {
			mDebugGizmoAABB = new ER_RenderableAABB(*mGame, XMFLOAT4{ 0.0f, 0.0f, 1.0f, 1.0f });
			mDebugGizmoAABB->InitializeGeometry({ mLocalAABB.first, mLocalAABB.second });
		}

		XMFLOAT4X4 transform = XMFLOAT4X4( mCurrentObjectTransformMatrix );
		mTransformationMatrix = XMLoadFloat4x4(&transform);
	}

	ER_RenderingObject::~ER_RenderingObject()
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

		DeleteObject(mDebugGizmoAABB);
	}

	void ER_RenderingObject::LoadMaterial(ER_Material* pMaterial, const std::string& materialName)
	{
		assert(pMaterial);
		mMaterials.emplace(materialName, pMaterial);
	}

	void ER_RenderingObject::LoadAssignedMeshTextures()
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
			if (mModel->GetMesh(i).GetMaterial().HasTexturesOfType(TextureType::TextureTypeDifffuse))
			{
				const std::vector<std::wstring>& texturesAlbedo = mModel->GetMesh(i).GetMaterial().GetTexturesByType(TextureType::TextureTypeDifffuse);
				if (texturesAlbedo.size() != 0)
				{
					std::wstring result = pathBuilder(texturesAlbedo.at(0));
					LoadTexture(TextureType::TextureTypeDifffuse, result, i);
				}
				else
					LoadTexture(TextureType::TextureTypeDifffuse, Utility::GetFilePath(L"content\\textures\\emptyDiffuseMap.png"), i);
			}
			else
				LoadTexture(TextureType::TextureTypeDifffuse, Utility::GetFilePath(L"content\\textures\\emptyDiffuseMap.png"), i);

			if (mModel->GetMesh(i).GetMaterial().HasTexturesOfType(TextureType::TextureTypeNormalMap))
			{
				const std::vector<std::wstring>& texturesNormal = mModel->GetMesh(i).GetMaterial().GetTexturesByType(TextureType::TextureTypeNormalMap);
				if (texturesNormal.size() != 0)
				{
					std::wstring result = pathBuilder(texturesNormal.at(0));
					LoadTexture(TextureType::TextureTypeNormalMap, result, i);
				}
				else
					LoadTexture(TextureType::TextureTypeNormalMap, Utility::GetFilePath(L"content\\textures\\emptyNormalMap.jpg"), i);
			}
			else
				LoadTexture(TextureType::TextureTypeNormalMap, Utility::GetFilePath(L"content\\textures\\emptyNormalMap.jpg"), i);

			if (mModel->GetMesh(i).GetMaterial().HasTexturesOfType(TextureType::TextureTypeSpecularMap))
			{
				const std::vector<std::wstring>& texturesSpec = mModel->GetMesh(i).GetMaterial().GetTexturesByType(TextureType::TextureTypeSpecularMap);
				if (texturesSpec.size() != 0)
				{
					std::wstring result = pathBuilder(texturesSpec.at(0));
					LoadTexture(TextureType::TextureTypeSpecularMap, result, i);
				}
				else
					LoadTexture(TextureType::TextureTypeSpecularMap, Utility::GetFilePath(L"content\\textures\\emptySpecularMap.png"), i);
			}
			else
				LoadTexture(TextureType::TextureTypeSpecularMap, Utility::GetFilePath(L"content\\textures\\emptySpecularMap.png"), i);

			if (mModel->GetMesh(i).GetMaterial().HasTexturesOfType(TextureType::TextureTypeDisplacementMap))
			{
				const std::vector<std::wstring>& texturesRoughness = mModel->GetMesh(i).GetMaterial().GetTexturesByType(TextureType::TextureTypeDisplacementMap);
				if (texturesRoughness.size() != 0)
				{
					std::wstring result = pathBuilder(texturesRoughness.at(0));
					LoadTexture(TextureType::TextureTypeDisplacementMap, result, i);
				}
				else
					LoadTexture(TextureType::TextureTypeDisplacementMap, Utility::GetFilePath(L"content\\textures\\emptyRoughnessMap.png"), i);
			}
			else
				LoadTexture(TextureType::TextureTypeDisplacementMap, Utility::GetFilePath(L"content\\textures\\emptyRoughnessMap.png"), i);

			if (mModel->GetMesh(i).GetMaterial().HasTexturesOfType(TextureType::TextureTypeEmissive))
			{
				const std::vector<std::wstring>& texturesMetallic = mModel->GetMesh(i).GetMaterial().GetTexturesByType(TextureType::TextureTypeEmissive);
				if (texturesMetallic.size() != 0)
				{
					std::wstring result = pathBuilder(texturesMetallic.at(0));
					LoadTexture(TextureType::TextureTypeEmissive, result, i);
				}
				else
					LoadTexture(TextureType::TextureTypeEmissive, Utility::GetFilePath(L"content\\textures\\emptyMetallicMap.png"), i);
			}
			else
				LoadTexture(TextureType::TextureTypeEmissive, Utility::GetFilePath(L"content\\textures\\emptyMetallicMap.png"), i);
		}
	}
	
	//from custom collections
	void ER_RenderingObject::LoadCustomMeshTextures(int meshIndex)
	{
		assert(meshIndex < mMeshesCount[0]);
		std::string errorMessage = mModel->GetFileName() + " of mesh index: " + std::to_string(meshIndex);

		if (!mCustomAlbedoTextures[meshIndex].empty())
			if (mCustomAlbedoTextures[meshIndex].back() != '\\')
				LoadTexture(TextureType::TextureTypeDifffuse, Utility::GetFilePath(Utility::ToWideString(mCustomAlbedoTextures[meshIndex])), meshIndex);

		if (!mCustomNormalTextures[meshIndex].empty())
			if (mCustomNormalTextures[meshIndex].back() != '\\')
				LoadTexture(TextureType::TextureTypeNormalMap, Utility::GetFilePath(Utility::ToWideString(mCustomNormalTextures[meshIndex])), meshIndex);

		if (!mCustomRoughnessTextures[meshIndex].empty())
			if (mCustomRoughnessTextures[meshIndex].back() != '\\')
				LoadTexture(TextureType::TextureTypeDisplacementMap, Utility::GetFilePath(Utility::ToWideString(mCustomRoughnessTextures[meshIndex])), meshIndex);

		if (!mCustomMetalnessTextures[meshIndex].empty())
			if (mCustomMetalnessTextures[meshIndex].back() != '\\')
				LoadTexture(TextureType::TextureTypeEmissive, Utility::GetFilePath(Utility::ToWideString(mCustomMetalnessTextures[meshIndex])), meshIndex);

		if (!mCustomHeightTextures[meshIndex].empty())
			if (mCustomHeightTextures[meshIndex].back() != '\\')
				LoadTexture(TextureType::TextureTypeHeightmap, Utility::GetFilePath(Utility::ToWideString(mCustomHeightTextures[meshIndex])), meshIndex);

		if (!mCustomReflectionMaskTextures[meshIndex].empty())
			if (mCustomReflectionMaskTextures[meshIndex].back() != '\\')
				LoadTexture(TextureType::TextureTypeLightMap, Utility::GetFilePath(Utility::ToWideString(mCustomReflectionMaskTextures[meshIndex])), meshIndex);

		//TODO
		//if (!extra2Path.empty())
		//TODO
		//if (!extra3Path.empty())
	}
	
	void ER_RenderingObject::LoadTexture(TextureType type, const std::wstring& path, int meshIndex)
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
		case TextureType::TextureTypeHeightmap:
			texType = "Height Texture";
			resource = &(mMeshesTextureBuffers[meshIndex].HeightMap);
			break;	
		case TextureType::TextureTypeLightMap:
			texType = "Reflection Mask Texture";
			resource = &(mMeshesTextureBuffers[meshIndex].ReflectionMaskMap);
			break;
		}

		bool failed = false;
		if (ddsLoader)
		{
			if (FAILED(DirectX::CreateDDSTextureFromFile(mGame->Direct3DDevice(), path.c_str(), nullptr, resource)))
			{
				std::string status = "Failed to load DDS Texture" + texType;
				status += errorMessage;
				std::cout << status;
				failed = true;
			}
		}
		else if (tgaLoader)
		{
			//TODO This will not work if accessed from multiple threads, since we need a device context for TGA loader (not thread safe)
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
				LoadTexture(type, Utility::GetFilePath(L"content\\textures\\emptyNormalMap.jpg"), meshIndex);
				break;
			case TextureType::TextureTypeEmissive:
				LoadTexture(type, Utility::GetFilePath(L"content\\textures\\emptyMetallicMap.png"), meshIndex);
				break;
			case TextureType::TextureTypeDisplacementMap:
				LoadTexture(type, Utility::GetFilePath(L"content\\textures\\emptyRoughness.png"), meshIndex);
			default:
				LoadTexture(type, Utility::GetFilePath(L"content\\textures\\emptyDiffuseMap.png"), meshIndex);
				break;
			}
		}
	}

	//TODO refactor (remove duplicated code)
	void ER_RenderingObject::LoadRenderBuffers(int lod)
	{
		assert(lod < GetLODCount());
		assert(mModel);

		mMeshesRenderBuffers.push_back({});
		assert(mMeshesRenderBuffers.size() - 1 == lod);

		auto createIndexBuffer = [this](const ER_Mesh& aMesh, int meshIndex, int lod, const std::string& materialName) {
			aMesh.CreateIndexBuffer(&(mMeshesRenderBuffers[lod][materialName][meshIndex]->IndexBuffer));
			mMeshesRenderBuffers[lod][materialName][meshIndex]->IndicesCount = aMesh.Indices().size();
		};

		for (auto material : mMaterials)
		{
			mMeshesRenderBuffers[lod].insert(std::pair<std::string, std::vector<RenderBufferData*>>(material.first, std::vector<RenderBufferData*>()));
			for (size_t i = 0; i < mMeshesCount[lod]; i++)
			{
				mMeshesRenderBuffers[lod][material.first].push_back(new RenderBufferData());

				material.second->CreateVertexBuffer((lod == 0) ? mModel->GetMesh(i) : mModelLODs[lod - 1]->GetMesh(i), &(mMeshesRenderBuffers[lod][material.first][i]->VertexBuffer));
				createIndexBuffer((lod == 0) ? mModel->GetMesh(i) : mModelLODs[lod - 1]->GetMesh(i), i, lod, material.first);

				mMeshesRenderBuffers[lod][material.first][i]->Stride = mMaterials[material.first]->VertexSize();
				mMeshesRenderBuffers[lod][material.first][i]->Offset = 0;
			}
		}

		//special case for forward lighting (non-material case)
		if (mIsForwardShading)
		{
			mMeshesRenderBuffers[lod].insert(std::pair<std::string, std::vector<RenderBufferData*>>(ER_MaterialHelper::forwardLightingNonMaterialName, std::vector<RenderBufferData*>()));
			for (size_t i = 0; i < mMeshesCount[lod]; i++)
			{
				mMeshesRenderBuffers[lod][ER_MaterialHelper::forwardLightingNonMaterialName].push_back(new RenderBufferData());

				if (lod == 0)
					mModel->GetMesh(i).CreateVertexBuffer_PositionUvNormalTangent(&(mMeshesRenderBuffers[lod][ER_MaterialHelper::forwardLightingNonMaterialName][i]->VertexBuffer));
				else
					mModelLODs[lod - 1]->GetMesh(i).CreateVertexBuffer_PositionUvNormalTangent(&(mMeshesRenderBuffers[lod][ER_MaterialHelper::forwardLightingNonMaterialName][i]->VertexBuffer));

				createIndexBuffer((lod == 0) ? mModel->GetMesh(i) : mModelLODs[lod - 1]->GetMesh(i), i, lod, ER_MaterialHelper::forwardLightingNonMaterialName);

				mMeshesRenderBuffers[lod][ER_MaterialHelper::forwardLightingNonMaterialName][i]->Stride = sizeof(VertexPositionTextureNormalTangent);
				mMeshesRenderBuffers[lod][ER_MaterialHelper::forwardLightingNonMaterialName][i]->Offset = 0;
			}
		}
	}
	
	void ER_RenderingObject::Draw(const std::string& materialName, bool toDepth, int meshIndex) {
		
		// for instanced objects we run DrawLOD() for all available LODs (some instances might end up in one LOD, others in other LODs)
		if (mIsInstanced)
		{
			for (int lod = 0; lod < GetLODCount(); lod++)
				DrawLOD(materialName, toDepth, meshIndex, lod);
		}
		else
			DrawLOD(materialName, toDepth, meshIndex, mCurrentLODIndex);
	}

	void ER_RenderingObject::DrawLOD(const std::string& materialName, bool toDepth, int meshIndex, int lod)
	{
		bool isForwardPass = materialName == ER_MaterialHelper::forwardLightingNonMaterialName && mIsForwardShading;

		ID3D11DeviceContext* context = mGame->Direct3DDeviceContext();
		context->IASetPrimitiveTopology(mWireframeMode ? D3D11_PRIMITIVE_TOPOLOGY_LINELIST : D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		if (mMaterials.find(materialName) == mMaterials.end() && !isForwardPass)
			return;
		
		if (mIsRendered && !mIsCulled && mCurrentLODIndex != -1)
		{
			if (!isForwardPass && (!mMaterials.size() || mMeshesRenderBuffers[lod].size() == 0))
				return;
			
			bool isSpecificMesh = (meshIndex != -1);
			for (int i = (isSpecificMesh) ? meshIndex : 0; i < ((isSpecificMesh) ? meshIndex + 1 : mMeshesCount[lod]); i++)
			{
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

				// run prepare callbacks for non-special materials (specials are, i.e., shadow mapping, which are processed in their own systems)
				if (!isForwardPass && !mMaterials[materialName]->IsSpecial())
				{
					auto prepareMaterialBeforeRendering = MeshMaterialVariablesUpdateEvent->GetListener(materialName);
					if (prepareMaterialBeforeRendering)
						prepareMaterialBeforeRendering(i);
				}
				else if (isForwardPass && mGame->GetLevel()->mIllumination)
					mGame->GetLevel()->mIllumination->PrepareForForwardLighting(this, i);

				if (mIsInstanced)
				{
					if (mInstanceCountToRender[lod] > 0)
						context->DrawIndexedInstanced(mMeshesRenderBuffers[lod][materialName][i]->IndicesCount, mInstanceCountToRender[lod], 0, 0, 0);
					else 
						continue;
				}
				else
					context->DrawIndexed(mMeshesRenderBuffers[lod][materialName][i]->IndicesCount, 0, 0);
			}
		}
	}

	void ER_RenderingObject::DrawAABB()
	{
		if (mIsSelected && mAvailableInEditorMode && mEnableAABBDebug && Utility::IsEditorMode)
			mDebugGizmoAABB->Draw();
	}

	void ER_RenderingObject::SetTransformationMatrix(const XMMATRIX& mat)
	{
		mTransformationMatrix = mat;
		MatrixHelper::GetFloatArray(mTransformationMatrix, mCurrentObjectTransformMatrix);
	}

	void ER_RenderingObject::SetTranslation(float x, float y, float z)
	{
		mTransformationMatrix *= XMMatrixTranslation(x, y, z);
		MatrixHelper::GetFloatArray(mTransformationMatrix, mCurrentObjectTransformMatrix);
	}

	void ER_RenderingObject::SetScale(float x, float y, float z)
	{
		mTransformationMatrix *= XMMatrixScaling(x, y, z);
		MatrixHelper::GetFloatArray(mTransformationMatrix, mCurrentObjectTransformMatrix);
	}

	void ER_RenderingObject::SetRotation(float x, float y, float z)
	{
		mTransformationMatrix *= XMMatrixRotationRollPitchYaw(x, y, z);
		MatrixHelper::GetFloatArray(mTransformationMatrix, mCurrentObjectTransformMatrix);
	}

	// new instancing code
	void ER_RenderingObject::LoadInstanceBuffers(int lod)
	{
		assert(mModel != nullptr);
		assert(mGame->Direct3DDevice() != nullptr);
		assert(mIsInstanced == true);

		mInstanceData.push_back({});
		assert(lod < mInstanceData.size());

		mInstanceCountToRender.push_back(0);
		assert(lod == mInstanceCountToRender.size() - 1);

		mMeshesInstanceBuffers.push_back({});
		assert(lod == mMeshesInstanceBuffers.size() - 1);

		//adding extra instance data until we reach MAX_INSTANCE_COUNT 
		for (int i = mInstanceData[lod].size(); i < MAX_INSTANCE_COUNT; i++)
			AddInstanceData(XMMatrixIdentity(), lod);

		//mMeshesInstanceBuffers.clear();
		for (size_t i = 0; i < mMeshesCount[lod]; i++)
		{
			mMeshesInstanceBuffers[lod].push_back(new InstanceBufferData());
			CreateInstanceBuffer(mGame->Direct3DDevice(), &mInstanceData[lod][0], MAX_INSTANCE_COUNT, &(mMeshesInstanceBuffers[lod][i]->InstanceBuffer));
			mMeshesInstanceBuffers[lod][i]->Stride = sizeof(InstancedData);
		}
	}
	// new instancing code
	void ER_RenderingObject::CreateInstanceBuffer(ID3D11Device* device, InstancedData* instanceData, UINT instanceCount, ID3D11Buffer** instanceBuffer)
	{
		if (instanceCount > MAX_INSTANCE_COUNT)
			throw GameException("Instances count limit is exceeded!");

		D3D11_BUFFER_DESC instanceBufferDesc;
		ZeroMemory(&instanceBufferDesc, sizeof(instanceBufferDesc));
		instanceBufferDesc.ByteWidth = InstanceSize() * MAX_INSTANCE_COUNT;
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
	void ER_RenderingObject::UpdateInstanceBuffer(std::vector<InstancedData>& instanceData, int lod)
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

	UINT ER_RenderingObject::InstanceSize() const
	{
		return sizeof(InstancedData);
	}

	void ER_RenderingObject::PerformCPUFrustumCull(ER_Camera* camera)
	{
		auto frustum = camera->GetFrustum();
		auto cullFunction = [frustum](ER_AABB& aabb) {
			bool culled = false;
			// start a loop through all frustum planes
			for (int planeID = 0; planeID < 6; ++planeID)
			{
				XMVECTOR planeNormal = XMVectorSet(frustum.Planes()[planeID].x, frustum.Planes()[planeID].y, frustum.Planes()[planeID].z, 0.0f);
				float planeConstant = frustum.Planes()[planeID].w;

				XMFLOAT3 axisVert;

				// x-axis
				if (frustum.Planes()[planeID].x > 0.0f)
					axisVert.x = aabb.first.x;
				else
					axisVert.x = aabb.second.x;

				// y-axis
				if (frustum.Planes()[planeID].y > 0.0f)
					axisVert.y = aabb.first.y;
				else
					axisVert.y = aabb.second.y;

				// z-axis
				if (frustum.Planes()[planeID].z > 0.0f)
					axisVert.z = aabb.first.z;
				else
					axisVert.z = aabb.second.z;


				if (XMVectorGetX(XMVector3Dot(planeNormal, XMLoadFloat3(&axisVert))) + planeConstant > 0.0f)
				{
					culled = true;
					// Skip remaining planes to check and move on 
					break;
				}
			}
			return culled;
		};

		assert(mInstanceCullingFlags.size() == mInstanceCount);

		if (mIsInstanced)
		{
			XMMATRIX instanceWorldMatrix = XMMatrixIdentity();
			const int currentLOD = 0; // no need to iterate through LODs (AABBs are shared between LODs, so culling results will be identical)

			mTempPostCullingInstanceData.clear();
			{
				std::vector<InstancedData> newInstanceData;
				for (int instanceIndex = 0; instanceIndex < mInstanceCount; instanceIndex++)
				{
					instanceWorldMatrix = XMLoadFloat4x4(&(mInstanceData[currentLOD][instanceIndex].World));
					mInstanceCullingFlags[instanceIndex] = cullFunction(mInstanceAABBs[instanceIndex]);
					if (!mInstanceCullingFlags[instanceIndex])
						newInstanceData.push_back(instanceWorldMatrix);
				}
				mTempPostCullingInstanceData = newInstanceData; //we store a copy for future usages

				//update every LOD group with new instance data after CPU frustum culling
				for (int lodIndex = 0; lodIndex < GetLODCount(); lodIndex++)
					UpdateInstanceBuffer(mTempPostCullingInstanceData, lodIndex);
			}
		}
		else
			mIsCulled = cullFunction(mGlobalAABB);
	}

	// Placement on terrain based on object's properties defined in level file (instance count, terrain splat, object scale variation, etc.)
	// This method is not supposed to run every frame, but during initialization or on request
	void ER_RenderingObject::PlaceProcedurallyOnTerrain()
	{
		ER_Terrain* terrain = mGame->GetLevel()->mTerrain;
		if (!terrain || !terrain->IsLoaded())
			return;

		if (!mIsInstanced)
		{
			XMFLOAT4 currentPos;
			MatrixHelper::GetTranslation(XMLoadFloat4x4(&(XMFLOAT4X4(mCurrentObjectTransformMatrix))), currentPos);
			terrain->PlaceOnTerrain(&currentPos, 1, (TerrainSplatChannels)mTerrainProceduralPlacementSplatChannel);

			MatrixHelper::SetTranslation(mTransformationMatrix, XMFLOAT3(currentPos.x, currentPos.y, currentPos.z));
			SetTransformationMatrix(mTransformationMatrix);
		}
		else
		{
			XMFLOAT4* instancesPositions = new XMFLOAT4[mInstanceCount];
			for (int instanceI = 0; instanceI < mInstanceCount; instanceI++)
			{
				instancesPositions[instanceI] = XMFLOAT4(
					mTerrainProceduralZoneCenterPos.x + Utility::RandomFloat(-mTerrainProceduralZoneRadius, mTerrainProceduralZoneRadius),
					mTerrainProceduralZoneCenterPos.y,
					mTerrainProceduralZoneCenterPos.z + Utility::RandomFloat(-mTerrainProceduralZoneRadius, mTerrainProceduralZoneRadius), 1.0f);
			}
			terrain->PlaceOnTerrain(instancesPositions, mInstanceCount, (TerrainSplatChannels)mTerrainProceduralPlacementSplatChannel);
			XMMATRIX worldMatrix = XMMatrixIdentity();

			for (int lod = 0; lod < GetLODCount(); lod++)
			{
				for (int instanceI = 0; instanceI < mInstanceCount; instanceI++)
				{
					float scale = Utility::RandomFloat(mTerrainProceduralObjectMinScale, mTerrainProceduralObjectMaxScale);
					float roll = Utility::RandomFloat(mTerrainProceduralObjectMinRoll, mTerrainProceduralObjectMaxRoll);
					float pitch = Utility::RandomFloat(mTerrainProceduralObjectMinPitch, mTerrainProceduralObjectMaxPitch);
					float yaw = Utility::RandomFloat(mTerrainProceduralObjectMinYaw, mTerrainProceduralObjectMaxYaw);

					worldMatrix = XMMatrixScaling(scale, scale, scale) * XMMatrixRotationRollPitchYaw(pitch, yaw, roll);
					MatrixHelper::SetTranslation(worldMatrix, XMFLOAT3(instancesPositions[instanceI].x, instancesPositions[instanceI].y, instancesPositions[instanceI].z));
					XMStoreFloat4x4(&(mInstanceData[lod][instanceI].World), worldMatrix);
					worldMatrix = XMMatrixIdentity();
				}
				UpdateInstanceBuffer(mInstanceData[lod], lod);
			}
			DeleteObjects(instancesPositions);
		}
		mIsTerrainPlacementFinished = true;
		
	}
	void ER_RenderingObject::Update(const GameTime& time)
	{
		ER_Camera* camera = (ER_Camera*)(mGame->Services().GetService(ER_Camera::TypeIdClass()));
		assert(camera);

		bool editable = Utility::IsEditorMode && mAvailableInEditorMode && mIsSelected;

		if (editable && mIsInstanced)
		{
			// load current selected instance's transform to temp transform (for UI)
			MatrixHelper::GetFloatArray(mInstanceData[0][mEditorSelectedInstancedObjectIndex].World, mCurrentObjectTransformMatrix);
		}

		// place procedurally on terrain (only executed once, on load)
		if (mIsTerrainPlacement && !mIsTerrainPlacementFinished)
			PlaceProcedurallyOnTerrain();

		//update AABBs (global and instanced)
		{
			mGlobalAABB = mLocalAABB;
			UpdateAABB(mGlobalAABB, mTransformationMatrix);

			if (mIsInstanced)
			{
				XMMATRIX instanceWorldMatrix = XMMatrixIdentity();
				for (int instanceIndex = 0; instanceIndex < mInstanceCount; instanceIndex++)
				{
					instanceWorldMatrix = XMLoadFloat4x4(&(mInstanceData[0][instanceIndex].World));
					mInstanceAABBs[instanceIndex] = mLocalAABB;
					UpdateAABB(mInstanceAABBs[instanceIndex], instanceWorldMatrix);
				}
			}
		}

		if (Utility::IsMainCameraCPUFrustumCulling && camera)
			PerformCPUFrustumCull(camera);
		else
		{
			if (mIsInstanced)
			{
				//just updating transforms (that could be changed in a previous frame); this is not optimal (GPU buffer map() every frame...)
				for (int lod = 0; lod < GetLODCount(); lod++)
					UpdateInstanceBuffer(mInstanceData[lod], lod);
			}
		}

		if (GetLODCount() > 1)
			UpdateLODs();

		if (editable)
		{
			UpdateGizmos();
			ShowInstancesListWindow();
			if (mEnableAABBDebug)
				mDebugGizmoAABB->Update(mGlobalAABB);
		}
	}

	void ER_RenderingObject::UpdateAABB(ER_AABB& aabb, const XMMATRIX& transformMatrix)
	{
		// computing AABB from the non-axis aligned BB
		mCurrentGlobalAABBVertices[0] = (XMFLOAT3(aabb.first.x, aabb.second.y, aabb.first.z));
		mCurrentGlobalAABBVertices[1] = (XMFLOAT3(aabb.second.x, aabb.second.y, aabb.first.z));
		mCurrentGlobalAABBVertices[2] = (XMFLOAT3(aabb.second.x, aabb.first.y, aabb.first.z));
		mCurrentGlobalAABBVertices[3] = (XMFLOAT3(aabb.first.x, aabb.first.y, aabb.first.z));
		mCurrentGlobalAABBVertices[4] = (XMFLOAT3(aabb.first.x, aabb.second.y, aabb.second.z));
		mCurrentGlobalAABBVertices[5] = (XMFLOAT3(aabb.second.x, aabb.second.y, aabb.second.z));
		mCurrentGlobalAABBVertices[6] = (XMFLOAT3(aabb.second.x, aabb.first.y, aabb.second.z));
		mCurrentGlobalAABBVertices[7] = (XMFLOAT3(aabb.first.x, aabb.first.y, aabb.second.z));

		// non-axis-aligned BB (applying transform)
		for (size_t i = 0; i < 8; i++)
		{
			XMVECTOR point = XMVector3Transform(XMLoadFloat3(&(mCurrentGlobalAABBVertices[i])), transformMatrix);
			XMStoreFloat3(&(mCurrentGlobalAABBVertices[i]), point);
		}

		XMFLOAT3 minVertex = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
		XMFLOAT3 maxVertex = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		for (UINT i = 0; i < 8; i++)
		{
			//Get the smallest vertex 
			minVertex.x = std::min(minVertex.x, mCurrentGlobalAABBVertices[i].x);    // Find smallest x value in model
			minVertex.y = std::min(minVertex.y, mCurrentGlobalAABBVertices[i].y);    // Find smallest y value in model
			minVertex.z = std::min(minVertex.z, mCurrentGlobalAABBVertices[i].z);    // Find smallest z value in model

			//Get the largest vertex 
			maxVertex.x = std::max(maxVertex.x, mCurrentGlobalAABBVertices[i].x);    // Find largest x value in model
			maxVertex.y = std::max(maxVertex.y, mCurrentGlobalAABBVertices[i].y);    // Find largest y value in model
			maxVertex.z = std::max(maxVertex.z, mCurrentGlobalAABBVertices[i].z);    // Find largest z value in model
		}

		aabb = ER_AABB(minVertex, maxVertex);
	}
	
	void ER_RenderingObject::UpdateGizmos()
	{
		if (!(mAvailableInEditorMode && mIsSelected))
			return;

		MatrixHelper::GetFloatArray(mCamera.ViewMatrix4X4(), mCameraViewMatrix);
		MatrixHelper::GetFloatArray(mCamera.ProjectionMatrix4X4(), mCameraProjectionMatrix);

		ShowObjectsEditorWindow(mCameraViewMatrix, mCameraProjectionMatrix, mCurrentObjectTransformMatrix);

		XMFLOAT4X4 mat(mCurrentObjectTransformMatrix);
		mTransformationMatrix = XMLoadFloat4x4(&mat);

		//update instance world transform (from editor's gizmo/UI)
		if (mIsInstanced && Utility::IsEditorMode)
		{
			for (int lod = 0; lod < GetLODCount(); lod++)
				mInstanceData[lod][mEditorSelectedInstancedObjectIndex].World = XMFLOAT4X4(mCurrentObjectTransformMatrix);
		}
	}
	
	// Shows and updates an ImGui/ImGizmo window for objects editor.
	// You can edit transform (via UI or via gizmo), you can move to object, set/read some flags, get some useful info, etc.
	void ER_RenderingObject::ShowObjectsEditorWindow(const float *cameraView, float *cameraProjection, float* matrix)
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
			Utility::IsFoliageEditor = false;

			ImGui::Begin("Object Editor");
			std::string name = mName;
			if (mIsInstanced)
			{
				name = mInstancesNames[mEditorSelectedInstancedObjectIndex];
				if (mInstanceCullingFlags[mEditorSelectedInstancedObjectIndex]) //showing info for main LOD only in editor
					name += " (Culled)";
			}
			else
			{
				name += " LOD #" + std::to_string(mCurrentLODIndex);
				if (mIsCulled)
					name += " (Culled)";
			}

			ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.24f, 1), name.c_str());

			ImGui::Separator();
			std::string lodCountText = "* LOD count: " + std::to_string(GetLODCount());
			ImGui::Text(lodCountText.c_str());
			for (int lodI = 0; lodI < GetLODCount(); lodI++)
			{
				std::string vertexCountText = "--> Vertex count LOD#" + std::to_string(lodI) + ": " + std::to_string(GetVertices(lodI).size());
				ImGui::Text(vertexCountText.c_str());
			}

			std::string meshCountText = "* Mesh count: " + std::to_string(GetMeshCount());
			ImGui::Text(meshCountText.c_str());

			std::string instanceCountText = "* Instance count: " + std::to_string(GetInstanceCount());
			ImGui::Text(instanceCountText.c_str());

			std::string shadingModeName = "* Shaded in: ";
			if (mIsForwardShading)
				shadingModeName += "Forward";
			else 
				shadingModeName += "Deferred";
			ImGui::Text(shadingModeName.c_str());

			ImGui::Separator();
			ImGui::Checkbox("Rendered", &mIsRendered);
			ImGui::Checkbox("Show AABB", &mEnableAABBDebug);
			ImGui::Checkbox("Show Wireframe", &mWireframeMode);
			if (ImGui::Button("Move camera to"))
			{
				XMFLOAT3 newCameraPos;
				MatrixHelper::GetTranslation(XMLoadFloat4x4(&(XMFLOAT4X4(mCurrentObjectTransformMatrix))), newCameraPos);

				ER_Camera* camera = (ER_Camera*)(mGame->Services().GetService(ER_Camera::TypeIdClass()));
				if (camera)
					camera->SetPosition(newCameraPos);
			}

			//terrain
			{
				ImGui::Combo("Terrain splat channel", &currentSplatChannnel, DisplayedSplatChannnelNames, 5);
				TerrainSplatChannels currentChannel = (TerrainSplatChannels)currentSplatChannnel;
				ER_Terrain* terrain = mGame->GetLevel()->mTerrain;

				if (ImGui::Button("Place on terrain") && terrain && terrain->IsLoaded())
				{
					XMFLOAT4 currentPos;
					MatrixHelper::GetTranslation(XMLoadFloat4x4(&(XMFLOAT4X4(mCurrentObjectTransformMatrix))), currentPos);
					terrain->PlaceOnTerrain(&currentPos, 1, currentChannel);

					mMatrixTranslation[0] = currentPos.x;
					mMatrixTranslation[1] = currentPos.y;
					mMatrixTranslation[2] = currentPos.z;
					ImGuizmo::RecomposeMatrixFromComponents(mMatrixTranslation, mMatrixRotation, mMatrixScale, matrix);
				}
			}

			//Transforms
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
	
	// Shows an ImGui window for instances list.
	// You can select an instance, read some useful info about it and edit it via "Objects Editor".
	// We do not need to edit per LOD (LODs share same transforms, AABBs, names).
	void ER_RenderingObject::ShowInstancesListWindow()
	{
		if (!(mAvailableInEditorMode && mIsSelected && mIsInstanced))
			return;

		assert(mInstanceCount != 0);
		assert(mInstanceData[0].size() != 0 && mInstanceData[0].size() == mInstanceCount);
		assert(mInstancesNames.size() == mInstanceCount);

		std::string title = mName + " instances:";
		ImGui::Begin(title.c_str());

		for (int i = 0; i < mInstanceData[0].size(); i++) {
			mInstancedNamesUI[i] = mInstancesNames[i].c_str();
		}

		ImGui::PushItemWidth(-1);
		ImGui::ListBox("##empty", &mEditorSelectedInstancedObjectIndex, mInstancedNamesUI, mInstanceData[0].size(), 15);
		ImGui::End();
	}
	
	void ER_RenderingObject::LoadLOD(std::unique_ptr<ER_Model> pModel)
	{
		//assert();
		mMeshesCount.push_back(pModel->Meshes().size());
		mModelLODs.push_back(std::move(pModel));
		mMeshVertices.push_back({});
		mMeshAllVertices.push_back({}); 

		int lodIndex = mMeshesCount.size() - 1;

		for (size_t i = 0; i < mMeshesCount[lodIndex]; i++)
			mMeshVertices[lodIndex].push_back(mModelLODs[lodIndex - 1]->GetMesh(i).Vertices());

		for (size_t i = 0; i < mMeshVertices[lodIndex].size(); i++)
		{
			for (size_t j = 0; j < mMeshVertices[lodIndex][i].size(); j++)
			{
				mMeshAllVertices[lodIndex].push_back(mMeshVertices[lodIndex][i][j]);
			}
		}

		LoadRenderBuffers(lodIndex);
	}

	void ER_RenderingObject::ResetInstanceData(int count, bool clear, int lod)
	{
		assert(lod < GetLODCount());

		mInstanceCountToRender[lod] = count;

		if (lod == 0)
		{
			mInstanceCount = count;
			for (int i = 0; i < mInstanceCount; i++)
			{
				std::string instanceName = mName + " #" + std::to_string(i);
				mInstancesNames.push_back(instanceName);
				mInstanceAABBs.push_back(mLocalAABB);
				mInstanceCullingFlags.push_back(false);
			}
		}

		if (clear)
			mInstanceData[lod].clear();
	}
	void ER_RenderingObject::AddInstanceData(const XMMATRIX& worldMatrix, int lod)
	{
		if (lod == -1) {
			for (int lod = 0; lod < GetLODCount(); lod++)
				mInstanceData[lod].push_back(InstancedData(worldMatrix));
			return;
		}

		assert(lod < mInstanceData.size());
		mInstanceData[lod].push_back(InstancedData(worldMatrix));
	}

	void ER_RenderingObject::UpdateLODs()
	{
		if (mIsInstanced) {
			if (!Utility::IsMainCameraCPUFrustumCulling && mInstanceData.size() == 0)
				return;
			if (Utility::IsMainCameraCPUFrustumCulling && mTempPostCullingInstanceData.size() == 0)
				return;

			mTempPostLoddingInstanceData.clear();
			for (int lod = 0; lod < GetLODCount(); lod++)
				mTempPostLoddingInstanceData.push_back({});

			//traverse through original or culled instance data (sort of "read-only") to rebalance LOD's instance buffers
			int length = (Utility::IsMainCameraCPUFrustumCulling) ? mTempPostCullingInstanceData.size() : mInstanceData[0].size();
			for (int i = 0; i < length; i++)
			{
				XMFLOAT3 pos;
				XMMATRIX mat = (Utility::IsMainCameraCPUFrustumCulling) ? XMLoadFloat4x4(&mTempPostCullingInstanceData[i].World) : XMLoadFloat4x4(&mInstanceData[0][i].World);
				MatrixHelper::GetTranslation(mat, pos);

				float distanceToCameraSqr =
					(mCamera.Position().x - pos.x) * (mCamera.Position().x - pos.x) +
					(mCamera.Position().y - pos.y) * (mCamera.Position().y - pos.y) +
					(mCamera.Position().z - pos.z) * (mCamera.Position().z - pos.z);

				XMMATRIX newMat;
				if (distanceToCameraSqr <= Utility::DistancesLOD[0] * Utility::DistancesLOD[0]) {
					mTempPostLoddingInstanceData[0].push_back((Utility::IsMainCameraCPUFrustumCulling) ? mTempPostCullingInstanceData[i].World : mInstanceData[0][i].World);
				}
				else if (Utility::DistancesLOD[0] * Utility::DistancesLOD[0] < distanceToCameraSqr && distanceToCameraSqr <= Utility::DistancesLOD[1] * Utility::DistancesLOD[1]) {
					mTempPostLoddingInstanceData[1].push_back((Utility::IsMainCameraCPUFrustumCulling) ? mTempPostCullingInstanceData[i].World : mInstanceData[0][i].World);
				}
				else if (Utility::DistancesLOD[1] * Utility::DistancesLOD[1] < distanceToCameraSqr && distanceToCameraSqr <= Utility::DistancesLOD[2] * Utility::DistancesLOD[2]) {
					mTempPostLoddingInstanceData[2].push_back((Utility::IsMainCameraCPUFrustumCulling) ? mTempPostCullingInstanceData[i].World : mInstanceData[0][i].World);
				}
			}

			for (int i = 0; i < GetLODCount(); i++)
				UpdateInstanceBuffer(mTempPostLoddingInstanceData[i], i);
		}
		else
		{
			XMFLOAT3 pos;
			MatrixHelper::GetTranslation(mTransformationMatrix, pos);

			float distanceToCameraSqr =
				(mCamera.Position().x - pos.x) * (mCamera.Position().x - pos.x) +
				(mCamera.Position().y - pos.y) * (mCamera.Position().y - pos.y) +
				(mCamera.Position().z - pos.z) * (mCamera.Position().z - pos.z);

			if (distanceToCameraSqr <= Utility::DistancesLOD[0] * Utility::DistancesLOD[0]) {
				mCurrentLODIndex = 0;
			}
			else if (Utility::DistancesLOD[0] * Utility::DistancesLOD[0] < distanceToCameraSqr && distanceToCameraSqr <= Utility::DistancesLOD[1] * Utility::DistancesLOD[1]) {
				mCurrentLODIndex = 1;
			}
			else if (Utility::DistancesLOD[1] * Utility::DistancesLOD[1] < distanceToCameraSqr && distanceToCameraSqr <= Utility::DistancesLOD[2] * Utility::DistancesLOD[2]) {
				mCurrentLODIndex = 2;
			}
			else
				mCurrentLODIndex = -1; //culled

			mCurrentLODIndex = std::min(mCurrentLODIndex, GetLODCount());
		}
	}
}

