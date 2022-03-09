#pragma once
#include "Common.h"
#include "Material.h"
#include "VertexDeclarations.h"

namespace Library
{
	class DeferredMaterial : public Material
	{
		RTTI_DECLARATIONS(DeferredMaterial, Material)

		MATERIAL_VARIABLE_DECLARATION(ViewProjection)
		MATERIAL_VARIABLE_DECLARATION(World)
		MATERIAL_VARIABLE_DECLARATION(AlbedoMap)
		MATERIAL_VARIABLE_DECLARATION(NormalMap)
		MATERIAL_VARIABLE_DECLARATION(RoughnessMap)
		MATERIAL_VARIABLE_DECLARATION(MetallicMap)
		MATERIAL_VARIABLE_DECLARATION(HeightMap)
		MATERIAL_VARIABLE_DECLARATION(ReflectionMaskFactor)
		MATERIAL_VARIABLE_DECLARATION(FoliageMaskFactor)
		MATERIAL_VARIABLE_DECLARATION(UseGlobalDiffuseProbeMaskFactor)
		MATERIAL_VARIABLE_DECLARATION(UsePOM)

	public:
		DeferredMaterial();

		virtual void Initialize(Effect* effect) override;
		virtual void CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const override;
		void CreateVertexBuffer(ID3D11Device* device, VertexPositionTextureNormalTangent* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const;
		virtual UINT VertexSize() const override;
	};
}