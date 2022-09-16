#pragma once
#include "ER_Material.h"

#define DEBUGLIGHTPROBE_MAT_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define DEBUGLIGHTPROBE_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 1

namespace EveryRay_Core
{
	class ER_RenderingObject;
	class ER_Mesh;

	namespace DebugLightProbeMaterial_CBufferData {
		struct DebugLightProbeCB
		{
			XMMATRIX ViewProjection;
			XMMATRIX World;
			XMFLOAT4 CameraPosition;
			XMFLOAT2 DiscardCulled_IsDiffuse;
		};
	}
	class ER_DebugLightProbeMaterial : public ER_Material
	{
	public:
		ER_DebugLightProbeMaterial(ER_Core& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced = false);
		~ER_DebugLightProbeMaterial();

		void PrepareForRendering(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, int aProbeType, ER_RHI_GPURootSignature* rs);
		virtual void PrepareResourcesForStandardMaterial(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, ER_RHI_GPURootSignature* rs) override;
		virtual void CreateVertexBuffer(const ER_Mesh& mesh, ER_RHI_GPUBuffer* vertexBuffer) override;
		virtual int VertexSize() override;

		ER_RHI_GPUConstantBuffer<DebugLightProbeMaterial_CBufferData::DebugLightProbeCB> mConstantBuffer;
	};
}