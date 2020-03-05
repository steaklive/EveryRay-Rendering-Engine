#pragma once

#include "Common.h"
#include "Material.h"
#include "VertexDeclarations.h"
using namespace Library;

namespace Rendering
{
	class StandardLightingMaterial : public Material
	{
		RTTI_DECLARATIONS(StandardLightingMaterial, Material)

		MATERIAL_VARIABLE_DECLARATION(WorldViewProjection)
		MATERIAL_VARIABLE_DECLARATION(World)
		MATERIAL_VARIABLE_DECLARATION(ModelToShadow)
		MATERIAL_VARIABLE_DECLARATION(CameraPosition)

		MATERIAL_VARIABLE_DECLARATION(SunDirection)
		MATERIAL_VARIABLE_DECLARATION(SunColor)
		MATERIAL_VARIABLE_DECLARATION(AmbientColor)
		MATERIAL_VARIABLE_DECLARATION(ShadowTexelSize)

		MATERIAL_VARIABLE_DECLARATION(AlbedoTexture)
		MATERIAL_VARIABLE_DECLARATION(NormalTexture)
		MATERIAL_VARIABLE_DECLARATION(SpecularTexture)
		MATERIAL_VARIABLE_DECLARATION(MetallicTexture)
		MATERIAL_VARIABLE_DECLARATION(RoughnessTexture)
		MATERIAL_VARIABLE_DECLARATION(ShadowTexture)
		MATERIAL_VARIABLE_DECLARATION(IrradianceTexture)
		MATERIAL_VARIABLE_DECLARATION(RadianceTexture)
		MATERIAL_VARIABLE_DECLARATION(IntegrationTexture)

	public:
		StandardLightingMaterial();

		virtual void Initialize(Effect* effect) override;
		virtual void CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const override;
		void CreateVertexBuffer(ID3D11Device* device, VertexPositionTextureNormalTangent* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const;
		virtual UINT VertexSize() const override;
	};
}