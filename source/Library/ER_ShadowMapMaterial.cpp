#include "ER_ShadowMapMaterial.h"
#include "ER_MaterialsCallbacks.h"
#include "ShaderCompiler.h"
#include "ER_Utility.h"
#include "ER_CoreException.h"
#include "ER_Core.h"
#include "ER_Camera.h"
#include "ER_RenderingObject.h"
#include "ER_Mesh.h"
#include "ER_ShadowMapper.h"

namespace Library
{
	ER_ShadowMapMaterial::ER_ShadowMapMaterial(ER_Core& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced)
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
					{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
				};
				ER_Material::CreateVertexShader("content\\shaders\\ShadowMap.hlsl", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
			}
			else
			{
				D3D11_INPUT_ELEMENT_DESC inputElementDescriptionsInstanced[] =
				{
					{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
					{ "WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
					{ "WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
					{ "WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 }
				};
				ER_Material::CreateVertexShader("content\\shaders\\ShadowMap.hlsl", inputElementDescriptionsInstanced, ARRAYSIZE(inputElementDescriptionsInstanced));
			}
		}

		if (shaderFlags & HAS_PIXEL_SHADER)
			ER_Material::CreatePixelShader("content\\shaders\\ShadowMap.hlsl");

		mConstantBuffer.Initialize(ER_Material::GetCore()->Direct3DDevice());
	}

	ER_ShadowMapMaterial::~ER_ShadowMapMaterial()
	{
		mConstantBuffer.Release();
		ER_Material::~ER_Material();
	}

	void ER_ShadowMapMaterial::PrepareForRendering(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, int cascadeIndex)
	{
		auto context = ER_Material::GetCore()->Direct3DDeviceContext();
		ER_Camera* camera = (ER_Camera*)(ER_Material::GetCore()->Services().GetService(ER_Camera::TypeIdClass()));

		assert(aObj);
		assert(camera);
		assert(neededSystems.mShadowMapper);

		ER_Material::PrepareForRendering(neededSystems, aObj, meshIndex);

		XMMATRIX lvp = neededSystems.mShadowMapper->GetViewMatrix(cascadeIndex) * neededSystems.mShadowMapper->GetProjectionMatrix(cascadeIndex);

		mConstantBuffer.Data.WorldLightViewProjection = XMMatrixTranspose(aObj->GetTransformationMatrix() * lvp);
		mConstantBuffer.Data.LightViewProjection = XMMatrixTranspose(lvp);
		mConstantBuffer.ApplyChanges(context);
		ID3D11Buffer* CBs[1] = { mConstantBuffer.Buffer() };

		context->VSSetConstantBuffers(0, 1, CBs);
		context->PSSetConstantBuffers(0, 1, CBs);

		ID3D11ShaderResourceView* SRs[1] = { aObj->GetTextureData(meshIndex).AlbedoMap };
		context->PSSetShaderResources(0, 1, SRs);

		ID3D11SamplerState* SS[1] = { SamplerStates::TrilinearWrap };
		context->PSSetSamplers(0, 1, SS);
	}

	void ER_ShadowMapMaterial::CreateVertexBuffer(const ER_Mesh& mesh, ID3D11Buffer** vertexBuffer)
	{
		mesh.CreateVertexBuffer_PositionUv(vertexBuffer);
	}

	int ER_ShadowMapMaterial::VertexSize()
	{
		return sizeof(VertexPositionTexture);
	}

}