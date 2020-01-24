#pragma once
#include "Common.h"
#include "Material.h"

namespace Library
{
	typedef struct _BasicMaterialVertex
	{
		XMFLOAT4 Position;
		XMFLOAT4 Color;

		_BasicMaterialVertex() { }

		_BasicMaterialVertex(XMFLOAT4 position, XMFLOAT4 color)
			: Position(position), Color(color) { }
	} BasicMaterialVertex;

	class BasicMaterial : public Material
	{
		RTTI_DECLARATIONS(BasicMaterial, Material)

		MATERIAL_VARIABLE_DECLARATION(WorldViewProjection)

	public:
		BasicMaterial();

		virtual void Initialize(Effect* effect) override;
		virtual void CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const override;
		void CreateVertexBuffer(ID3D11Device* device, BasicMaterialVertex* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const;
		virtual UINT VertexSize() const override;
	};
}