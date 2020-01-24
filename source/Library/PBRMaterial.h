#pragma once
#include "Common.h"
#include "Material.h"
#include "VertexDeclarations.h"

namespace Library
{
	//typedef struct _NormalMappingMaterialVertex
	//{
	//	XMFLOAT4 Position;
	//	XMFLOAT2 TextureCoordinates;
	//	XMFLOAT3 Normal;
	//	XMFLOAT3 Tangent;
	//
	//	_NormalMappingMaterialVertex()
	//	{
	//	}
	//
	//	_NormalMappingMaterialVertex(XMFLOAT4 position, XMFLOAT2 textureCoordinates, XMFLOAT3 normal, XMFLOAT3 tangent)
	//		: Position(position), TextureCoordinates(textureCoordinates), Normal(normal), Tangent(tangent)
	//	{
	//	}
	//
	//} NormalMappingMaterialVertex;

	class PBRMaterial : public Material
	{
		RTTI_DECLARATIONS(PBRMaterial, Material)

			MATERIAL_VARIABLE_DECLARATION(WorldViewProjection)
			MATERIAL_VARIABLE_DECLARATION(World)

			MATERIAL_VARIABLE_DECLARATION(LightPosition)
			MATERIAL_VARIABLE_DECLARATION(LightRadius)
			MATERIAL_VARIABLE_DECLARATION(CameraPosition)
			MATERIAL_VARIABLE_DECLARATION(Roughness)
			MATERIAL_VARIABLE_DECLARATION(Metalness)

			MATERIAL_VARIABLE_DECLARATION(irradianceTexture)
			MATERIAL_VARIABLE_DECLARATION(radianceTexture)
			MATERIAL_VARIABLE_DECLARATION(integrationTexture)

			MATERIAL_VARIABLE_DECLARATION(albedoTexture)
			MATERIAL_VARIABLE_DECLARATION(metallicTexture)
			MATERIAL_VARIABLE_DECLARATION(roughnessTexture)
			MATERIAL_VARIABLE_DECLARATION(normalTexture)


	public:
		PBRMaterial();

		virtual void Initialize(Effect* effect) override;
		virtual void CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const override;
		void CreateVertexBuffer(ID3D11Device* device, VertexPositionTextureNormalTangent* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const;
		virtual UINT VertexSize() const override;
	};
}
