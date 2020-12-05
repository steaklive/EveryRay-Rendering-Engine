#pragma once
#include "Common.h"
#include "Material.h"

namespace Library
{
	class SkyboxMaterial : public Material
	{
		RTTI_DECLARATIONS(SkyboxMaterial, Material)

		MATERIAL_VARIABLE_DECLARATION(WorldViewProjection)
		MATERIAL_VARIABLE_DECLARATION(SkyboxTexture)
		MATERIAL_VARIABLE_DECLARATION(UseCustomColor)
		MATERIAL_VARIABLE_DECLARATION(BottomColor)
		MATERIAL_VARIABLE_DECLARATION(TopColor)

	public:
		SkyboxMaterial();

		virtual void Initialize(Effect* effect) override;
		virtual void CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const override;
		void CreateVertexBuffer(ID3D11Device* device, XMFLOAT4* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const;
		virtual UINT VertexSize() const override;
	};
}