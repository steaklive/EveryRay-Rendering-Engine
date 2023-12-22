#include "ER_SimpleSnowMaterial.h"
#include "ER_MaterialsCallbacks.h"
#include "ER_Utility.h"
#include "ER_CoreException.h"
#include "ER_Core.h"
#include "ER_Camera.h"
#include "ER_RenderingObject.h"
#include "ER_Mesh.h"
#include "ER_ShadowMapper.h"
#include "ER_DirectionalLight.h"
#include "ER_Illumination.h"

namespace EveryRay_Core
{
	static const std::string psoNameNonInstanced = "ER_RHI_GPUPipelineStateObject: SimpleSnowMaterial";
	static const std::string psoNameInstanced = "ER_RHI_GPUPipelineStateObject: SimpleSnowMaterial w/ Instancing";

	ER_SimpleSnowMaterial::ER_SimpleSnowMaterial(ER_Core& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced)
		: ER_Material(game, entries, shaderFlags)
	{
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
				ER_Material::CreateVertexShader("content\\shaders\\SimpleSnow.hlsl", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
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
					{ "WORLD", 1, ER_FORMAT_R32G32B32A32_FLOAT, 1, 16,false, 1 },
					{ "WORLD", 2, ER_FORMAT_R32G32B32A32_FLOAT, 1, 32,false, 1 },
					{ "WORLD", 3, ER_FORMAT_R32G32B32A32_FLOAT, 1, 48,false, 1 }
				};
				ER_Material::CreateVertexShader("content\\shaders\\SimpleSnow.hlsl", inputElementDescriptionsInstanced, ARRAYSIZE(inputElementDescriptionsInstanced));
			}
		}

		if (shaderFlags & HAS_PIXEL_SHADER)
			ER_Material::CreatePixelShader("content\\shaders\\SimpleSnow.hlsl");

		mConstantBuffer.Initialize(ER_Material::GetCore()->GetRHI(), "ER_RHI_GPUBuffer: SimpleSnowMaterial CB");
	}

	ER_SimpleSnowMaterial::~ER_SimpleSnowMaterial()
	{
		mConstantBuffer.Release();
		ER_Material::~ER_Material();
	}

	void ER_SimpleSnowMaterial::PrepareResourcesForStandardMaterial(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, ER_RHI_GPURootSignature* rs)
	{
		//ImGui::Begin("Snow");
		//ImGui::SliderFloat("Level", &mSnowLevel, -1.0f, 1.0f);
		//ImGui::SliderFloat("Depth", &mSnowDepth, 0.0f, 1.0f);
		//ImGui::SliderFloat("UV Scale", &mSnowUVScale, 0.0f, 100.0f);
		//ImGui::End();

		auto rhi = ER_Material::GetCore()->GetRHI();
		ER_Camera* camera = (ER_Camera*)(ER_Material::GetCore()->GetServices().FindService(ER_Camera::TypeIdClass()));

		assert(aObj);
		assert(camera);
		assert(neededSystems.mIllumination);
		rhi->SetRootSignature(rs);
		rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		const std::string psoName = aObj->IsInstanced() ? psoNameInstanced : psoNameNonInstanced;
		if (!rhi->IsPSOReady(psoName))
		{
			rhi->InitializePSO(psoName);
			PrepareShaders();
			rhi->SetRenderTargetFormats({ neededSystems.mIllumination->GetLocalIlluminationRT() }, neededSystems.mIllumination->GetGBufferDepth()); // we assume that we render in local RT (don't like it but idk how to properly pass RT atm)
			rhi->SetRasterizerState(ER_NO_CULLING);
			rhi->SetBlendState(ER_NO_BLEND);
			rhi->SetDepthStencilState(ER_DEPTH_ONLY_WRITE_COMPARISON_LESS_EQUAL);
			rhi->SetRootSignatureToPSO(psoName, rs);
			rhi->SetTopologyTypeToPSO(psoName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			rhi->FinalizePSO(psoName);
		}
		rhi->SetPSO(psoName);

		for (int i = 0; i < NUM_SHADOW_CASCADES; i++)
			mConstantBuffer.Data.ShadowMatrices[i] = XMMatrixTranspose(neededSystems.mShadowMapper->GetViewMatrix(i) * neededSystems.mShadowMapper->GetProjectionMatrix(i) * XMLoadFloat4x4(&ER_MatrixHelper::GetProjectionShadowMatrix()));
		mConstantBuffer.Data.ViewProjection = XMMatrixTranspose(neededSystems.mCamera->ViewMatrix() * neededSystems.mCamera->ProjectionMatrix());
		mConstantBuffer.Data.ShadowTexelSize = XMFLOAT4{ 1.0f / neededSystems.mShadowMapper->GetResolution(), 1.0f, 1.0f, 1.0f };
		mConstantBuffer.Data.ShadowCascadeDistances = XMFLOAT4{ 
			neededSystems.mShadowMapper->GetCameraFarShadowCascadeDistance(0), 
			neededSystems.mShadowMapper->GetCameraFarShadowCascadeDistance(1), 
			neededSystems.mShadowMapper->GetCameraFarShadowCascadeDistance(2), 1.0f };
		mConstantBuffer.Data.SunDirection = XMFLOAT4{ -neededSystems.mDirectionalLight->Direction().x, -neededSystems.mDirectionalLight->Direction().y, -neededSystems.mDirectionalLight->Direction().z, 1.0f };
		mConstantBuffer.Data.SunColor = XMFLOAT4{ neededSystems.mDirectionalLight->GetColor().x, neededSystems.mDirectionalLight->GetColor().y, neededSystems.mDirectionalLight->GetColor().z, neededSystems.mDirectionalLight->mLightIntensity };
		mConstantBuffer.Data.CameraPosition = XMFLOAT4{ neededSystems.mCamera->Position().x, neededSystems.mCamera->Position().y, neededSystems.mCamera->Position().z, 1.0f };
		mConstantBuffer.Data.SnowDepthLevel = XMFLOAT4{ mSnowDepth, mSnowLevel, mSnowUVScale, 0.0f };
		mConstantBuffer.ApplyChanges(rhi);
		rhi->SetConstantBuffers(ER_VERTEX, { mConstantBuffer.Buffer(), aObj->GetObjectsConstantBuffer().Buffer() }, 0, rs, SNOW_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
		rhi->SetConstantBuffers(ER_PIXEL, { mConstantBuffer.Buffer(), aObj->GetObjectsConstantBuffer().Buffer() }, 0, rs, SNOW_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);

		std::vector<ER_RHI_GPUResource*> resources(LIGHTING_SRV_INDEX_MAX_RESERVED_FOR_TEXTURES + NUM_SHADOW_CASCADES + 1);
		resources[0] = aObj->GetSnowAlbedoTexture();
		resources[1] = aObj->GetSnowNormalTexture();
		resources[2] = aObj->GetSnowRoughnessTexture();
		resources[3] = aObj->GetTextureData(meshIndex).NormalMap;
		for (int i = 0; i < NUM_SHADOW_CASCADES; i++)
			resources[LIGHTING_SRV_INDEX_CSM_START + i] = neededSystems.mShadowMapper->GetShadowTexture(i);
		rhi->SetShaderResources(ER_PIXEL, resources, 0, rs, SNOW_MAT_ROOT_DESCRIPTOR_TABLE_SRV_INDEX);

		rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SAMPLER_STATE::ER_SHADOW_SS });
	}

	void ER_SimpleSnowMaterial::CreateVertexBuffer(const ER_Mesh& mesh, ER_RHI_GPUBuffer* vertexBuffer)
	{
		mesh.CreateVertexBuffer_PositionUvNormalTangent(vertexBuffer);
	}

	int ER_SimpleSnowMaterial::VertexSize()
	{
		return sizeof(VertexPositionTextureNormalTangent);
	}

}