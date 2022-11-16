#pragma once
#include "ER_Material.h"

#define VOXELIZATION_MAT_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define VOXELIZATION_MAT_ROOT_DESCRIPTOR_TABLE_UAV_INDEX 1
#define VOXELIZATION_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 2

namespace EveryRay_Core
{
	class ER_RenderingObject;
	class ER_Mesh;
	class ER_Camera;

	namespace VoxelizationMaterial_CBufferData {
		struct ER_ALIGN_GPU_BUFFER VoxelizationCB
		{
			XMMATRIX World;
			XMMATRIX ViewProjection;
			XMMATRIX ShadowMatrix;
			XMFLOAT4 ShadowTexelSize;
			XMFLOAT4 ShadowCascadeDistances;
			XMFLOAT4 VoxelCameraPos;
			float VoxelTextureDimension;
			float WorldVoxelScale;
		};
	}
	class ER_VoxelizationMaterial : public ER_Material
	{
	public:
		ER_VoxelizationMaterial(ER_Core& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced = false);
		~ER_VoxelizationMaterial();

		void PrepareForRendering(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, 
			float voxelScale, float voxelTexSize, const XMFLOAT4& voxelCameraPos, ER_RHI_GPURootSignature* rs);
		virtual void PrepareResourcesForStandardMaterial(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, ER_RHI_GPURootSignature* rs) override;
		virtual void CreateVertexBuffer(const ER_Mesh& mesh, ER_RHI_GPUBuffer* vertexBuffer) override;
		virtual int VertexSize() override;

		ER_RHI_GPUConstantBuffer<VoxelizationMaterial_CBufferData::VoxelizationCB> mConstantBuffer;
	};
}