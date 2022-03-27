#include "ER_GBufferMaterial.h"
#include "ER_MaterialsCallbacks.h"
#include "ShaderCompiler.h"
#include "Utility.h"
#include "GameException.h"
#include "Game.h"
#include "Camera.h"
#include "RenderingObject.h"
#include "Mesh.h"
#include "ER_ShadowMapper.h"

namespace Library
{
	ER_GBufferMaterial::ER_GBufferMaterial(Game& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced)
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
				ER_Material::CreateVertexShader("content\\shaders\\GBuffer.hlsl", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
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
				ER_Material::CreateVertexShader("content\\shaders\\GBuffer.hlsl", inputElementDescriptionsInstanced, ARRAYSIZE(inputElementDescriptionsInstanced));
			}
		}

		if (shaderFlags & HAS_PIXEL_SHADER)
			ER_Material::CreatePixelShader("content\\shaders\\GBuffer.hlsl");

		mConstantBuffer.Initialize(ER_Material::GetGame()->Direct3DDevice());
	}

	ER_GBufferMaterial::~ER_GBufferMaterial()
	{
		mConstantBuffer.Release();
		ER_Material::~ER_Material();
	}

	void ER_GBufferMaterial::PrepareForRendering(ER_MaterialSystems neededSystems, RenderingObject* aObj, int meshIndex)
	{
		auto context = ER_Material::GetGame()->Direct3DDeviceContext();
		Camera* camera = (Camera*)(ER_Material::GetGame()->Services().GetService(Camera::TypeIdClass()));

		assert(aObj);
		assert(camera);

		ER_Material::PrepareForRendering(neededSystems, aObj, meshIndex);

		mConstantBuffer.Data.ViewProjection = XMMatrixTranspose(camera->ViewMatrix() * camera->ProjectionMatrix());
		mConstantBuffer.Data.World = XMMatrixTranspose(aObj->GetTransformationMatrix());
		mConstantBuffer.Data.Reflection_Foliage_UseGlobalDiffuseProbe_POM_MaskFactor = XMFLOAT4(
			aObj->GetMeshReflectionFactor(meshIndex) ? 1.0f : 0.0f,
			aObj->GetFoliageMask() ? 1.0f : 0.0f,
			aObj->GetUseGlobalLightProbeMask() ? 1.0f : 0.0f,
			aObj->IsParallaxOcclusionMapping() ? 1.0f : 0.0f);
		mConstantBuffer.Data.SkipDeferredLighting = XMFLOAT4(aObj->IsForwardShading() ? 1.0f : 0.0f, 0.0, 0.0, 0.0);
		mConstantBuffer.ApplyChanges(context);
		ID3D11Buffer* CBs[1] = { mConstantBuffer.Buffer() };

		context->VSSetConstantBuffers(0, 1, CBs);
		context->PSSetConstantBuffers(0, 1, CBs);

		ID3D11ShaderResourceView* SRs[5] = { 
			aObj->GetTextureData(meshIndex).AlbedoMap,
			aObj->GetTextureData(meshIndex).NormalMap,
			aObj->GetTextureData(meshIndex).RoughnessMap,
			aObj->GetTextureData(meshIndex).MetallicMap,
			aObj->GetTextureData(meshIndex).HeightMap
		};
		context->PSSetShaderResources(0, 5, SRs);

		ID3D11SamplerState* SS[1] = { SamplerStates::TrilinearWrap };
		context->PSSetSamplers(0, 1, SS);
	}

	void ER_GBufferMaterial::CreateVertexBuffer(Mesh& mesh, ID3D11Buffer** vertexBuffer)
	{
		mesh.CreateVertexBuffer_PositionUvNormalTangent(vertexBuffer);
	}

	int ER_GBufferMaterial::VertexSize()
	{
		return sizeof(VertexPositionTextureNormalTangent);
	}

}