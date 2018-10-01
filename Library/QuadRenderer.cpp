#include "Common.h"
#include "QuadRenderer.h"

namespace Library {
	
	QuadRenderer::QuadRenderer(ID3D11Device* device)
	{
		Vertex* vertices = new Vertex[4];

		// Bottom left.
		vertices[0].Position = Vector3(1.0f, -1.0f, 0.0f);
		vertices[0].TextureCoordinates = Vector2(1.0f, 1.0f);

		// Top left.
		vertices[1].Position = Vector3(-1.0f, -1.0f, 0.0f);
		vertices[1].TextureCoordinates = Vector2(0.0f, 1.0f);

		// Bottom right.
		vertices[2].Position = Vector3(-1.0f, 1.0f, 0.0f);
		vertices[2].TextureCoordinates = Vector2(0.0f, 0.0f);

		// Top right.
		vertices[3].Position = Vector3(1.0f, 1.0f, 0.0f);
		vertices[3].TextureCoordinates = Vector2(1.0f, 0.0f);

		unsigned long* indices = new unsigned long[6];

		indices[0] = 0;
		indices[1] = 1;
		indices[2] = 2;
		indices[3] = 2;
		indices[4] = 3;
		indices[5] = 0;

		D3D11_BUFFER_DESC vertexBufferDesc;

		vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		vertexBufferDesc.ByteWidth = sizeof(Vertex) * 6;
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		vertexBufferDesc.MiscFlags = 0;
		vertexBufferDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA vertexData;

		vertexData.pSysMem = vertices;
		vertexData.SysMemPitch = 0;
		vertexData.SysMemSlicePitch = 0;

		HRESULT hr = device->CreateBuffer(&vertexBufferDesc, &vertexData, &this->vertexBuffer);

		if (FAILED(hr))
		{
			MessageBox(NULL, L"An error occurred while trying to create the vertex buffer.", L"Error", MB_OK);
		}

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

		hr = device->CreateBuffer(&indexBufferDesc, &indexData, &this->indexBuffer);

		if (FAILED(hr))
		{
			MessageBox(NULL, L"An error occurred while trying to create the index buffer.", L"Error", MB_OK);
		}
	}
	QuadRenderer::~QuadRenderer()
	{
	}

	void QuadRenderer::Draw(ID3D11DeviceContext* context)
	{
		unsigned int stride;
		unsigned int offset;

		stride = sizeof(Vertex);
		offset = 0;

		context->IASetVertexBuffers(0, 1, this->vertexBuffer.GetAddressOf(), &stride, &offset);
		context->IASetIndexBuffer(this->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		context->DrawIndexed(6, 0, 0);
	}


}
