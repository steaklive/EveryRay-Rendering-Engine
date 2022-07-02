#pragma once
#include "Common.h"
#include "ER_CoreComponent.h"

namespace Library {
	struct QuadVertex
	{
		XMFLOAT3 Position;
		XMFLOAT2 TextureCoordinates;
	};
	class ER_QuadRenderer : public ER_CoreComponent
	{
		RTTI_DECLARATIONS(ER_QuadRenderer, ER_CoreComponent)
	public:
		ER_QuadRenderer(ER_Core& game);
		~ER_QuadRenderer();

		void Draw(ID3D11DeviceContext* context);

	private:
		ID3D11VertexShader* mVS = nullptr;
		ID3D11InputLayout* mInputLayout = nullptr;
		ID3D11Buffer* mVertexBuffer;
		ID3D11Buffer* mIndexBuffer;
	};
}

