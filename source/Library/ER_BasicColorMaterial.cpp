#include "ER_BasicColorMaterial.h"
#include "ShaderCompiler.h"
#include "Utility.h"
#include "GameException.h"
#include "Game.h"
#include "Camera.h"
#include "RenderingObject.h"
#include "Mesh.h"
#include "ER_MaterialsCallbacks.h"
namespace Library
{
	ER_BasicColorMaterial::ER_BasicColorMaterial(Game& game, const MaterialShaderEntries& entries, unsigned int shaderFlags)
		: ER_Material(game, entries, shaderFlags)
	{

		if (shaderFlags & HAS_VERTEX_SHADER)
		{
			D3D11_INPUT_ELEMENT_DESC inputElementDescriptions[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
			};
			ER_Material::CreateVertexShader("content\\shaders\\BasicColor.hlsl", inputElementDescriptions, ARRAYSIZE(inputElementDescriptions));
		}
		
		if (shaderFlags & HAS_PIXEL_SHADER)
			ER_Material::CreatePixelShader("content\\shaders\\BasicColor.hlsl");

		mConstantBuffer.Initialize(ER_Material::GetGame()->Direct3DDevice());
	}

	ER_BasicColorMaterial::~ER_BasicColorMaterial()
	{
		mConstantBuffer.Release();
		ER_Material::~ER_Material();
	}

	void ER_BasicColorMaterial::PrepareForRendering(ER_MaterialSystems neededSystems, Rendering::RenderingObject* aObj, int meshIndex)
	{
		auto context = ER_Material::GetGame()->Direct3DDeviceContext();
		Camera* camera = (Camera*)(ER_Material::GetGame()->Services().GetService(Camera::TypeIdClass()));
		
		assert(aObj);
		assert(camera);

		ER_Material::PrepareForRendering(neededSystems, aObj, meshIndex);

		mConstantBuffer.Data.World = XMMatrixTranspose(aObj->GetTransformationMatrix());
		mConstantBuffer.Data.ViewProjection = XMMatrixTranspose(camera->ViewMatrix() * camera->ProjectionMatrix());
		mConstantBuffer.Data.Color = XMFLOAT4{0.0, 1.0, 0.0, 0.0};
		mConstantBuffer.ApplyChanges(context);
		ID3D11Buffer* CBs[1] = { mConstantBuffer.Buffer() };

		context->VSSetConstantBuffers(0, 1, CBs);
		context->PSSetConstantBuffers(0, 1, CBs);
	}

	void ER_BasicColorMaterial::CreateVertexBuffer(Mesh& mesh, ID3D11Buffer** vertexBuffer)
	{
		assert(aObj);
		mesh.CreateVertexBuffer_Position(vertexBuffer);
	}

	int ER_BasicColorMaterial::VertexSize()
	{
		return sizeof(VertexPosition);
	}

}