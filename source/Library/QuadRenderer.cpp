#include "QuadRenderer.h"
#include "GameException.h"
#include "ShaderCompiler.h"
#include "Game.h"
#include "Utility.h"

namespace Library {
	RTTI_DEFINITIONS(QuadRenderer)

	QuadRenderer::QuadRenderer(Game& game) : GameComponent(game)
	{
		auto device = game.Direct3DDevice();
		QuadVertex* vertices = new QuadVertex[4];

		// Bottom left.
		vertices[0].Position = XMFLOAT3(1.0f, -1.0f, 0.0f);
		vertices[0].TextureCoordinates = XMFLOAT2(1.0f, 1.0f);

		// Top left.
		vertices[1].Position = XMFLOAT3(-1.0f, -1.0f, 0.0f);
		vertices[1].TextureCoordinates = XMFLOAT2(0.0f, 1.0f);

		// Bottom right.
		vertices[2].Position = XMFLOAT3(-1.0f, 1.0f, 0.0f);
		vertices[2].TextureCoordinates = XMFLOAT2(0.0f, 0.0f);

		// Top right.
		vertices[3].Position = XMFLOAT3(1.0f, 1.0f, 0.0f);
		vertices[3].TextureCoordinates = XMFLOAT2(1.0f, 0.0f);

		unsigned long* indices = new unsigned long[6];

		indices[0] = 0;
		indices[1] = 1;
		indices[2] = 2;
		indices[3] = 2;
		indices[4] = 3;
		indices[5] = 0;

		D3D11_BUFFER_DESC vertexBufferDesc;

		vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		vertexBufferDesc.ByteWidth = sizeof(QuadVertex) * 6;
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		vertexBufferDesc.MiscFlags = 0;
		vertexBufferDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA vertexData;

		vertexData.pSysMem = vertices;
		vertexData.SysMemPitch = 0;
		vertexData.SysMemSlicePitch = 0;

		HRESULT hr = device->CreateBuffer(&vertexBufferDesc, &vertexData, &mVertexBuffer);

		if (FAILED(hr))
			MessageBox(NULL, L"An error occurred while trying to create the vertex buffer for Quad.", L"Error", MB_OK);

		D3D11_BUFFER_DESC indexBufferDesc;

		indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		indexBufferDesc.ByteWidth = sizeof(unsigned long) * 6;
		indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		indexBufferDesc.CPUAccessFlags = 0;
		indexBufferDesc.MiscFlags = 0;
		indexBufferDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA indexData;

		indexData.pSysMem = indices;
		indexData.SysMemPitch = 0;
		indexData.SysMemSlicePitch = 0;

		hr = device->CreateBuffer(&indexBufferDesc, &indexData, &mIndexBuffer);

		if (FAILED(hr))
			MessageBox(NULL, L"An error occurred while trying to create the index buffer for Quad.", L"Error", MB_OK);

		ID3DBlob* blob = nullptr;
		if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Quad.hlsl").c_str(), "VSMain", "vs_5_0", &blob)))
			throw GameException("Failed to load VSMain from shader: Quad.hlsl!");
		if (FAILED(device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mVS)))
			throw GameException("Failed to create vertex shader from Quad.hlsl!");
		{
			D3D11_INPUT_ELEMENT_DESC inputLayoutDesc[2];
			inputLayoutDesc[0].SemanticName = "POSITION";
			inputLayoutDesc[0].SemanticIndex = 0;
			inputLayoutDesc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
			inputLayoutDesc[0].InputSlot = 0;
			inputLayoutDesc[0].AlignedByteOffset = 0;
			inputLayoutDesc[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			inputLayoutDesc[0].InstanceDataStepRate = 0;

			inputLayoutDesc[1].SemanticName = "TEXCOORD";
			inputLayoutDesc[1].SemanticIndex = 0;
			inputLayoutDesc[1].Format = DXGI_FORMAT_R32G32_FLOAT;
			inputLayoutDesc[1].InputSlot = 0;
			inputLayoutDesc[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			inputLayoutDesc[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			inputLayoutDesc[1].InstanceDataStepRate = 0;

			int numElements = sizeof(inputLayoutDesc) / sizeof(inputLayoutDesc[0]);
			device->CreateInputLayout(inputLayoutDesc, numElements, blob->GetBufferPointer(), blob->GetBufferSize(), &mInputLayout);
		}
		blob->Release();
	}
	QuadRenderer::~QuadRenderer()
	{
		ReleaseObject(mVS);
		ReleaseObject(mInputLayout);
		ReleaseObject(mVertexBuffer);
		ReleaseObject(mIndexBuffer);
	}

	void QuadRenderer::Draw(ID3D11DeviceContext* context)
	{
		unsigned int stride;
		unsigned int offset;

		stride = sizeof(QuadVertex);
		offset = 0;

		context->IASetInputLayout(mInputLayout);
		context->VSSetShader(mVS, NULL, NULL);

		context->IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &offset);
		context->IASetIndexBuffer(mIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		context->DrawIndexed(6, 0, 0);
	}


}
