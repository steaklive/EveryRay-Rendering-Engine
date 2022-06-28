#include "ER_VoxelizationMaterial.h"
#include "ER_MaterialsCallbacks.h"
#include "ShaderCompiler.h"
#include "Utility.h"
#include "GameException.h"
#include "Game.h"
#include "ER_Camera.h"
#include "ER_RenderingObject.h"
#include "ER_Mesh.h"
#include "ER_ShadowMapper.h"
#include "DirectionalLight.h"
#include "ER_Illumination.h"
namespace Library
{
	ER_VoxelizationMaterial::ER_VoxelizationMaterial(Game& game, const MaterialShaderEntries& entries, unsigned int shaderFlags, bool instanced)
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
				ER_Material::CreateVertexShader("content\\shaders\\Voxelization.hlsl", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
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
				ER_Material::CreateVertexShader("content\\shaders\\Voxelization.hlsl", inputElementDescriptionsInstanced, ARRAYSIZE(inputElementDescriptionsInstanced));
			}
		}

		if (shaderFlags & HAS_GEOMETRY_SHADER)
			ER_Material::CreateGeometryShader("content\\shaders\\Voxelization.hlsl");

		if (shaderFlags & HAS_PIXEL_SHADER)
			ER_Material::CreatePixelShader("content\\shaders\\Voxelization.hlsl");

		mConstantBuffer.Initialize(ER_Material::GetGame()->Direct3DDevice());
	}

	ER_VoxelizationMaterial::~ER_VoxelizationMaterial()
	{
		mConstantBuffer.Release();
		ER_Material::~ER_Material();
	}

	void ER_VoxelizationMaterial::PrepareForRendering(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, float voxelScale, float voxelTexSize, const XMFLOAT4& voxelCameraPos)
	{
		auto context = ER_Material::GetGame()->Direct3DDeviceContext();
		ER_Camera* camera = (ER_Camera*)(ER_Material::GetGame()->Services().GetService(ER_Camera::TypeIdClass()));

		assert(aObj);
		assert(camera);
		assert(neededSystems.mShadowMapper);
		assert(neededSystems.mDirectionalLight);

		ER_Material::PrepareForRendering(neededSystems, aObj, meshIndex);
		
		int shadowCascadeIndex = 1;
		mConstantBuffer.Data.World = XMMatrixTranspose(aObj->GetTransformationMatrix());
		mConstantBuffer.Data.ViewProjection = XMMatrixTranspose(camera->ViewMatrix() * camera->ProjectionMatrix());
		mConstantBuffer.Data.ShadowMatrix = XMMatrixTranspose(neededSystems.mShadowMapper->GetViewMatrix(shadowCascadeIndex) * neededSystems.mShadowMapper->GetProjectionMatrix(shadowCascadeIndex) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix()));
		mConstantBuffer.Data.ShadowTexelSize = XMFLOAT4{ 1.0f / neededSystems.mShadowMapper->GetResolution(), 1.0f, 1.0f, 1.0f };
		mConstantBuffer.Data.ShadowCascadeDistances = XMFLOAT4{ camera->GetCameraFarShadowCascadeDistance(0), camera->GetCameraFarShadowCascadeDistance(1), camera->GetCameraFarShadowCascadeDistance(2), 1.0f };
		mConstantBuffer.Data.VoxelCameraPos = voxelCameraPos;
		mConstantBuffer.Data.VoxelTextureDimension = voxelTexSize;
		mConstantBuffer.Data.WorldVoxelScale = voxelScale;
		mConstantBuffer.ApplyChanges(context);

		ID3D11Buffer* CBs[1] = { mConstantBuffer.Buffer() };

		context->VSSetConstantBuffers(0, 1, CBs);
		context->GSSetConstantBuffers(0, 1, CBs);
		context->PSSetConstantBuffers(0, 1, CBs);

		ID3D11ShaderResourceView* SRs[2] = {
			aObj->GetTextureData(meshIndex).AlbedoMap,
			neededSystems.mShadowMapper->GetShadowTexture(1)
		};
		context->PSSetShaderResources(0, 2, SRs);

		ID3D11SamplerState* SS[2] = { SamplerStates::TrilinearWrap, SamplerStates::ShadowSamplerState };
		context->PSSetSamplers(0, 2, SS);
	}

	void ER_VoxelizationMaterial::CreateVertexBuffer(const ER_Mesh& mesh, ID3D11Buffer** vertexBuffer)
	{
		mesh.CreateVertexBuffer_PositionUvNormalTangent(vertexBuffer);
	}

	int ER_VoxelizationMaterial::VertexSize()
	{
		return sizeof(VertexPositionTextureNormalTangent);
	}

}