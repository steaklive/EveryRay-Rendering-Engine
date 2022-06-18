#include "ER_DebugLightProbeMaterial.h"
#include "ER_MaterialsCallbacks.h"
#include "ShaderCompiler.h"
#include "Utility.h"
#include "GameException.h"
#include "Game.h"
#include "Camera.h"
#include "ER_RenderingObject.h"
#include "ER_Mesh.h"
#include "ER_LightProbesManager.h"
#include "ER_GPUBuffer.h"

namespace Library
{
	ER_DebugLightProbeMaterial::ER_DebugLightProbeMaterial(Game& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced)
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
				ER_Material::CreateVertexShader("content\\shaders\\IBL\\DebugLightProbe.hlsl", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
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
				ER_Material::CreateVertexShader("content\\shaders\\IBL\\DebugLightProbe.hlsl", inputElementDescriptionsInstanced, ARRAYSIZE(inputElementDescriptionsInstanced));
			}
		}

		if (shaderFlags & HAS_PIXEL_SHADER)
			ER_Material::CreatePixelShader("content\\shaders\\IBL\\DebugLightProbe.hlsl");

		mConstantBuffer.Initialize(ER_Material::GetGame()->Direct3DDevice());
	}

	ER_DebugLightProbeMaterial::~ER_DebugLightProbeMaterial()
	{
		mConstantBuffer.Release();
		ER_Material::~ER_Material();
	}

	void ER_DebugLightProbeMaterial::PrepareForRendering(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, int aProbeType)
	{
		auto context = ER_Material::GetGame()->Direct3DDeviceContext();
		Camera* camera = (Camera*)(ER_Material::GetGame()->Services().GetService(Camera::TypeIdClass()));
		
		assert(aObj);
		assert(camera);
		assert(neededSystems.mProbesManager);
		
		ER_Material::PrepareForRendering(neededSystems, aObj, meshIndex);
		
		mConstantBuffer.Data.ViewProjection = XMMatrixTranspose(camera->ViewMatrix() * camera->ProjectionMatrix());
		mConstantBuffer.Data.World = XMMatrixTranspose(aObj->GetTransformationMatrix());
		mConstantBuffer.Data.CameraPosition = XMFLOAT4{ camera->Position().x, camera->Position().y, camera->Position().z, 1.0f };
		mConstantBuffer.Data.DiscardCulled_IsDiffuse = XMFLOAT2(
			neededSystems.mProbesManager->mDebugDiscardCulledProbes ? 1.0f : -1.0f,
			static_cast<ER_ProbeType>(aProbeType) == DIFFUSE_PROBE ? 1.0f : -1.0f
		);
		mConstantBuffer.ApplyChanges(context);
		ID3D11Buffer* CBs[1] = { mConstantBuffer.Buffer() };
		
		context->VSSetConstantBuffers(0, 1, CBs);
		context->PSSetConstantBuffers(0, 1, CBs);
		
		ID3D11ShaderResourceView* SRs[2] = 
		{ 
			neededSystems.mProbesManager->GetDiffuseProbesSphericalHarmonicsCoefficientsBuffer()->GetBufferSRV(),
			static_cast<ER_ProbeType>(aProbeType) == DIFFUSE_PROBE ?
				nullptr : neededSystems.mProbesManager->GetCulledSpecularProbesTextureArray()->GetSRV()
		};
		context->PSSetShaderResources(0, 2, SRs);
		
		ID3D11SamplerState* SS[1] = { SamplerStates::TrilinearWrap };
		context->PSSetSamplers(0, 1, SS);
	}

	void ER_DebugLightProbeMaterial::CreateVertexBuffer(const ER_Mesh& mesh, ID3D11Buffer** vertexBuffer)
	{
		mesh.CreateVertexBuffer_PositionUvNormalTangent(vertexBuffer);
	}

	int ER_DebugLightProbeMaterial::VertexSize()
	{
		return sizeof(VertexPositionTextureNormalTangent);
	}

}