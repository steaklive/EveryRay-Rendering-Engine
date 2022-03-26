#include "ER_RenderToLightProbeMaterial.h"
#include "ER_MaterialsCallbacks.h"
#include "ShaderCompiler.h"
#include "Utility.h"
#include "GameException.h"
#include "Game.h"
#include "Camera.h"
#include "RenderingObject.h"
#include "Mesh.h"
#include "ER_ShadowMapper.h"
#include "DirectionalLight.h"

namespace Library
{
	ER_RenderToLightProbeMaterial::ER_RenderToLightProbeMaterial(Game& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced)
		: ER_Material(game, entries, shaderFlags)
	{
		mIsSpecial = true;

		if (shaderFlags & HAS_VERTEX_SHADER)
		{
			if (!instanced)
			{
				D3D11_INPUT_ELEMENT_DESC inputElementDescriptions[] =
				{
					{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
				};
				ER_Material::CreateVertexShader("content\\shaders\\ForwardLighting.hlsl", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
			}
			else
			{
				D3D11_INPUT_ELEMENT_DESC inputElementDescriptionsInstanced[] =
				{
					{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
					{ "WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
					{ "WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
					{ "WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 }
				};
				ER_Material::CreateVertexShader("content\\shaders\\ForwardLighting.hlsl", inputElementDescriptionsInstanced, ARRAYSIZE(inputElementDescriptionsInstanced));
			}
		}

		if (shaderFlags & HAS_PIXEL_SHADER)
			ER_Material::CreatePixelShader("content\\shaders\\ForwardLighting.hlsl");

		mConstantBuffer.Initialize(ER_Material::GetGame()->Direct3DDevice());
	}

	ER_RenderToLightProbeMaterial::~ER_RenderToLightProbeMaterial()
	{
		mConstantBuffer.Release();
		ER_Material::~ER_Material();
	}

	void ER_RenderToLightProbeMaterial::PrepareForRendering(ER_MaterialSystems neededSystems, Rendering::RenderingObject* aObj, int meshIndex, Camera* cubemapCamera)
	{
		auto context = ER_Material::GetGame()->Direct3DDeviceContext();

		assert(aObj);
		assert(cubemapCamera);
		assert(neededSystems.mShadowMapper);
		assert(neededSystems.mDirectionalLight);

		ER_Material::PrepareForRendering(neededSystems, aObj, meshIndex);

		for (size_t i = 0; i < NUM_SHADOW_CASCADES; i++)
			mConstantBuffer.Data.ShadowMatrices[i] = XMMatrixTranspose(neededSystems.mShadowMapper->GetViewMatrix(i) * neededSystems.mShadowMapper->GetProjectionMatrix(i) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix()));
		mConstantBuffer.Data.ViewProjection = XMMatrixTranspose(cubemapCamera->ViewMatrix() * cubemapCamera->ProjectionMatrix());
		mConstantBuffer.Data.World = XMMatrixTranspose(aObj->GetTransformationMatrix());
		mConstantBuffer.Data.ShadowTexelSize = XMFLOAT4{ 1.0f / neededSystems.mShadowMapper->GetResolution(), 1.0f, 1.0f, 1.0f };
		mConstantBuffer.Data.ShadowCascadeDistances = XMFLOAT4{ cubemapCamera->GetCameraFarCascadeDistance(0), cubemapCamera->GetCameraFarCascadeDistance(1), cubemapCamera->GetCameraFarCascadeDistance(2), 1.0f };
		mConstantBuffer.Data.SunDirection = XMFLOAT4{ -neededSystems.mDirectionalLight->Direction().x, -neededSystems.mDirectionalLight->Direction().y, -neededSystems.mDirectionalLight->Direction().z, 1.0f };
		mConstantBuffer.Data.SunColor = XMFLOAT4{ neededSystems.mDirectionalLight->GetDirectionalLightColor().x, neededSystems.mDirectionalLight->GetDirectionalLightColor().y, neededSystems.mDirectionalLight->GetDirectionalLightColor().z, neededSystems.mDirectionalLight->GetDirectionalLightIntensity() };
		mConstantBuffer.Data.CameraPosition = XMFLOAT4{ cubemapCamera->Position().x, cubemapCamera->Position().y, cubemapCamera->Position().z, 1.0f };
		mConstantBuffer.Data.UseGlobalDiffuseProbe = false;
		mConstantBuffer.ApplyChanges(context);

		ID3D11Buffer* CBs[1] = { mConstantBuffer.Buffer() };

		context->VSSetConstantBuffers(0, 1, CBs);
		context->PSSetConstantBuffers(0, 1, CBs);

		ID3D11ShaderResourceView* SRs[8] = {
			aObj->GetTextureData(meshIndex).AlbedoMap,
			aObj->GetTextureData(meshIndex).NormalMap,
			aObj->GetTextureData(meshIndex).MetallicMap,
			aObj->GetTextureData(meshIndex).RoughnessMap,
			aObj->GetTextureData(meshIndex).HeightMap
		};
		for (int i = 0; i < NUM_SHADOW_CASCADES; i++)
			SRs[5 + i] = neededSystems.mShadowMapper->GetShadowTexture(i);

		context->PSSetShaderResources(0, 8, SRs);

		ID3D11SamplerState* SS[2] = { SamplerStates::TrilinearWrap, SamplerStates::ShadowSamplerState };
		context->PSSetSamplers(0, 2, SS);
	}

	void ER_RenderToLightProbeMaterial::CreateVertexBuffer(Mesh& mesh, ID3D11Buffer** vertexBuffer)
	{
		mesh.CreateVertexBuffer_PositionUvNormalTangent(vertexBuffer);
	}

	int ER_RenderToLightProbeMaterial::VertexSize()
	{
		return sizeof(VertexPositionTextureNormalTangent);
	}

}