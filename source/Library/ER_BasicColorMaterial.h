#pragma once
#include "ER_Material.h"

namespace Library
{
	class ER_RenderingObject;
	class ER_Mesh;

	namespace BasicMaterial_CBufferData {
		struct BasicMaterialCB
		{
			XMMATRIX World;
			XMMATRIX ViewProjection;
			XMFLOAT4 Color;
		};
	}
	class ER_BasicColorMaterial : public ER_Material
	{
	public:
		ER_BasicColorMaterial(ER_Core& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced = false);
		~ER_BasicColorMaterial();

		virtual void PrepareForRendering(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex) override;
		void PrepareForRendering(const XMMATRIX& worldTransform, const XMFLOAT4& color);

		virtual void CreateVertexBuffer(const ER_Mesh& mesh, ER_RHI_GPUBuffer* vertexBuffer) override;
		virtual int VertexSize() override;

		ER_RHI_GPUConstantBuffer<BasicMaterial_CBufferData::BasicMaterialCB> mConstantBuffer;
	};
}