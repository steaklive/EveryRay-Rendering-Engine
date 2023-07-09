#include "ER_GBufferMaterial.h"
#include "ER_MaterialsCallbacks.h"
#include "ER_Utility.h"
#include "ER_CoreException.h"
#include "ER_Core.h"
#include "ER_Camera.h"
#include "ER_RenderingObject.h"
#include "ER_Mesh.h"
#include "ER_ShadowMapper.h"

namespace EveryRay_Core
{
	ER_GBufferMaterial::ER_GBufferMaterial(ER_Core& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced)
		: ER_Material(game, entries, shaderFlags)
	{
		mIsStandard = false;

		if (shaderFlags & HAS_VERTEX_SHADER)
		{
			if (!instanced)
			{
				ER_RHI_INPUT_ELEMENT_DESC inputElementDescriptions[] =
				{
					{ "POSITION", 0, ER_FORMAT_R32G32B32A32_FLOAT, 0, 0, true, 0 },
					{ "TEXCOORD", 0, ER_FORMAT_R32G32_FLOAT, 0, 0xffffffff, true, 0 },
					{ "NORMAL", 0, ER_FORMAT_R32G32B32_FLOAT, 0, 0xffffffff, true, 0 },
					{ "TANGENT", 0, ER_FORMAT_R32G32B32_FLOAT, 0, 0xffffffff, true, 0 }
				};
				ER_Material::CreateVertexShader("content\\shaders\\GBuffer.hlsl", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
			}
			else
			{
				ER_RHI_INPUT_ELEMENT_DESC inputElementDescriptionsInstanced[] =
				{
					{ "POSITION", 0, ER_FORMAT_R32G32B32A32_FLOAT, 0, 0, true, 0 },
					{ "TEXCOORD", 0, ER_FORMAT_R32G32_FLOAT, 0, 0xffffffff, true, 0 },
					{ "NORMAL", 0, ER_FORMAT_R32G32B32_FLOAT, 0, 0xffffffff, true, 0 },
					{ "TANGENT", 0, ER_FORMAT_R32G32B32_FLOAT, 0, 0xffffffff, true, 0 },
					{ "WORLD", 0, ER_FORMAT_R32G32B32A32_FLOAT, 1, 0, false, 1 },
					{ "WORLD", 1, ER_FORMAT_R32G32B32A32_FLOAT, 1, 16, false, 1 },
					{ "WORLD", 2, ER_FORMAT_R32G32B32A32_FLOAT, 1, 32, false, 1 },
					{ "WORLD", 3, ER_FORMAT_R32G32B32A32_FLOAT, 1, 48, false, 1 }
				};
				ER_Material::CreateVertexShader("content\\shaders\\GBuffer.hlsl", inputElementDescriptionsInstanced, ARRAYSIZE(inputElementDescriptionsInstanced));
			}
		}

		if (shaderFlags & HAS_PIXEL_SHADER)
			ER_Material::CreatePixelShader("content\\shaders\\GBuffer.hlsl");

		mConstantBuffer.Initialize(ER_Material::GetCore()->GetRHI(), "ER_RHI_GPUBuffer: GBufferMaterial CB");
	}

	ER_GBufferMaterial::~ER_GBufferMaterial()
	{
		mConstantBuffer.Release();
		ER_Material::~ER_Material();
	}

	void ER_GBufferMaterial::PrepareForRendering(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, ER_RHI_GPURootSignature* rs)
	{
		auto rhi = ER_Material::GetCore()->GetRHI();
		ER_Camera* camera = (ER_Camera*)(ER_Material::GetCore()->GetServices().FindService(ER_Camera::TypeIdClass()));

		assert(aObj);
		assert(camera);

		mConstantBuffer.Data.ViewProjection = XMMatrixTranspose(camera->ViewMatrix() * camera->ProjectionMatrix());
		mConstantBuffer.ApplyChanges(rhi);
		
		if (!rhi->IsRootConstantSupported())
		{
			rhi->SetConstantBuffers(ER_VERTEX, { mConstantBuffer.Buffer(), aObj->GetObjectsConstantBuffer().Buffer(), aObj->GetObjectsFakeRootConstantBuffer().Buffer() },
				0, rs, GBUFFER_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
		}
		else
			rhi->SetConstantBuffers(ER_VERTEX, { mConstantBuffer.Buffer(), aObj->GetObjectsConstantBuffer().Buffer() }, 0, rs, GBUFFER_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);

		rhi->SetConstantBuffers(ER_PIXEL, { mConstantBuffer.Buffer() , aObj->GetObjectsConstantBuffer().Buffer() }, 0, rs, GBUFFER_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);

		std::vector<ER_RHI_GPUResource*> resources;
		resources.push_back(aObj->GetTextureData(meshIndex).AlbedoMap);	
		resources.push_back(aObj->GetTextureData(meshIndex).NormalMap);
		resources.push_back(aObj->GetTextureData(meshIndex).RoughnessMap);
		resources.push_back(aObj->GetTextureData(meshIndex).MetallicMap);
		resources.push_back(aObj->GetTextureData(meshIndex).HeightMap);	
		resources.push_back(aObj->GetTextureData(meshIndex).ExtraMaskMap);
		rhi->SetShaderResources(ER_PIXEL, resources, 0, rs, GBUFFER_MAT_ROOT_DESCRIPTOR_TABLE_PIXEL_SRV_INDEX);
		rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP }, 0, rs);

		if (aObj->IsGPUIndirectlyRendered())
			rhi->SetShaderResources(ER_VERTEX, { aObj->GetIndirectNewInstanceBuffer() }, static_cast<int>(resources.size()), rs, GBUFFER_MAT_ROOT_DESCRIPTOR_TABLE_VERTEX_SRV_INDEX);
	}

	void ER_GBufferMaterial::PrepareResourcesForStandardMaterial(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, ER_RHI_GPURootSignature* rs)
	{
		//not used because this material is not standard
	}

	void ER_GBufferMaterial::SetRootConstantForMaterial(UINT a32BitConstant)
	{
		auto rhi = ER_Material::GetCore()->GetRHI();
		rhi->SetRootConstant(a32BitConstant, GBUFFER_MAT_ROOT_CONSTANT_INDEX);
	}

	void ER_GBufferMaterial::CreateVertexBuffer(const ER_Mesh& mesh, ER_RHI_GPUBuffer* vertexBuffer)
	{
		mesh.CreateVertexBuffer_PositionUvNormalTangent(vertexBuffer);
	}

	int ER_GBufferMaterial::VertexSize()
	{
		return sizeof(VertexPositionTextureNormalTangent);
	}

}