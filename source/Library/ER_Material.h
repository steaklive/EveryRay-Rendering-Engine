#pragma once

#include "Common.h"
#include "ConstantBuffer.h"

namespace Library
{
	class Model;
	class Mesh;
	class RenderingObject;
	class GameComponent;

	class ER_Material : public GameComponent
	{
	public:
		ER_Material(Game& game);
		~ER_Material();

		virtual void PrepareForRendering(Rendering::RenderingObject* aObj);

		/*virtual*/ void CreateVertexShader(const std::string& path);
		/*virtual*/ void CreatePixelShader(const std::string& path);
		/*virtual*/ void CreateGeometryShader(const std::string& path);
		/*virtual*/ void CreateTessellationShader(const std::string& path);

		virtual void CreateInputLayout(D3D11_INPUT_ELEMENT_DESC* inputElementDescriptions, UINT inputElementDescriptionCount, const void* shaderBytecodeWithInputSignature, UINT byteCodeLength);

		bool IsReady() { return mIsReady; }
	protected:

		ID3D11VertexShader* mVS = nullptr;
		ID3D11PixelShader* mPS = nullptr;
		ID3D11InputLayout* mInputLayout = nullptr;

		bool mIsReady = false;
	};
}