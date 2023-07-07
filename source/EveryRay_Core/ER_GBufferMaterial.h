#pragma once
#include "ER_Material.h"

#define GBUFFER_MAT_ROOT_DESCRIPTOR_TABLE_PIXEL_SRV_INDEX 0
#define GBUFFER_MAT_ROOT_DESCRIPTOR_TABLE_VERTEX_SRV_INDEX 1
#define GBUFFER_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 2
#define GBUFFER_MAT_ROOT_CONSTANT_INDEX 3

namespace EveryRay_Core
{
	class ER_Mesh;
	class ER_RenderingObject;

	namespace GBufferMaterial_CBufferData {
		struct ER_ALIGN_GPU_BUFFER GBufferCB
		{
			XMMATRIX ViewProjection;
		};
	}
	class ER_GBufferMaterial : public ER_Material
	{
	public:
		ER_GBufferMaterial(ER_Core& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced = false);
		~ER_GBufferMaterial();

		void PrepareForRendering(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, ER_RHI_GPURootSignature* rs);
		virtual void PrepareResourcesForStandardMaterial(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, ER_RHI_GPURootSignature* rs) override;
		virtual void SetRootConstantForMaterial(UINT a32BitConstant) override; // We use root constant for LOD index in this material
		virtual void CreateVertexBuffer(const ER_Mesh& mesh, ER_RHI_GPUBuffer* vertexBuffer) override;
		virtual int VertexSize() override;

		ER_RHI_GPUConstantBuffer<GBufferMaterial_CBufferData::GBufferCB> mConstantBuffer;
	};
}