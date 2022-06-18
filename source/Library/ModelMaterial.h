#pragma once

#include "Common.h"

struct aiMaterial;

namespace Library
{
	enum TextureType
	{
		TextureTypeDifffuse = 0,
		TextureTypeSpecularMap,
		TextureTypeAmbient,
		TextureTypeEmissive,
		TextureTypeHeightmap,
		TextureTypeNormalMap,
		TextureTypeSpecularPowerMap,
		TextureTypeDisplacementMap,
		TextureTypeLightMap,
		TextureTypeEnd
	};

	class Model;

	class ModelMaterial
	{
	public:
		ModelMaterial(Model& model, aiMaterial* material);
		ModelMaterial(Model& model);
		~ModelMaterial();

		Model& GetModel();
		const std::string& Name() const;
		const std::map<TextureType, std::vector<std::wstring>>& Textures() const;
		const std::vector<std::wstring>& GetTexturesByType(TextureType type) const;
		bool HasTexturesOfType(TextureType type) const;

	private:
		static void InitializeTextureTypeMappings();
		static std::map<TextureType, UINT> sTextureTypeMappings;

		Model& mModel;
		std::string mName;
		std::map<TextureType, std::vector<std::wstring>> mTextures;
	};
}