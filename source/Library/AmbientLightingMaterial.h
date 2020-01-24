#pragma once

#include "Common.h"
#include "Material.h"

using namespace Library;

namespace Rendering
{
	typedef struct _AmbientLightingMaterialVertex
	{
		XMFLOAT4 Position;
		XMFLOAT2 TextureCoordinates;

		_AmbientLightingMaterialVertex() { }

		_AmbientLightingMaterialVertex(XMFLOAT4 position, XMFLOAT2 textureCoordinates)
			: Position(position), TextureCoordinates(textureCoordinates) { }
	} AmbientLightingMaterialVertex;

	class AmbientLightingMaterial : public Material
	{
		RTTI_DECLARATIONS(AmbientLightingMaterial, Material)

			MATERIAL_VARIABLE_DECLARATION(WorldViewProjection)
			MATERIAL_VARIABLE_DECLARATION(AmbientColor)
			MATERIAL_VARIABLE_DECLARATION(ColorTexture)

	public:
		AmbientLightingMaterial();

		virtual void Initialize(Effect* effect) override;
		virtual void CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const override;
		void CreateVertexBuffer(ID3D11Device* device, AmbientLightingMaterialVertex* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const;
		virtual UINT VertexSize() const override;
	};
}
