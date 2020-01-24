#include "stdafx.h"

#include "ModelMaterial.h"
#include "GameException.h"
#include "Utility.h"
#include "..\assimp\scene.h"

namespace Library
{
	std::map<TextureType, UINT> ModelMaterial::sTextureTypeMappings;
	
	
	ModelMaterial::ModelMaterial(Model& model)
		: mModel(model), mTextures()
	{
		InitializeTextureTypeMappings();
	}

	ModelMaterial::ModelMaterial(Model& model, aiMaterial* material)
		: mModel(model), mTextures()
	{
		InitializeTextureTypeMappings();

		aiString name;
		material->Get(AI_MATKEY_NAME, name);
		mName = name.C_Str();

		for (TextureType textureType = (TextureType)0; textureType < TextureTypeEnd; textureType = (TextureType)(textureType + 1))
		{
			aiTextureType mappedTextureType = (aiTextureType)sTextureTypeMappings[textureType];

			UINT textureCount = material->GetTextureCount(mappedTextureType);
			if (textureCount > 0)
			{
				std::vector<std::wstring>* textures = new std::vector<std::wstring>();
				mTextures.insert(std::pair<TextureType, std::vector<std::wstring>*>(textureType, textures));

				textures->reserve(textureCount);
				for (UINT textureIndex = 0; textureIndex < textureCount; textureIndex++)
				{
					aiString path;
					if (material->GetTexture(mappedTextureType, textureIndex, &path) == AI_SUCCESS)
					{
						std::wstring wPath;
						Utility::ToWideString(path.C_Str(), wPath);

						textures->push_back(wPath);
					}
				}
			}
		}
	}

	ModelMaterial::~ModelMaterial()
	{
		for (std::pair<TextureType, std::vector<std::wstring>*> textures : mTextures)
		{
			DeleteObject(textures.second);
		}
	}

	Model& ModelMaterial::GetModel()
	{
		return mModel;
	}

	const std::string& ModelMaterial::Name() const
	{
		return mName;
	}

	const std::map<TextureType, std::vector<std::wstring>*> ModelMaterial::Textures() const
	{
		return mTextures;
	}

	std::vector<std::wstring>* ModelMaterial::GetTexturesByType(TextureType type)
	{
		auto it = mTextures.find(type);
		if (it != mTextures.end())
			return mTextures[type];
		else
			return nullptr;
	}

	void ModelMaterial::InitializeTextureTypeMappings()
	{
		if (sTextureTypeMappings.size() != TextureTypeEnd)
		{
			sTextureTypeMappings[TextureTypeDifffuse] = aiTextureType_DIFFUSE;
			sTextureTypeMappings[TextureTypeSpecularMap] = aiTextureType_SPECULAR;
			sTextureTypeMappings[TextureTypeAmbient] = aiTextureType_AMBIENT;
			sTextureTypeMappings[TextureTypeHeightmap] = aiTextureType_HEIGHT;
			sTextureTypeMappings[TextureTypeNormalMap] = aiTextureType_NORMALS;
			sTextureTypeMappings[TextureTypeSpecularPowerMap] = aiTextureType_SHININESS;
			sTextureTypeMappings[TextureTypeDisplacementMap] = aiTextureType_DISPLACEMENT;
			sTextureTypeMappings[TextureTypeLightMap] = aiTextureType_LIGHTMAP;
		}
	}
}