#pragma once

#include "Common.h"
#include "Material.h"
#include "VertexDeclarations.h"
using namespace Library;

namespace Rendering
{
	class ParallaxMappingTestMaterial : public Material
	{
		RTTI_DECLARATIONS(ParallaxMappingTestMaterial, Material)

			MATERIAL_VARIABLE_DECLARATION(ViewProjection)
			MATERIAL_VARIABLE_DECLARATION(World)
			MATERIAL_VARIABLE_DECLARATION(CameraPosition)
			MATERIAL_VARIABLE_DECLARATION(Albedo)
			MATERIAL_VARIABLE_DECLARATION(Metalness)
			MATERIAL_VARIABLE_DECLARATION(Roughness)
			MATERIAL_VARIABLE_DECLARATION(LightPosition)
			MATERIAL_VARIABLE_DECLARATION(LightColor)
			MATERIAL_VARIABLE_DECLARATION(UseAmbientOcclusion)
			MATERIAL_VARIABLE_DECLARATION(POMHeightScale)
			MATERIAL_VARIABLE_DECLARATION(POMSoftShadows)
			MATERIAL_VARIABLE_DECLARATION(DemonstrateAO)
			MATERIAL_VARIABLE_DECLARATION(DemonstrateNormal)
			MATERIAL_VARIABLE_DECLARATION(HeightMap)
			MATERIAL_VARIABLE_DECLARATION(EnvMap)

	public:

		ParallaxMappingTestMaterial();

		virtual void Initialize(Effect* effect) override;
		virtual void CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const override;
		void CreateVertexBuffer(ID3D11Device* device, VertexPositionTextureNormalTangent* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const;
		virtual UINT VertexSize() const override;
	};
}