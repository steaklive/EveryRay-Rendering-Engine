#include "ER_DebugLightProbeMaterial.h"
#include "ER_MaterialsCallbacks.h"
#include "ER_Utility.h"
#include "ER_CoreException.h"
#include "ER_Core.h"
#include "ER_Camera.h"
#include "ER_RenderingObject.h"
#include "ER_Mesh.h"
#include "ER_LightProbesManager.h"

namespace EveryRay_Core
{
	ER_DebugLightProbeMaterial::ER_DebugLightProbeMaterial(ER_Core& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced)
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
				ER_Material::CreateVertexShader("content\\shaders\\IBL\\DebugLightProbe.hlsl", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
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
				ER_Material::CreateVertexShader("content\\shaders\\IBL\\DebugLightProbe.hlsl", inputElementDescriptionsInstanced, ARRAYSIZE(inputElementDescriptionsInstanced));
			}
		}

		if (shaderFlags & HAS_PIXEL_SHADER)
			ER_Material::CreatePixelShader("content\\shaders\\IBL\\DebugLightProbe.hlsl");

		mConstantBuffer.Initialize(ER_Material::GetCore()->GetRHI(), "ER_RHI_GPUBuffer: DebugLightProbeMaterial CB");
	}

	ER_DebugLightProbeMaterial::~ER_DebugLightProbeMaterial()
	{
		mConstantBuffer.Release();
		ER_Material::~ER_Material();
	}

	void ER_DebugLightProbeMaterial::PrepareForRendering(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, int aProbeType, ER_RHI_GPURootSignature* rs)
	{
		auto rhi = ER_Material::GetCore()->GetRHI();
		ER_Camera* camera = (ER_Camera*)(ER_Material::GetCore()->GetServices().FindService(ER_Camera::TypeIdClass()));
		
		assert(aObj);
		assert(camera);
		assert(neededSystems.mProbesManager);
				
		mConstantBuffer.Data.ViewProjection = XMMatrixTranspose(camera->ViewMatrix() * camera->ProjectionMatrix());
		mConstantBuffer.Data.World = XMMatrixTranspose(aObj->GetTransformationMatrix());
		mConstantBuffer.Data.CameraPosition = XMFLOAT4{ camera->Position().x, camera->Position().y, camera->Position().z, 1.0f };
		mConstantBuffer.Data.DiscardCulled_IsDiffuse = XMFLOAT2(
			neededSystems.mProbesManager->mDebugDiscardCulledProbes ? 1.0f : -1.0f,
			static_cast<ER_ProbeType>(aProbeType) == DIFFUSE_PROBE ? 1.0f : -1.0f
		);
		mConstantBuffer.ApplyChanges(rhi);
		rhi->SetConstantBuffers(ER_VERTEX, { mConstantBuffer.Buffer() }, 0, rs, DEBUGLIGHTPROBE_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
		rhi->SetConstantBuffers(ER_PIXEL,  { mConstantBuffer.Buffer() }, 0, rs, DEBUGLIGHTPROBE_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
		
		if (static_cast<ER_ProbeType>(aProbeType) == DIFFUSE_PROBE)
		{
			rhi->SetShaderResources(ER_PIXEL, { neededSystems.mProbesManager->GetDiffuseProbesSphericalHarmonicsCoefficientsBuffer() }, 0,
				rs, DEBUGLIGHTPROBE_MAT_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);
		}
		else
		{
			rhi->SetShaderResources(ER_PIXEL, { neededSystems.mProbesManager->GetDiffuseProbesSphericalHarmonicsCoefficientsBuffer(), neededSystems.mProbesManager->GetCulledSpecularProbesTextureArray() }, 0,
				rs, DEBUGLIGHTPROBE_MAT_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);
		}
		rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP });
	}

	void ER_DebugLightProbeMaterial::PrepareResourcesForStandardMaterial(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, ER_RHI_GPURootSignature* rs)
	{
		//not used because this material is not standard
	}

	void ER_DebugLightProbeMaterial::CreateVertexBuffer(const ER_Mesh& mesh, ER_RHI_GPUBuffer* vertexBuffer)
	{
		mesh.CreateVertexBuffer_PositionUvNormalTangent(vertexBuffer);
	}

	int ER_DebugLightProbeMaterial::VertexSize()
	{
		return sizeof(VertexPositionTextureNormalTangent);
	}

}