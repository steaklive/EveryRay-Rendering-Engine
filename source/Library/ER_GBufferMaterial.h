#pragma once
#include "ER_Material.h"

namespace Library
{
	class ER_Mesh;
	class ER_RenderingObject;

	namespace GBufferMaterial_CBufferData {
		struct GBufferCB
		{
			XMMATRIX ViewProjection;
			XMMATRIX World;
			XMFLOAT4 Reflection_Foliage_UseGlobalDiffuseProbe_POM_MaskFactor;
			XMFLOAT4 SkipDeferredLighting;
		};
	}
	class ER_GBufferMaterial : public ER_Material
	{
	public:
		ER_GBufferMaterial(Game& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced = false);
		~ER_GBufferMaterial();

		virtual void PrepareForRendering(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex) override;
		virtual void CreateVertexBuffer(const ER_Mesh& mesh, ID3D11Buffer** vertexBuffer) override;
		virtual int VertexSize() override;

		ConstantBuffer<GBufferMaterial_CBufferData::GBufferCB> mConstantBuffer;
	};
}