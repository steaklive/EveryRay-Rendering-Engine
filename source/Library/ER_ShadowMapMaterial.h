#pragma once
#include "ER_Material.h"

namespace Library
{
	class ER_RenderingObject;
	class ER_Mesh;

	namespace ShadowMapMaterial_CBufferData {
		struct ShadowMapCB
		{
			XMMATRIX WorldLightViewProjection;
			XMMATRIX LightViewProjection;
		};
	}
	class ER_ShadowMapMaterial : public ER_Material
	{
	public:
		ER_ShadowMapMaterial(ER_Core& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced = false);
		~ER_ShadowMapMaterial();

		void PrepareForRendering(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, int cascadeIndex);
		virtual void CreateVertexBuffer(const ER_Mesh& mesh, ER_RHI_GPUBuffer* vertexBuffer) override;
		virtual int VertexSize() override;

		ConstantBuffer<ShadowMapMaterial_CBufferData::ShadowMapCB> mConstantBuffer;
	};
}