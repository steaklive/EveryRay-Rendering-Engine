#include "stdafx.h"

#include "ModelMaterial.h"
#include "ER_Model.h"
#include "GameException.h"
#include "Utility.h"
#include "assimp\scene.h"

namespace Library
{
	std::map<TextureType, UINT> ModelMaterial::sTextureTypeMappings;
	
	
	ModelMaterial::ModelMaterial(ER_Model& model)
		: mModel(model), mTextures()
	{
		InitializeTextureTypeMappings();
	}

	ModelMaterial::ModelMaterial(ER_Model& model, aiMaterial* material)
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
				mTextures.insert(std::pair<TextureType, std::vector<std::wstring>>(textureType, {}));
				for (UINT textureIndex = 0; textureIndex < textureCount; textureIndex++)
				{
					aiString path;
					if (material->GetTexture(mappedTextureType, textureIndex, &path) == AI_SUCCESS)
					{
						std::wstring wPath;
						Utility::ToWideString(path.C_Str(), wPath);
						mTextures[textureType].push_back(wPath);
					}
				}
			}
		}
	}

	ModelMaterial::~ModelMaterial()
	{
	}

	ER_Model& ModelMaterial::GetModel()
	{
		return mModel;
	}

	const std::string& ModelMaterial::Name() const
	{
		return mName;
	}

	const std::map<TextureType, std::vector<std::wstring>>& ModelMaterial::Textures() const
	{
		return mTextures;
	}

	const std::vector<std::wstring>& ModelMaterial::GetTexturesByType(TextureType type) const
	{
		auto it = mTextures.find(type);
		if (it != mTextures.end())
			return it->second;
		else
			return {};
	}

	bool ModelMaterial::HasTexturesOfType(TextureType type) const
	{
		auto it = mTextures.find(type);
		if (it != mTextures.end())
			return true;
		else
			return false;
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