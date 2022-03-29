#pragma once
#include "Common.h"
#include "GameComponent.h"

namespace Library {
	struct QuadVertex
	{
		XMFLOAT3 Position;
		XMFLOAT2 TextureCoordinates;
	};
	class ER_QuadRenderer : public GameComponent
	{
		RTTI_DECLARATIONS(ER_QuadRenderer, GameComponent)
	public:
		ER_QuadRenderer(Game& game);
		~ER_QuadRenderer();

		void Draw(ID3D11DeviceContext* context);

	private:
		ID3D11VertexShader* mVS = nullptr;
		ID3D11InputLayout* mInputLayout = nullptr;
		ID3D11Buffer* mVertexBuffer;
		ID3D11Buffer* mIndexBuffer;
	};
}

