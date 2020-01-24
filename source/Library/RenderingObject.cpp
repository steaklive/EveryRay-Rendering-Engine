#include "stdafx.h"

#include "RenderingObject.h"
#include "GameException.h"
#include "Model.h"
#include "Mesh.h"
#include "Game.h"
#include "MatrixHelper.h"
#include "Camera.h"
#include "Utility.h"

#include "TGATextureLoader.h"
#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>

namespace Rendering
{
	RenderingObject::RenderingObject(std::string pName, Game& pGame, std::unique_ptr<Model> pModel) 
		: 
		GameComponent(pGame),
		mModel(std::move(pModel)),
		mMeshesRenderBuffers(0, nullptr),
		mMeshesInstanceBuffers(0, nullptr),
		mMeshVertices(0),
		mName(pName),
		mInstanceCount(0)
	{
		if (!mModel)
			throw GameException("Failed to create a RenderingObject from a model");

		mMeshesCount = mModel->Meshes().size();
		for (size_t i = 0; i < mMeshesCount; i++)
		{
			mMeshVertices.push_back(mModel->Meshes().at(i)->Vertices());
			mMeshesRenderBuffers.push_back(new RenderBufferData());
			mMeshesTextureBuffers.push_back(TextureData());
		}

		LoadAssignedMeshTextures();
		mAABB = mModel->GenerateAABB();
		
	}

	RenderingObject::~RenderingObject()
	{
		DeleteObject(MeshMaterialVariablesUpdateEvent);
		DeletePointerCollection(mMeshesMaterials);
		DeletePointerCollection(mMeshesRenderBuffers);
		DeletePointerCollection(mMeshesInstanceBuffers);
		mMeshesTextureBuffers.clear();
	}

	void RenderingObject::LoadMaterial(Material* pMaterial, Effect* pEffect)
	{
		assert(mModel != nullptr);

		for (size_t i = 0; i < mMeshesCount; i++)
		{
			mMeshesMaterials.push_back(pMaterial);
			mMeshesMaterials.back()->Initialize(pEffect);
		}
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
			}

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
			}

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
			}
		}
	}
	void RenderingObject::LoadCustomMeshTextures(int meshIndex, std::wstring albedoPath, std::wstring normalPath, std::wstring roughnessPath, std::wstring metallicPath, std::wstring extra1Path, std::wstring extra2Path, std::wstring extra3Path)
	{
		assert(meshIndex < mMeshesCount);
		
		std::string errorMessage = mModel->GetFileName() + " of mesh index: " + std::to_string(meshIndex);

		if (!albedoPath.empty())
			LoadTexture(TextureType::TextureTypeDifffuse, albedoPath, meshIndex);

		if (!normalPath.empty())
			LoadTexture(TextureType::TextureTypeNormalMap, normalPath, meshIndex);

		if (!roughnessPath.empty())
			LoadTexture(TextureType::TextureTypeSpecularMap, roughnessPath, meshIndex);

		if (!metallicPath.empty())
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
			texType = "Roughness Texture";
			resource = &(mMeshesTextureBuffers[meshIndex].RoughnessMap);
			break;
		case TextureType::TextureTypeEmissive:
			texType = "Metallic Texture";
			resource = &(mMeshesTextureBuffers[meshIndex].MetallicMap);
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

		for (size_t i = 0; i < mMeshesCount; i++)
		{
			mMeshesMaterials[i]->CreateVertexBuffer(mGame->Direct3DDevice(), *mModel->Meshes()[i], &(mMeshesRenderBuffers[i]->VertexBuffer));
			mModel->Meshes()[i]->CreateIndexBuffer(&(mMeshesRenderBuffers[i]->IndexBuffer));
			mMeshesRenderBuffers[i]->IndicesCount = mModel->Meshes()[i]->Indices().size();
			mMeshesRenderBuffers[i]->Stride = mMeshesMaterials[i]->VertexSize();
			mMeshesRenderBuffers[i]->Offset = 0;
		}
	}
	void RenderingObject::LoadInstanceBuffers(std::vector<InstancingMaterial::InstancedData>& pInstanceData)
	{
		assert(mModel != nullptr);
		assert(mGame->Direct3DDevice() != nullptr);

		mInstanceCount = pInstanceData.size();
		for (size_t i = 0; i < mMeshesCount; i++)
		{
			mMeshesInstanceBuffers.push_back(new InstanceBufferData());
			static_cast<InstancingMaterial*>(mMeshesMaterials[i])->CreateInstanceBuffer(mGame->Direct3DDevice(), pInstanceData, &(mMeshesInstanceBuffers[i]->InstanceBuffer));
			mMeshesInstanceBuffers[i]->Stride = sizeof(InstancingMaterial::InstancedData);
		}
	}
	
	void RenderingObject::Draw()
	{
		if (mMeshesMaterials.size() == 0 || mMeshesRenderBuffers.size() == 0)
			return;

		for (size_t i = 0; i < mMeshesCount; i++)
		{
			ID3D11DeviceContext* context = mGame->Direct3DDeviceContext();
			Pass* pass = mMeshesMaterials.at(i)->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mMeshesMaterials.at(i)->InputLayouts().at(pass);
			context->IASetInputLayout(inputLayout);

			UINT stride = mMeshesRenderBuffers[i]->Stride;
			UINT offset = mMeshesRenderBuffers[i]->Offset;
			context->IASetVertexBuffers(0, 1, &(mMeshesRenderBuffers[i]->VertexBuffer), &stride, &offset);
			context->IASetIndexBuffer(mMeshesRenderBuffers[i]->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

			for (auto listener : MeshMaterialVariablesUpdateEvent->GetListeners())
				listener(i);

			pass->Apply(0, context);
			context->DrawIndexed(mMeshesRenderBuffers[i]->IndicesCount, 0, 0);
		}
	}
	void RenderingObject::DrawInstanced()
	{
		for (int i = 0; i < mMeshesCount; i++)
		{
			ID3D11DeviceContext* context = mGame->Direct3DDeviceContext();
			Pass* pass = mMeshesMaterials.at(i)->CurrentTechnique()->Passes().at(0);
			ID3D11InputLayout* inputLayout = mMeshesMaterials.at(i)->InputLayouts().at(pass);
			context->IASetInputLayout(inputLayout);

			ID3D11Buffer* vertexBuffers[2] = { mMeshesRenderBuffers[i]->VertexBuffer, mMeshesInstanceBuffers[i]->InstanceBuffer };
			UINT strides[2] = { mMeshesRenderBuffers[i]->Stride, mMeshesInstanceBuffers[i]->Stride };
			UINT offsets[2] = { mMeshesRenderBuffers[i]->Offset, mMeshesInstanceBuffers[i]->Offset };

			context->IASetVertexBuffers(0, 2, vertexBuffers, strides, offsets);
			context->IASetIndexBuffer(mMeshesRenderBuffers[i]->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

			for (auto listener : MeshMaterialVariablesUpdateEvent->GetListeners())
				listener(i);

			pass->Apply(0, context);

			context->DrawIndexedInstanced(mMeshesRenderBuffers[i]->IndicesCount, mInstanceCount, 0, 0, 0);
		}
	}

}

