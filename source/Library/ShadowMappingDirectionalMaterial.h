#pragma once

#include "Common.h"
#include "Material.h"
#include "VertexDeclarations.h"

namespace Library
{
	class ShadowMappingDirectionalMaterial : public Material
	{
		RTTI_DECLARATIONS(ShadowMappingDirectionalMaterial, Material)

			MATERIAL_VARIABLE_DECLARATION(WorldViewProjection)
			MATERIAL_VARIABLE_DECLARATION(World)
			MATERIAL_VARIABLE_DECLARATION(AmbientColor)
			MATERIAL_VARIABLE_DECLARATION(LightColor)
			MATERIAL_VARIABLE_DECLARATION(LightDirection)
			MATERIAL_VARIABLE_DECLARATION(CameraPosition)

			MATERIAL_VARIABLE_DECLARATION(ColorTexture)
			MATERIAL_VARIABLE_DECLARATION(Distances)

			MATERIAL_VARIABLE_DECLARATION(ProjectiveTextureMatrix0)
			MATERIAL_VARIABLE_DECLARATION(ProjectiveTextureMatrix1)
			MATERIAL_VARIABLE_DECLARATION(ProjectiveTextureMatrix2)

			MATERIAL_VARIABLE_DECLARATION(ShadowMap0)
			MATERIAL_VARIABLE_DECLARATION(ShadowMap1)
			MATERIAL_VARIABLE_DECLARATION(ShadowMap2)
			MATERIAL_VARIABLE_DECLARATION(ShadowMapSize)
			MATERIAL_VARIABLE_DECLARATION(visualizeCascades)



	public:
		ShadowMappingDirectionalMaterial();

		virtual void Initialize(Effect* effect) override;
		virtual void CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const override;
		void CreateVertexBuffer(ID3D11Device* device, VertexPositionTextureNormal* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const;
		virtual UINT VertexSize() const override;
	};
}
