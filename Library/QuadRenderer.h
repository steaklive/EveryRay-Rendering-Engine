#pragma once

#include <windows.h>
#include <wrl/client.h>

#include <d3d11_1.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <d3dcompiler.h>

#include "CommonStates.h"
#include "DDSTextureLoader.h"
#include "DirectXHelpers.h"
#include "Effects.h"
#include "GamePad.h"
#include "GeometricPrimitive.h"
#include "Model.h"
#include "PrimitiveBatch.h"
#include "ScreenGrab.h"
#include "SimpleMath.h"
#include "SpriteBatch.h"
#include "SpriteFont.h"
#include "VertexTypes.h"
#include "WICTextureLoader.h"

using namespace DirectX::SimpleMath;

struct Vertex
{
	Vector3 Position;
	Vector2 TextureCoordinates;
};

namespace Library {
	class QuadRenderer
	{
	public:
		QuadRenderer(ID3D11Device* device);
		~QuadRenderer();

		void Draw(ID3D11DeviceContext* context);

	private:
		Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
	};
}

