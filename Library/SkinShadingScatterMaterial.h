#pragma once

#include "Common.h"
#include "Material.h"

using namespace Library;

namespace Rendering
{
	typedef struct _PostProcessingMaterialVertex
	{
		XMFLOAT4 Position;
		XMFLOAT2 TextureCoordinates;

		_PostProcessingMaterialVertex() { }

		_PostProcessingMaterialVertex(XMFLOAT4 position, XMFLOAT2 textureCoordinates)
			: Position(position), TextureCoordinates(textureCoordinates) { }
	} PostProcessingMaterialVertex;

	class SkinShadingScatterMaterial : public Material
	{
		RTTI_DECLARATIONS(SkinShadingScatterMaterial, Material)
		
		MATERIAL_VARIABLE_DECLARATION(ColorTexture)
		MATERIAL_VARIABLE_DECLARATION(DepthTexture)
		MATERIAL_VARIABLE_DECLARATION(id)
		MATERIAL_VARIABLE_DECLARATION(sssWidth)
		MATERIAL_VARIABLE_DECLARATION(sssStrength)
		MATERIAL_VARIABLE_DECLARATION(followSurface)
		MATERIAL_VARIABLE_DECLARATION(dir)

	public:
		SkinShadingScatterMaterial();

		virtual void Initialize(Effect* effect) override;
		virtual void CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const override;
		void CreateVertexBuffer(ID3D11Device* device, PostProcessingMaterialVertex* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const;
		virtual UINT VertexSize() const override;
	};
}