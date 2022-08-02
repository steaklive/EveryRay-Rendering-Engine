#include "ER_RenderToLightProbeMaterial.h"
#include "ER_MaterialsCallbacks.h"
#include "ER_Utility.h"
#include "ER_CoreException.h"
#include "ER_Core.h"
#include "ER_Camera.h"
#include "ER_RenderingObject.h"
#include "ER_Mesh.h"
#include "ER_ShadowMapper.h"
#include "DirectionalLight.h"

namespace Library
{
	ER_RenderToLightProbeMaterial::ER_RenderToLightProbeMaterial(ER_Core& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced)
		: ER_Material(game, entries, shaderFlags)
	{
		mIsSpecial = true;

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
				ER_Material::CreateVertexShader("content\\shaders\\ForwardLighting.hlsl", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
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
				ER_Material::CreateVertexShader("content\\shaders\\ForwardLighting.hlsl", inputElementDescriptionsInstanced, ARRAYSIZE(inputElementDescriptionsInstanced));
			}
		}

		if (shaderFlags & HAS_PIXEL_SHADER)
			ER_Material::CreatePixelShader("content\\shaders\\ForwardLighting.hlsl");

		mConstantBuffer.Initialize(ER_Material::GetCore()->GetRHI());
	}

	ER_RenderToLightProbeMaterial::~ER_RenderToLightProbeMaterial()
	{
		mConstantBuffer.Release();
		ER_Material::~ER_Material();
	}

	void ER_RenderToLightProbeMaterial::PrepareForRendering(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, ER_Camera* cubemapCamera)
	{
		auto rhi = ER_Material::GetCore()->GetRHI();

		assert(aObj);
		assert(cubemapCamera);
		assert(neededSystems.mShadowMapper);
		assert(neededSystems.mDirectionalLight);

		ER_Material::PrepareForRendering(neededSystems, aObj, meshIndex);

		for (int i = 0; i < NUM_SHADOW_CASCADES; i++)
			mConstantBuffer.Data.ShadowMatrices[i] = XMMatrixTranspose(neededSystems.mShadowMapper->GetViewMatrix(i) * neededSystems.mShadowMapper->GetProjectionMatrix(i) * XMLoadFloat4x4(&ER_MatrixHelper::GetProjectionShadowMatrix()));
		mConstantBuffer.Data.ViewProjection = XMMatrixTranspose(cubemapCamera->ViewMatrix() * cubemapCamera->ProjectionMatrix());
		mConstantBuffer.Data.World = XMMatrixTranspose(aObj->GetTransformationMatrix());
		mConstantBuffer.Data.ShadowTexelSize = XMFLOAT4{ 1.0f / neededSystems.mShadowMapper->GetResolution(), 1.0f, 1.0f, 1.0f };
		mConstantBuffer.Data.ShadowCascadeDistances = XMFLOAT4{ cubemapCamera->GetCameraFarShadowCascadeDistance(0), cubemapCamera->GetCameraFarShadowCascadeDistance(1), cubemapCamera->GetCameraFarShadowCascadeDistance(2), 1.0f };
		mConstantBuffer.Data.SunDirection = XMFLOAT4{ -neededSystems.mDirectionalLight->Direction().x, -neededSystems.mDirectionalLight->Direction().y, -neededSystems.mDirectionalLight->Direction().z, 1.0f };
		mConstantBuffer.Data.SunColor = XMFLOAT4{ neededSystems.mDirectionalLight->GetDirectionalLightColor().x, neededSystems.mDirectionalLight->GetDirectionalLightColor().y, neededSystems.mDirectionalLight->GetDirectionalLightColor().z, neededSystems.mDirectionalLight->GetDirectionalLightIntensity() };
		mConstantBuffer.Data.CameraPosition = XMFLOAT4{ cubemapCamera->Position().x, cubemapCamera->Position().y, cubemapCamera->Position().z, 1.0f };
		mConstantBuffer.Data.UseGlobalDiffuseProbe = false;
		mConstantBuffer.ApplyChanges(rhi);
		rhi->SetConstantBuffers(ER_VERTEX, { mConstantBuffer.Buffer() });
		rhi->SetConstantBuffers(ER_PIXEL, { mConstantBuffer.Buffer() });

		std::vector<ER_RHI_GPUResource*> resources;
		resources.push_back(aObj->GetTextureData(meshIndex).AlbedoMap);
		resources.push_back(aObj->GetTextureData(meshIndex).NormalMap);
		resources.push_back(aObj->GetTextureData(meshIndex).MetallicMap);
		resources.push_back(aObj->GetTextureData(meshIndex).RoughnessMap);
		resources.push_back(aObj->GetTextureData(meshIndex).HeightMap);
		rhi->SetShaderResources(ER_PIXEL, resources);

		std::vector<ER_RHI_GPUResource*> shadowResources;
		for (int i = 0; i < NUM_SHADOW_CASCADES; i++)
			shadowResources.push_back(neededSystems.mShadowMapper->GetShadowTexture(i));
		rhi->SetShaderResources(ER_PIXEL, shadowResources, 5);

		rhi->SetSamplers(ER_PIXEL, { ER_RHI_SAMPLER_STATE::ER_TRILINEAR_WRAP, ER_RHI_SAMPLER_STATE::ER_SHADOW_SS });
	}

	void ER_RenderToLightProbeMaterial::CreateVertexBuffer(const ER_Mesh& mesh, ER_RHI_GPUBuffer* vertexBuffer)
	{
		mesh.CreateVertexBuffer_PositionUvNormalTangent(vertexBuffer);
	}

	int ER_RenderToLightProbeMaterial::VertexSize()
	{
		return sizeof(VertexPositionTextureNormalTangent);
	}

}