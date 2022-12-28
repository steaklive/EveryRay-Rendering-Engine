#pragma once
#include "ER_Material.h"

#define FRESNEL_OUTLINE_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 0

namespace EveryRay_Core
{
	class ER_RenderingObject;
	class ER_Mesh;
	class ER_Camera;

	namespace FresnelOutlineMaterial_CBufferData {
		struct ER_ALIGN_GPU_BUFFER FresnelOutlineCB //must match with FresnelOutline.hlsl
		{
			XMMATRIX ViewProjection;
			XMFLOAT4 CameraPosition;
			XMFLOAT4 FresnelColorExp;
		};
	}
	class ER_FresnelOutlineMaterial : public ER_Material
	{
	public:
		ER_FresnelOutlineMaterial(ER_Core& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced = false);
		~ER_FresnelOutlineMaterial();

		virtual void PrepareResourcesForStandardMaterial(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, ER_RHI_GPURootSignature* rs) override;
		virtual void CreateVertexBuffer(const ER_Mesh& mesh, ER_RHI_GPUBuffer* vertexBuffer) override;
		virtual int VertexSize() override;

		ER_RHI_GPUConstantBuffer<FresnelOutlineMaterial_CBufferData::FresnelOutlineCB> mConstantBuffer;

		float mFresnelColor[4] = { 1.0f, 0.0, 0.0, 1.0f };
		float mFresnelExponent = 1.75f;
	};
}
