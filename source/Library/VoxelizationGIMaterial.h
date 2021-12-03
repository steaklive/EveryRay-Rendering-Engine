#pragma once

#include "Common.h"
#include "Material.h"
#include "VertexDeclarations.h"
using namespace Library;

namespace Rendering
{
	class VoxelizationGIMaterial : public Material
	{
		RTTI_DECLARATIONS(VoxelizationGIMaterial, Material)

			MATERIAL_VARIABLE_DECLARATION(ViewProjection)
			MATERIAL_VARIABLE_DECLARATION(ShadowMatrices)
			MATERIAL_VARIABLE_DECLARATION(ShadowTexelSize)
			MATERIAL_VARIABLE_DECLARATION(ShadowCascadeDistances)
			MATERIAL_VARIABLE_DECLARATION(VoxelCameraPos)
			MATERIAL_VARIABLE_DECLARATION(WorldVoxelScale)
			MATERIAL_VARIABLE_DECLARATION(MeshWorld)
			MATERIAL_VARIABLE_DECLARATION(MeshAlbedo)
			MATERIAL_VARIABLE_DECLARATION(CascadedShadowTextures)
	public:

		VoxelizationGIMaterial();

		virtual void Initialize(Effect* effect) override;
		virtual void CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const override;
		void CreateVertexBuffer(ID3D11Device* device, VertexPositionTextureNormalTangent* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const;
		virtual UINT VertexSize() const override;
	};
}