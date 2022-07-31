#include "ER_QuadRenderer.h"
#include "ER_Core.h"
#include "ER_CoreException.h"
#include "ER_Utility.h"

namespace Library {
	RTTI_DEFINITIONS(ER_QuadRenderer)

	ER_QuadRenderer::ER_QuadRenderer(ER_Core& game) : ER_CoreComponent(game)
	{
		auto rhi = game.GetRHI();
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

		mVertexBuffer = rhi->CreateGPUBuffer();
		mVertexBuffer->CreateGPUBufferResource(rhi, vertices, 6, sizeof(QuadVertex), false, ER_BIND_VERTEX_BUFFER);

		mIndexBuffer = rhi->CreateGPUBuffer();
		mIndexBuffer->CreateGPUBufferResource(rhi, indices, 6, sizeof(unsigned long), false, ER_BIND_INDEX_BUFFER, 0, ER_RESOURCE_MISC_NONE, ER_FORMAT_R32_UINT);


		ER_RHI_INPUT_ELEMENT_DESC inputLayoutDesc[2];
		inputLayoutDesc[0].SemanticName = "POSITION";
		inputLayoutDesc[0].SemanticIndex = 0;
		inputLayoutDesc[0].Format = ER_FORMAT_R32G32B32_FLOAT;
		inputLayoutDesc[0].InputSlot = 0;
		inputLayoutDesc[0].AlignedByteOffset = 0;
		inputLayoutDesc[0].IsPerVertex = true;
		inputLayoutDesc[0].InstanceDataStepRate = 0;

		inputLayoutDesc[1].SemanticName = "TEXCOORD";
		inputLayoutDesc[1].SemanticIndex = 0;
		inputLayoutDesc[1].Format = ER_FORMAT_R32G32_FLOAT;
		inputLayoutDesc[1].InputSlot = 0;
		inputLayoutDesc[1].AlignedByteOffset = 0xffffffff;
		inputLayoutDesc[1].IsPerVertex = true;
		inputLayoutDesc[1].InstanceDataStepRate = 0;

		int numElements = sizeof(inputLayoutDesc) / sizeof(inputLayoutDesc[0]);
		mInputLayout = rhi->CreateInputLayout(inputLayoutDesc, numElements);

		mVS = rhi->CreateGPUShader();
		mVS->CompileShader(rhi, "content\\shaders\\Quad.hlsl", "VSMain", ER_VERTEX, mInputLayout);
	}
	ER_QuadRenderer::~ER_QuadRenderer()
	{
		DeleteObject(mVS);
		DeleteObject(mInputLayout);
		DeleteObject(mVertexBuffer);
		DeleteObject(mIndexBuffer);
	}

	void ER_QuadRenderer::Draw(ER_RHI* rhi)
	{
		unsigned int stride;
		unsigned int offset;

		stride = sizeof(QuadVertex);
		offset = 0;

		rhi->SetInputLayout(mInputLayout);
		rhi->SetShader(mVS);
		rhi->SetVertexBuffers({ mVertexBuffer });
		rhi->SetIndexBuffer(mIndexBuffer);
		rhi->SetTopologyType(ER_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		rhi->DrawIndexed(6);
	}
}
