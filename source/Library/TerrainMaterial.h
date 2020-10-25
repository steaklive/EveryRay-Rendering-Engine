#pragma once
#include "Common.h"
#include "Material.h"
#include "VertexDeclarations.h"

namespace Library
{
	class TerrainMaterial : public Material
	{
		RTTI_DECLARATIONS(TerrainMaterial, Material)

		MATERIAL_VARIABLE_DECLARATION(World)
		MATERIAL_VARIABLE_DECLARATION(View)
		MATERIAL_VARIABLE_DECLARATION(Projection)
		MATERIAL_VARIABLE_DECLARATION(heightTexture)
		MATERIAL_VARIABLE_DECLARATION(groundTexture)
		MATERIAL_VARIABLE_DECLARATION(grassTexture)
		MATERIAL_VARIABLE_DECLARATION(rockTexture)
		MATERIAL_VARIABLE_DECLARATION(mudTexture)
		MATERIAL_VARIABLE_DECLARATION(splatTexture)
		//MATERIAL_VARIABLE_DECLARATION(normalTexture)
		MATERIAL_VARIABLE_DECLARATION(SunDirection)
		MATERIAL_VARIABLE_DECLARATION(SunColor)
		MATERIAL_VARIABLE_DECLARATION(AmbientColor)
		MATERIAL_VARIABLE_DECLARATION(ShadowTexelSize)
		MATERIAL_VARIABLE_DECLARATION(ShadowCascadeDistances)
		MATERIAL_VARIABLE_DECLARATION(CameraPosition)
		MATERIAL_VARIABLE_DECLARATION(TessellationFactor)
		MATERIAL_VARIABLE_DECLARATION(TerrainHeightScale)
		MATERIAL_VARIABLE_DECLARATION(UseDynamicTessellation)
		MATERIAL_VARIABLE_DECLARATION(TessellationFactorDynamic)
		MATERIAL_VARIABLE_DECLARATION(DistanceFactor)

	public:
		TerrainMaterial();

		virtual void Initialize(Effect* effect) override;
		virtual void CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const override;
		void CreateVertexBuffer(ID3D11Device* device, TerrainVertexInput* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const;
		virtual UINT VertexSize() const override;
	};
}