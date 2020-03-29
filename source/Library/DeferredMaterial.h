#pragma once
#include "Common.h"
#include "Material.h"
#include "VertexDeclarations.h"

namespace Library
{
	class DeferredMaterial : public Material
	{
		RTTI_DECLARATIONS(DeferredMaterial, Material)

		MATERIAL_VARIABLE_DECLARATION(WorldViewProjection)
		MATERIAL_VARIABLE_DECLARATION(World)
		MATERIAL_VARIABLE_DECLARATION(AlbedoMap)

	public:
		DeferredMaterial();

		virtual void Initialize(Effect* effect) override;
		virtual void CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const override;
		void CreateVertexBuffer(ID3D11Device* device, VertexPositionTextureNormal* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const;
		virtual UINT VertexSize() const override;
	};
}