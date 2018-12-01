#pragma once
#include "Common.h"
#include "Material.h"
#include "VertexDeclarations.h"

using namespace Library;

namespace Rendering{


	class SkinShadingColorMaterial : public Material
	{
		RTTI_DECLARATIONS(SkinShadingColorMaterial, Material)

		MATERIAL_VARIABLE_DECLARATION(cameraPosition)
		MATERIAL_VARIABLE_DECLARATION(lightposition)
		MATERIAL_VARIABLE_DECLARATION(lightdirection)
		MATERIAL_VARIABLE_DECLARATION(lightfalloffStart)
		MATERIAL_VARIABLE_DECLARATION(lightfalloffWidth)
		MATERIAL_VARIABLE_DECLARATION(lightcolor)
		MATERIAL_VARIABLE_DECLARATION(lightattenuation)
		MATERIAL_VARIABLE_DECLARATION(farPlane)
		MATERIAL_VARIABLE_DECLARATION(bias)
		MATERIAL_VARIABLE_DECLARATION(lightviewProjection)
		MATERIAL_VARIABLE_DECLARATION(currWorldViewProj)
		MATERIAL_VARIABLE_DECLARATION(world)
		MATERIAL_VARIABLE_DECLARATION(id)
		MATERIAL_VARIABLE_DECLARATION(bumpiness)
		MATERIAL_VARIABLE_DECLARATION(specularIntensity)
		MATERIAL_VARIABLE_DECLARATION(specularRoughness)
		MATERIAL_VARIABLE_DECLARATION(specularFresnel)
		MATERIAL_VARIABLE_DECLARATION(translucency)
		MATERIAL_VARIABLE_DECLARATION(sssEnabled)
		MATERIAL_VARIABLE_DECLARATION(sssWidth)
		MATERIAL_VARIABLE_DECLARATION(ambient)
		MATERIAL_VARIABLE_DECLARATION(shadowMap)
		MATERIAL_VARIABLE_DECLARATION(diffuseTex)
		MATERIAL_VARIABLE_DECLARATION(normalTex)
		MATERIAL_VARIABLE_DECLARATION(specularAOTex)
		MATERIAL_VARIABLE_DECLARATION(beckmannTex)
		MATERIAL_VARIABLE_DECLARATION(irradianceTex)
		MATERIAL_VARIABLE_DECLARATION(ProjectiveTextureMatrix)


		


	public:
		SkinShadingColorMaterial();

		virtual void Initialize(Effect* effect) override;
		virtual void CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const override;
		void CreateVertexBuffer(ID3D11Device* device, NormalMappingMaterialVertex* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const;
		virtual UINT VertexSize() const override;
	};
}
