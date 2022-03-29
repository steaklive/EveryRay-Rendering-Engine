#pragma once
#include "Common.h"
#include "GameComponent.h"

namespace Library {
	struct QuadVertex
	{
		XMFLOAT3 Position;
		XMFLOAT2 TextureCoordinates;
	};
	class QuadRenderer : public GameComponent
	{
		RTTI_DECLARATIONS(QuadRenderer, GameComponent)
	public:
		QuadRenderer(Game& game);
		~QuadRenderer();

		void Draw(ID3D11DeviceContext* context);

	private:
		ID3D11VertexShader* mVS = nullptr;
		ID3D11InputLayout* mInputLayout = nullptr;
		ID3D11Buffer* mVertexBuffer;
		ID3D11Buffer* mIndexBuffer;
	};
}

