#include "stdafx.h"
#include <iostream>

#include "ER_RenderingObject.h"
#include "ER_CoreComponent.h"
#include "ER_CoreException.h"
#include "ER_Core.h"
#include "ER_CoreTime.h"
#include "ER_Model.h"
#include "ER_Mesh.h"
#include "ER_Utility.h"
#include "ER_Illumination.h"
#include "ER_RenderableAABB.h"
#include "ER_Material.h"
#include "ER_Camera.h"
#include "ER_MatrixHelper.h"
#include "ER_Terrain.h"

namespace EveryRay_Core
{
	static int currentSplatChannnel = (int)TerrainSplatChannels::NONE;

	ER_RenderingObject::ER_RenderingObject(const std::string& pName, int index, ER_Core& pCore, ER_Camera& pCamera, std::unique_ptr<ER_Model> pModel, bool availableInEditor, bool isInstanced)
		:
		mCore(&pCore),
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
			throw ER_CoreException(message.c_str());
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
			mDebugGizmoAABB = new ER_RenderableAABB(*mCore, XMFLOAT4{ 0.0f, 0.0f, 1.0f, 1.0f });
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
			ER_Utility::GetDirectory(mModel->GetFileName(), fullPath);
			fullPath += "/";
			std::wstring resultPath;
			ER_Utility::ToWideString(fullPath, resultPath);
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
					LoadTexture(TextureType::TextureTypeDifffuse, ER_Utility::GetFilePath(L"content\\textures\\emptyDiffuseMap.png"), i);
			}
			else
				LoadTexture(TextureType::TextureTypeDifffuse, ER_Utility::GetFilePath(L"content\\textures\\emptyDiffuseMap.png"), i);

			if (mModel->GetMesh(i).GetMaterial().HasTexturesOfType(TextureType::TextureTypeNormalMap))
			{
				const std::vector<std::wstring>& texturesNormal = mModel->GetMesh(i).GetMaterial().GetTexturesByType(TextureType::TextureTypeNormalMap);
				if (texturesNormal.size() != 0)
				{
					std::wstring result = pathBuilder(texturesNormal.at(0));
					LoadTexture(TextureType::TextureTypeNormalMap, result, i);
				}
				else
					LoadTexture(TextureType::TextureTypeNormalMap, ER_Utility::GetFilePath(L"content\\textures\\emptyNormalMap.jpg"), i);
			}
			else
				LoadTexture(TextureType::TextureTypeNormalMap, ER_Utility::GetFilePath(L"content\\textures\\emptyNormalMap.jpg"), i);

			if (mModel->GetMesh(i).GetMaterial().HasTexturesOfType(TextureType::TextureTypeSpecularMap))
			{
				const std::vector<std::wstring>& texturesSpec = mModel->GetMesh(i).GetMaterial().GetTexturesByType(TextureType::TextureTypeSpecularMap);
				if (texturesSpec.size() != 0)
				{
					std::wstring result = pathBuilder(texturesSpec.at(0));
					LoadTexture(TextureType::TextureTypeSpecularMap, result, i);
				}
				else
					LoadTexture(TextureType::TextureTypeSpecularMap, ER_Utility::GetFilePath(L"content\\textures\\emptySpecularMap.png"), i);
			}
			else
				LoadTexture(TextureType::TextureTypeSpecularMap, ER_Utility::GetFilePath(L"content\\textures\\emptySpecularMap.png"), i);

			if (mModel->GetMesh(i).GetMaterial().HasTexturesOfType(TextureType::TextureTypeDisplacementMap))
			{
				const std::vector<std::wstring>& texturesRoughness = mModel->GetMesh(i).GetMaterial().GetTexturesByType(TextureType::TextureTypeDisplacementMap);
				if (texturesRoughness.size() != 0)
				{
					std::wstring result = pathBuilder(texturesRoughness.at(0));
					LoadTexture(TextureType::TextureTypeDisplacementMap, result, i);
				}
				else
					LoadTexture(TextureType::TextureTypeDisplacementMap, ER_Utility::GetFilePath(L"content\\textures\\emptyRoughnessMap.png"), i);
			}
			else
				LoadTexture(TextureType::TextureTypeDisplacementMap, ER_Utility::GetFilePath(L"content\\textures\\emptyRoughnessMap.png"), i);

			if (mModel->GetMesh(i).GetMaterial().HasTexturesOfType(TextureType::TextureTypeEmissive))
			{
				const std::vector<std::wstring>& texturesMetallic = mModel->GetMesh(i).GetMaterial().GetTexturesByType(TextureType::TextureTypeEmissive);
				if (texturesMetallic.size() != 0)
				{
					std::wstring result = pathBuilder(texturesMetallic.at(0));
					LoadTexture(TextureType::TextureTypeEmissive, result, i);
				}
				else
					LoadTexture(TextureType::TextureTypeEmissive, ER_Utility::GetFilePath(L"content\\textures\\emptyMetallicMap.png"), i);
			}
			else
				LoadTexture(TextureType::TextureTypeEmissive, ER_Utility::GetFilePath(L"content\\textures\\emptyMetallicMap.png"), i);
		}
	}
	
	//from custom collections
	void ER_RenderingObject::LoadCustomMeshTextures(int meshIndex)
	{
		assert(meshIndex < mMeshesCount[0]);
		std::string errorMessage = mModel->GetFileName() + " of mesh index: " + std::to_string(meshIndex);

		if (!mCustomAlbedoTextures[meshIndex].empty())
			if (mCustomAlbedoTextures[meshIndex].back() != '\\')
				LoadTexture(TextureType::TextureTypeDifffuse, ER_Utility::GetFilePath(ER_Utility::ToWideString(mCustomAlbedoTextures[meshIndex])), meshIndex);

		if (!mCustomNormalTextures[meshIndex].empty())
			if (mCustomNormalTextures[meshIndex].back() != '\\')
				LoadTexture(TextureType::TextureTypeNormalMap, ER_Utility::GetFilePath(ER_Utility::ToWideString(mCustomNormalTextures[meshIndex])), meshIndex);

		if (!mCustomRoughnessTextures[meshIndex].empty())
			if (mCustomRoughnessTextures[meshIndex].back() != '\\')
				LoadTexture(TextureType::TextureTypeDisplacementMap, ER_Utility::GetFilePath(ER_Utility::ToWideString(mCustomRoughnessTextures[meshIndex])), meshIndex);

		if (!mCustomMetalnessTextures[meshIndex].empty())
			if (mCustomMetalnessTextures[meshIndex].back() != '\\')
				LoadTexture(TextureType::TextureTypeEmissive, ER_Utility::GetFilePath(ER_Utility::ToWideString(mCustomMetalnessTextures[meshIndex])), meshIndex);

		if (!mCustomHeightTextures[meshIndex].empty())
			if (mCustomHeightTextures[meshIndex].back() != '\\')
				LoadTexture(TextureType::TextureTypeHeightmap, ER_Utility::GetFilePath(ER_Utility::ToWideString(mCustomHeightTextures[meshIndex])), meshIndex);

		if (!mCustomReflectionMaskTextures[meshIndex].empty())
			if (mCustomReflectionMaskTextures[meshIndex].back() != '\\')
				LoadTexture(TextureType::TextureTypeLightMap, ER_Utility::GetFilePath(ER_Utility::ToWideString(mCustomReflectionMaskTextures[meshIndex])), meshIndex);

		//TODO
		//if (!extra2Path.empty())
		//TODO
		//if (!extra3Path.empty())
	}
	
	void ER_RenderingObject::LoadTexture(TextureType type, const std::wstring& path, int meshIndex)
	{
		ER_RHI* rhi = mCore->GetRHI();

		const wchar_t* postfixDDS = L".dds";
		const wchar_t* postfixDDS_Capital = L".DDS";
		const wchar_t* postfixTGA = L".tga";
		const wchar_t* postfixTGA_Capital = L".TGA";

		bool ddsLoader = (path.substr(path.length() - 4) == std::wstring(postfixDDS)) || (path.substr(path.length() - 4) == std::wstring(postfixDDS_Capital));
		bool tgaLoader = (path.substr(path.length() - 4) == std::wstring(postfixTGA)) || (path.substr(path.length() - 4) == std::wstring(postfixTGA_Capital));
		std::string errorMessage = mModel->GetFileName() + " of mesh index: " + std::to_string(meshIndex);

		switch (type)
		{
		case TextureType::TextureTypeDifffuse:
		{
			mMeshesTextureBuffers[meshIndex].AlbedoMap = rhi->CreateGPUTexture();
			mMeshesTextureBuffers[meshIndex].AlbedoMap->CreateGPUTextureResource(rhi, path, true);
			break;
		}
		case TextureType::TextureTypeNormalMap:
		{
			mMeshesTextureBuffers[meshIndex].NormalMap = rhi->CreateGPUTexture();
			mMeshesTextureBuffers[meshIndex].NormalMap->CreateGPUTextureResource(rhi, path, true);
			break;
		}
		case TextureType::TextureTypeSpecularMap:
		{
			mMeshesTextureBuffers[meshIndex].SpecularMap = rhi->CreateGPUTexture();
			mMeshesTextureBuffers[meshIndex].SpecularMap->CreateGPUTextureResource(rhi, path, true);
			break;
		}
		case TextureType::TextureTypeEmissive:
		{
			mMeshesTextureBuffers[meshIndex].MetallicMap = rhi->CreateGPUTexture();
			mMeshesTextureBuffers[meshIndex].MetallicMap->CreateGPUTextureResource(rhi, path, true);
			break;	
		}
		case TextureType::TextureTypeDisplacementMap:
		{
			mMeshesTextureBuffers[meshIndex].RoughnessMap = rhi->CreateGPUTexture();
			mMeshesTextureBuffers[meshIndex].RoughnessMap->CreateGPUTextureResource(rhi, path, true);
			break;
		}
		case TextureType::TextureTypeHeightmap:
		{
			mMeshesTextureBuffers[meshIndex].HeightMap = rhi->CreateGPUTexture();
			mMeshesTextureBuffers[meshIndex].HeightMap->CreateGPUTextureResource(rhi, path, true);
			break;	
		}
		case TextureType::TextureTypeLightMap:
		{
			mMeshesTextureBuffers[meshIndex].ReflectionMaskMap = rhi->CreateGPUTexture();
			mMeshesTextureBuffers[meshIndex].ReflectionMaskMap->CreateGPUTextureResource(rhi, path, true);
			break;
		}
		}
	}

	//TODO refactor (remove duplicated code)
	void ER_RenderingObject::LoadRenderBuffers(int lod)
	{
		assert(lod < GetLODCount());
		assert(mModel);
		ER_RHI* rhi = mCore->GetRHI();

		mMeshesRenderBuffers.push_back({});
		assert(mMeshesRenderBuffers.size() - 1 == lod);

		auto createIndexBuffer = [this, rhi](const ER_Mesh& aMesh, int meshIndex, int lod, const std::string& materialName) {
			mMeshesRenderBuffers[lod][materialName][meshIndex]->IndexBuffer = rhi->CreateGPUBuffer();
			aMesh.CreateIndexBuffer(mMeshesRenderBuffers[lod][materialName][meshIndex]->IndexBuffer);
			mMeshesRenderBuffers[lod][materialName][meshIndex]->IndicesCount = aMesh.Indices().size();
		};

		for (auto material : mMaterials)
		{
			mMeshesRenderBuffers[lod].insert(std::pair<std::string, std::vector<RenderBufferData*>>(material.first, std::vector<RenderBufferData*>()));
			for (size_t i = 0; i < mMeshesCount[lod]; i++)
			{
				mMeshesRenderBuffers[lod][material.first].push_back(new RenderBufferData());
				mMeshesRenderBuffers[lod][material.first][i]->VertexBuffer = rhi->CreateGPUBuffer();
				material.second->CreateVertexBuffer((lod == 0) ? mModel->GetMesh(i) : mModelLODs[lod - 1]->GetMesh(i), mMeshesRenderBuffers[lod][material.first][i]->VertexBuffer);
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
				mMeshesRenderBuffers[lod][ER_MaterialHelper::forwardLightingNonMaterialName][i]->VertexBuffer = rhi->CreateGPUBuffer();

				if (lod == 0)
					mModel->GetMesh(i).CreateVertexBuffer_PositionUvNormalTangent(mMeshesRenderBuffers[lod][ER_MaterialHelper::forwardLightingNonMaterialName][i]->VertexBuffer);
				else
					mModelLODs[lod - 1]->GetMesh(i).CreateVertexBuffer_PositionUvNormalTangent(mMeshesRenderBuffers[lod][ER_MaterialHelper::forwardLightingNonMaterialName][i]->VertexBuffer);

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

		ER_RHI* rhi = mCore->GetRHI();

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
					rhi->SetVertexBuffers({ mMeshesRenderBuffers[lod][materialName][i]->VertexBuffer, mMeshesInstanceBuffers[lod][i]->InstanceBuffer });
				else
					rhi->SetVertexBuffers({ mMeshesRenderBuffers[lod][materialName][i]->VertexBuffer });
				rhi->SetIndexBuffer(mMeshesRenderBuffers[lod][materialName][i]->IndexBuffer);

				// run prepare callbacks for standard materials (specials are, i.e., shadow mapping, which are processed in their own systems)
				if (!isForwardPass && mMaterials[materialName]->IsStandard())
				{
					auto prepareMaterialBeforeRendering = MeshMaterialVariablesUpdateEvent->GetListener(materialName);
					if (prepareMaterialBeforeRendering)
						prepareMaterialBeforeRendering(i);
				}
				else if (isForwardPass && mCore->GetLevel()->mIllumination)
					mCore->GetLevel()->mIllumination->PrepareForForwardLighting(this, i);

				if (mIsInstanced)
				{
					if (mInstanceCountToRender[lod] > 0)
						rhi->DrawIndexedInstanced(mMeshesRenderBuffers[lod][materialName][i]->IndicesCount, mInstanceCountToRender[lod], 0, 0, 0);
					else 
						continue;
				}
				else
					rhi->DrawIndexed(mMeshesRenderBuffers[lod][materialName][i]->IndicesCount);
			}
		}
	}

	void ER_RenderingObject::DrawAABB(ER_RHI_GPUTexture* aRenderTarget, ER_RHI_GPURootSignature* rs)
	{
		if (mIsSelected && mAvailableInEditorMode && mEnableAABBDebug && ER_Utility::IsEditorMode)
			mDebugGizmoAABB->Draw(aRenderTarget, rs);
	}

	void ER_RenderingObject::SetTransformationMatrix(const XMMATRIX& mat)
	{
		mTransformationMatrix = mat;
		ER_MatrixHelper::GetFloatArray(mTransformationMatrix, mCurrentObjectTransformMatrix);
	}

	void ER_RenderingObject::SetTranslation(float x, float y, float z)
	{
		mTransformationMatrix *= XMMatrixTranslation(x, y, z);
		ER_MatrixHelper::GetFloatArray(mTransformationMatrix, mCurrentObjectTransformMatrix);
	}

	void ER_RenderingObject::SetScale(float x, float y, float z)
	{
		mTransformationMatrix *= XMMatrixScaling(x, y, z);
		ER_MatrixHelper::GetFloatArray(mTransformationMatrix, mCurrentObjectTransformMatrix);
	}

	void ER_RenderingObject::SetRotation(float x, float y, float z)
	{
		mTransformationMatrix *= XMMatrixRotationRollPitchYaw(x, y, z);
		ER_MatrixHelper::GetFloatArray(mTransformationMatrix, mCurrentObjectTransformMatrix);
	}

	// new instancing code
	void ER_RenderingObject::LoadInstanceBuffers(int lod)
	{
		auto rhi = mCore->GetRHI();
		assert(mModel != nullptr);
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
			mMeshesInstanceBuffers[lod][i]->InstanceBuffer = rhi->CreateGPUBuffer();
			CreateInstanceBuffer(&mInstanceData[lod][0], MAX_INSTANCE_COUNT, mMeshesInstanceBuffers[lod][i]->InstanceBuffer);
			mMeshesInstanceBuffers[lod][i]->Stride = sizeof(InstancedData);
		}
	}
	// new instancing code
	void ER_RenderingObject::CreateInstanceBuffer(InstancedData* instanceData, UINT instanceCount, ER_RHI_GPUBuffer* instanceBuffer)
	{
		if (instanceCount > MAX_INSTANCE_COUNT)
			throw ER_CoreException("Instances count limit is exceeded!");

		assert(instanceBuffer);
		instanceBuffer->CreateGPUBufferResource(mCore->GetRHI(), instanceData, MAX_INSTANCE_COUNT, InstanceSize(), true, ER_BIND_VERTEX_BUFFER);
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
			mCore->GetRHI()->UpdateBuffer(mMeshesInstanceBuffers[lod][i]->InstanceBuffer, mInstanceCountToRender[lod] == 0 ? nullptr : &instanceData[0], InstanceSize() * mInstanceCountToRender[lod]);
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
		ER_Terrain* terrain = mCore->GetLevel()->mTerrain;
		if (!terrain || !terrain->IsLoaded())
			return;

		if (!mIsInstanced)
		{
			XMFLOAT4 currentPos;
			ER_MatrixHelper::GetTranslation(XMLoadFloat4x4(&(XMFLOAT4X4(mCurrentObjectTransformMatrix))), currentPos);
			terrain->PlaceOnTerrain(&currentPos, 1, (TerrainSplatChannels)mTerrainProceduralPlacementSplatChannel);

			ER_MatrixHelper::SetTranslation(mTransformationMatrix, XMFLOAT3(currentPos.x, currentPos.y, currentPos.z));
			SetTransformationMatrix(mTransformationMatrix);
		}
		else
		{
			XMFLOAT4* instancesPositions = new XMFLOAT4[mInstanceCount];
			for (int instanceI = 0; instanceI < mInstanceCount; instanceI++)
			{
				instancesPositions[instanceI] = XMFLOAT4(
					mTerrainProceduralZoneCenterPos.x + ER_Utility::RandomFloat(-mTerrainProceduralZoneRadius, mTerrainProceduralZoneRadius),
					mTerrainProceduralZoneCenterPos.y,
					mTerrainProceduralZoneCenterPos.z + ER_Utility::RandomFloat(-mTerrainProceduralZoneRadius, mTerrainProceduralZoneRadius), 1.0f);
			}
			terrain->PlaceOnTerrain(instancesPositions, mInstanceCount, (TerrainSplatChannels)mTerrainProceduralPlacementSplatChannel);
			XMMATRIX worldMatrix = XMMatrixIdentity();

			for (int lod = 0; lod < GetLODCount(); lod++)
			{
				for (int instanceI = 0; instanceI < mInstanceCount; instanceI++)
				{
					float scale = ER_Utility::RandomFloat(mTerrainProceduralObjectMinScale, mTerrainProceduralObjectMaxScale);
					float roll = ER_Utility::RandomFloat(mTerrainProceduralObjectMinRoll, mTerrainProceduralObjectMaxRoll);
					float pitch = ER_Utility::RandomFloat(mTerrainProceduralObjectMinPitch, mTerrainProceduralObjectMaxPitch);
					float yaw = ER_Utility::RandomFloat(mTerrainProceduralObjectMinYaw, mTerrainProceduralObjectMaxYaw);

					worldMatrix = XMMatrixScaling(scale, scale, scale) * XMMatrixRotationRollPitchYaw(pitch, yaw, roll);
					ER_MatrixHelper::SetTranslation(worldMatrix, XMFLOAT3(instancesPositions[instanceI].x, instancesPositions[instanceI].y, instancesPositions[instanceI].z));
					XMStoreFloat4x4(&(mInstanceData[lod][instanceI].World), worldMatrix);
					worldMatrix = XMMatrixIdentity();
				}
				UpdateInstanceBuffer(mInstanceData[lod], lod);
			}
			DeleteObjects(instancesPositions);
		}
		mIsTerrainPlacementFinished = true;
		
	}
	void ER_RenderingObject::Update(const ER_CoreTime& time)
	{
		ER_Camera* camera = (ER_Camera*)(mCore->GetServices().FindService(ER_Camera::TypeIdClass()));
		assert(camera);

		bool editable = ER_Utility::IsEditorMode && mAvailableInEditorMode && mIsSelected;

		if (editable && mIsInstanced)
		{
			// load current selected instance's transform to temp transform (for UI)
			ER_MatrixHelper::GetFloatArray(mInstanceData[0][mEditorSelectedInstancedObjectIndex].World, mCurrentObjectTransformMatrix);
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

		if (ER_Utility::IsMainCameraCPUFrustumCulling && camera)
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

		ER_MatrixHelper::GetFloatArray(mCamera.ViewMatrix4X4(), mCameraViewMatrix);
		ER_MatrixHelper::GetFloatArray(mCamera.ProjectionMatrix4X4(), mCameraProjectionMatrix);

		ShowObjectsEditorWindow(mCameraViewMatrix, mCameraProjectionMatrix, mCurrentObjectTransformMatrix);

		XMFLOAT4X4 mat(mCurrentObjectTransformMatrix);
		mTransformationMatrix = XMLoadFloat4x4(&mat);

		//update instance world transform (from editor's gizmo/UI)
		if (mIsInstanced && ER_Utility::IsEditorMode)
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

		if (ER_Utility::IsEditorMode) {

			ER_Utility::IsLightEditor = false;
			ER_Utility::IsFoliageEditor = false;

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
				ER_MatrixHelper::GetTranslation(XMLoadFloat4x4(&(XMFLOAT4X4(mCurrentObjectTransformMatrix))), newCameraPos);

				ER_Camera* camera = (ER_Camera*)(mCore->GetServices().FindService(ER_Camera::TypeIdClass()));
				if (camera)
					camera->SetPosition(newCameraPos);
			}

			//terrain
			{
				ImGui::Combo("Terrain splat channel", &currentSplatChannnel, DisplayedSplatChannnelNames, 5);
				TerrainSplatChannels currentChannel = (TerrainSplatChannels)currentSplatChannnel;
				ER_Terrain* terrain = mCore->GetLevel()->mTerrain;

				if (ImGui::Button("Place on terrain") && terrain && terrain->IsLoaded())
				{
					XMFLOAT4 currentPos;
					ER_MatrixHelper::GetTranslation(XMLoadFloat4x4(&(XMFLOAT4X4(mCurrentObjectTransformMatrix))), currentPos);
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
			if (!ER_Utility::IsMainCameraCPUFrustumCulling && mInstanceData.size() == 0)
				return;
			if (ER_Utility::IsMainCameraCPUFrustumCulling && mTempPostCullingInstanceData.size() == 0)
				return;

			mTempPostLoddingInstanceData.clear();
			for (int lod = 0; lod < GetLODCount(); lod++)
				mTempPostLoddingInstanceData.push_back({});

			//traverse through original or culled instance data (sort of "read-only") to rebalance LOD's instance buffers
			int length = (ER_Utility::IsMainCameraCPUFrustumCulling) ? mTempPostCullingInstanceData.size() : mInstanceData[0].size();
			for (int i = 0; i < length; i++)
			{
				XMFLOAT3 pos;
				XMMATRIX mat = (ER_Utility::IsMainCameraCPUFrustumCulling) ? XMLoadFloat4x4(&mTempPostCullingInstanceData[i].World) : XMLoadFloat4x4(&mInstanceData[0][i].World);
				ER_MatrixHelper::GetTranslation(mat, pos);

				float distanceToCameraSqr =
					(mCamera.Position().x - pos.x) * (mCamera.Position().x - pos.x) +
					(mCamera.Position().y - pos.y) * (mCamera.Position().y - pos.y) +
					(mCamera.Position().z - pos.z) * (mCamera.Position().z - pos.z);

				XMMATRIX newMat;
				if (distanceToCameraSqr <= ER_Utility::DistancesLOD[0] * ER_Utility::DistancesLOD[0]) {
					mTempPostLoddingInstanceData[0].push_back((ER_Utility::IsMainCameraCPUFrustumCulling) ? mTempPostCullingInstanceData[i].World : mInstanceData[0][i].World);
				}
				else if (ER_Utility::DistancesLOD[0] * ER_Utility::DistancesLOD[0] < distanceToCameraSqr && distanceToCameraSqr <= ER_Utility::DistancesLOD[1] * ER_Utility::DistancesLOD[1]) {
					mTempPostLoddingInstanceData[1].push_back((ER_Utility::IsMainCameraCPUFrustumCulling) ? mTempPostCullingInstanceData[i].World : mInstanceData[0][i].World);
				}
				else if (ER_Utility::DistancesLOD[1] * ER_Utility::DistancesLOD[1] < distanceToCameraSqr && distanceToCameraSqr <= ER_Utility::DistancesLOD[2] * ER_Utility::DistancesLOD[2]) {
					mTempPostLoddingInstanceData[2].push_back((ER_Utility::IsMainCameraCPUFrustumCulling) ? mTempPostCullingInstanceData[i].World : mInstanceData[0][i].World);
				}
			}

			for (int i = 0; i < GetLODCount(); i++)
				UpdateInstanceBuffer(mTempPostLoddingInstanceData[i], i);
		}
		else
		{
			XMFLOAT3 pos;
			ER_MatrixHelper::GetTranslation(mTransformationMatrix, pos);

			float distanceToCameraSqr =
				(mCamera.Position().x - pos.x) * (mCamera.Position().x - pos.x) +
				(mCamera.Position().y - pos.y) * (mCamera.Position().y - pos.y) +
				(mCamera.Position().z - pos.z) * (mCamera.Position().z - pos.z);

			if (distanceToCameraSqr <= ER_Utility::DistancesLOD[0] * ER_Utility::DistancesLOD[0]) {
				mCurrentLODIndex = 0;
			}
			else if (ER_Utility::DistancesLOD[0] * ER_Utility::DistancesLOD[0] < distanceToCameraSqr && distanceToCameraSqr <= ER_Utility::DistancesLOD[1] * ER_Utility::DistancesLOD[1]) {
				mCurrentLODIndex = 1;
			}
			else if (ER_Utility::DistancesLOD[1] * ER_Utility::DistancesLOD[1] < distanceToCameraSqr && distanceToCameraSqr <= ER_Utility::DistancesLOD[2] * ER_Utility::DistancesLOD[2]) {
				mCurrentLODIndex = 2;
			}
			else
				mCurrentLODIndex = -1; //culled

			mCurrentLODIndex = std::min(mCurrentLODIndex, GetLODCount());
		}
	}
}

