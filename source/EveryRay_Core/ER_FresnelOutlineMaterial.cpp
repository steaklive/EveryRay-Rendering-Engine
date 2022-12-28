#include "ER_FresnelOutlineMaterial.h"
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
	static const std::string psoNameNonInstanced = "ER_RHI_GPUPipelineStateObject: FresnelOutlineMaterial";
	static const std::string psoNameInstanced = "ER_RHI_GPUPipelineStateObject: FresnelOutlineMaterial w/ Instancing";

	ER_FresnelOutlineMaterial::ER_FresnelOutlineMaterial(ER_Core& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced)
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
				ER_Material::CreateVertexShader("content\\shaders\\FresnelOutline.hlsl", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
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
				ER_Material::CreateVertexShader("content\\shaders\\FresnelOutline.hlsl", inputElementDescriptionsInstanced, ARRAYSIZE(inputElementDescriptionsInstanced));
			}
		}

		if (shaderFlags & HAS_PIXEL_SHADER)
			ER_Material::CreatePixelShader("content\\shaders\\FresnelOutline.hlsl");

		mConstantBuffer.Initialize(ER_Material::GetCore()->GetRHI(), "ER_RHI_GPUBuffer: SimpleSnowMaterial CB");
	}

	ER_FresnelOutlineMaterial::~ER_FresnelOutlineMaterial()
	{
		mConstantBuffer.Release();
		ER_Material::~ER_Material();
	}

	void ER_FresnelOutlineMaterial::PrepareResourcesForStandardMaterial(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, ER_RHI_GPURootSignature* rs)
	{
		//ImGui::Begin("Fresnel Effect");
		//ImGui::ColorEdit4("Color", mFresnelColor);
		//ImGui::SliderFloat("Exponent", &mFresnelExponent, 0.25f, 10.0f);
		//ImGui::End();

		auto rhi = ER_Material::GetCore()->GetRHI();
		ER_Camera* camera = (ER_Camera*)(ER_Material::GetCore()->GetServices().FindService(ER_Camera::TypeIdClass()));

		assert(aObj);
		assert(camera);
		assert(neededSystems.mIllumination);
		rhi->SetRootSignature(rs);
		rhi->SetTopologyType(ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		const std::string& psoName = aObj->IsInstanced() ? psoNameInstanced : psoNameNonInstanced;
		if (!rhi->IsPSOReady(psoName))
		{
			rhi->InitializePSO(psoName);
			PrepareShaders();
			rhi->SetRenderTargetFormats({ neededSystems.mIllumination->GetLocalIlluminationRT() }, neededSystems.mIllumination->GetGBufferDepth()); // we assume that we render in local RT (don't like it but idk how to properly pass RT atm)
			rhi->SetRasterizerState(ER_NO_CULLING);
			rhi->SetBlendState(ER_ALPHA_BLEND);
			rhi->SetDepthStencilState(ER_DEPTH_ONLY_WRITE_COMPARISON_LESS_EQUAL);
			rhi->SetRootSignatureToPSO(psoName, rs);
			rhi->SetTopologyTypeToPSO(psoName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			rhi->FinalizePSO(psoName);
		}
		rhi->SetPSO(psoName);

		mConstantBuffer.Data.ViewProjection = XMMatrixTranspose(neededSystems.mCamera->ViewMatrix() * neededSystems.mCamera->ProjectionMatrix());
		mConstantBuffer.Data.CameraPosition = XMFLOAT4{ neededSystems.mCamera->Position().x, neededSystems.mCamera->Position().y, neededSystems.mCamera->Position().z, 1.0f };
		mConstantBuffer.Data.FresnelColorExp = XMFLOAT4{ aObj->GetFresnelOutlineColor().x, aObj->GetFresnelOutlineColor().y, aObj->GetFresnelOutlineColor().z, mFresnelExponent };
		mConstantBuffer.ApplyChanges(rhi);
		rhi->SetConstantBuffers(ER_VERTEX, { mConstantBuffer.Buffer(), aObj->GetObjectsConstantBuffer().Buffer() }, 0, rs, FRESNEL_OUTLINE_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
		rhi->SetConstantBuffers(ER_PIXEL, { mConstantBuffer.Buffer(), aObj->GetObjectsConstantBuffer().Buffer() }, 0, rs, FRESNEL_OUTLINE_MAT_ROOT_DESCRIPTOR_TABLE_CBV_INDEX);
	}

	void ER_FresnelOutlineMaterial::CreateVertexBuffer(const ER_Mesh& mesh, ER_RHI_GPUBuffer* vertexBuffer)
	{
		mesh.CreateVertexBuffer_PositionUvNormalTangent(vertexBuffer);
	}

	int ER_FresnelOutlineMaterial::VertexSize()
	{
		return sizeof(VertexPositionTextureNormalTangent);
	}

}