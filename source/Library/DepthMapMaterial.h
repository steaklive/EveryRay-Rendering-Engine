#pragma once

#include "Common.h"
#include "Material.h"
#include "VertexDeclarations.h"

namespace Library
{
	class DepthMapMaterial : public Material
	{
		RTTI_DECLARATIONS(DepthMapMaterial, Material)

		MATERIAL_VARIABLE_DECLARATION(WorldLightViewProjection)
		MATERIAL_VARIABLE_DECLARATION(LightViewProjection)
		MATERIAL_VARIABLE_DECLARATION(AlbedoAlphaMap)

	public:
		DepthMapMaterial();

		virtual void Initialize(Effect* effect) override;
		virtual void CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const override;
		void CreateVertexBuffer(ID3D11Device* device, VertexPositionTexture* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const;
		virtual UINT VertexSize() const override;
	};
}