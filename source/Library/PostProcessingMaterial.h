#pragma once

#include "Common.h"
#include "Material.h"

namespace Library
{
	typedef struct _PostProcessingMaterialVertex
	{
		XMFLOAT4 Position;
		XMFLOAT2 TextureCoordinates;

		_PostProcessingMaterialVertex() { }

		_PostProcessingMaterialVertex(XMFLOAT4 position, XMFLOAT2 textureCoordinates)
			: Position(position), TextureCoordinates(textureCoordinates) { }
	} PostProcessingMaterialVertex;

	class PostProcessingMaterial : public Material
	{
		RTTI_DECLARATIONS(PostProcessingMaterial, Material)

		MATERIAL_VARIABLE_DECLARATION(ColorTexture)

	public:
		PostProcessingMaterial();

		//Effect ref
		virtual void Initialize(Effect* effect) override;
		virtual void CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const override;
		void CreateVertexBuffer(ID3D11Device* device, PostProcessingMaterialVertex* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const;
		virtual UINT VertexSize() const override;
	};
}