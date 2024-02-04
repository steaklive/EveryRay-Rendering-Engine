#pragma once
#include "ER_Material.h"

// TODO these serve no purpose for now because we support this material only in DX11
#define RENDERTOLIGHTPROBE_MAT_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define RENDERTOLIGHTPROBE_MAT_ROOT_DESCRIPTOR_TABLE_VERTEX_SRV_INDEX 1
#define RENDERTOLIGHTPROBE_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 2

namespace EveryRay_Core
{
	class ER_RenderingObject;
	class ER_Mesh;
	class ER_Camera;

	namespace RenderToLightProbeMaterial_CBufferData {
		struct ER_ALIGN_GPU_BUFFER RenderToLightProbeCB //must match with ER_Illumination::ForwardLightingCB
		{
			XMMATRIX ShadowMatrices[NUM_SHADOW_CASCADES];
			XMMATRIX ViewProjection;
			XMFLOAT4 ShadowTexelSize;
			XMFLOAT4 ShadowCascadeDistances;
			XMFLOAT4 SunDirection;
			XMFLOAT4 SunColor;
			XMFLOAT4 CameraPosition;
		};
	}
	class ER_RenderToLightProbeMaterial : public ER_Material
	{
	public:
		ER_RenderToLightProbeMaterial(ER_Core& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced = false);
		~ER_RenderToLightProbeMaterial();

		void PrepareForRendering(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, ER_Camera* cubemapCamera, ER_RHI_GPURootSignature* rs);
		virtual void PrepareResourcesForStandardMaterial(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, ER_RHI_GPURootSignature* rs) override;
		virtual void CreateVertexBuffer(const ER_Mesh& mesh, ER_RHI_GPUBuffer* vertexBuffer) override;
		virtual int VertexSize() override;

		ER_RHI_GPUConstantBuffer<RenderToLightProbeMaterial_CBufferData::RenderToLightProbeCB> mConstantBuffer;
	};
}