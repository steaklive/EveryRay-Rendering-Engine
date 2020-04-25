#pragma once

#include "Common.h"
#include "Material.h"
#include "VertexDeclarations.h"

using namespace Library;

namespace Rendering
{
	class ScreenSpaceReflectionsMaterial : public Material
	{
		RTTI_DECLARATIONS(ScreenSpaceReflectionsMaterial, Material)

			MATERIAL_VARIABLE_DECLARATION(ColorTexture)
			MATERIAL_VARIABLE_DECLARATION(GBufferDepth)
			MATERIAL_VARIABLE_DECLARATION(GBufferNormals)
			MATERIAL_VARIABLE_DECLARATION(GBufferExtra)
			MATERIAL_VARIABLE_DECLARATION(ViewProjection)
			MATERIAL_VARIABLE_DECLARATION(InvProjMatrix)
			MATERIAL_VARIABLE_DECLARATION(InvViewMatrix)
			MATERIAL_VARIABLE_DECLARATION(ViewMatrix)
			MATERIAL_VARIABLE_DECLARATION(ProjMatrix)
			MATERIAL_VARIABLE_DECLARATION(CameraPosition)
			MATERIAL_VARIABLE_DECLARATION(StepSize)
			MATERIAL_VARIABLE_DECLARATION(MaxThickness)
			MATERIAL_VARIABLE_DECLARATION(Time)
			MATERIAL_VARIABLE_DECLARATION(MaxRayCount)

	public:
		ScreenSpaceReflectionsMaterial();

		virtual void Initialize(Effect* effect) override;
		virtual void CreateVertexBuffer(ID3D11Device* device, const Mesh& mesh, ID3D11Buffer** vertexBuffer) const override;
		void CreateVertexBuffer(ID3D11Device* device, VertexPositionTexture* vertices, UINT vertexCount, ID3D11Buffer** vertexBuffer) const;
		virtual UINT VertexSize() const override;
	};
}
