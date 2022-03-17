#include "stdafx.h"

#include "ER_Material.h"
#include "GameComponent.h"
#include "Game.h"
#include "GameException.h"
#include "Model.h"
#include "RenderingObject.h"
#include "ShaderCompiler.h"
#include "Utility.h"

namespace Library
{
	ER_Material::ER_Material(Game& game) 
		: GameComponent(game)
	{
	}

	ER_Material::~ER_Material()
	{
		ReleaseObject(mInputLayout);
		ReleaseObject(mVS);
		ReleaseObject(mPS);
	}

	void ER_Material::PrepareForRendering(Rendering::RenderingObject* aObj)
	{
		ID3D11DeviceContext* context = GetGame()->Direct3DDeviceContext();

		context->IASetInputLayout(mInputLayout);
		context->VSSetShader(mVS, NULL, 0);
		context->PSSetShader(mPS, NULL, NULL);

		//override: apply constant buffers
	}

	void ER_Material::CreateVertexShader(const std::string& path)
	{
		std::string compilerErrorMessage = "Failed to compile VSMain from shader: " + path;
		std::string createErrorMessage = "Failed to create vertex shader from shader: " + path;

		ID3DBlob* blob = nullptr;
		if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(Utility::ToWideString(path)).c_str(), "VSMain", "vs_5_0", &blob)))
			throw GameException(compilerErrorMessage.c_str());
		if (FAILED(GetGame()->Direct3DDevice()->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mVS)))
			throw GameException(createErrorMessage.c_str());
		blob->Release();
	}

	void ER_Material::CreatePixelShader(const std::string& path)
	{
		std::string compilerErrorMessage = "Failed to compile PSMain from shader: " + path;
		std::string createErrorMessage = "Failed to create pixel shader from shader: " + path;

		ID3DBlob* blob = nullptr;
		if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(Utility::ToWideString(path)).c_str(), "PSMain", "ps_5_0", &blob)))
			throw GameException(compilerErrorMessage.c_str());
		if (FAILED(GetGame()->Direct3DDevice()->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mPS)))
			throw GameException(createErrorMessage.c_str());
		blob->Release();
	}

	void ER_Material::CreateInputLayout(D3D11_INPUT_ELEMENT_DESC* inputElementDescriptions, UINT inputElementDescriptionCount, const void* shaderBytecodeWithInputSignature, UINT byteCodeLength)
	{
		HRESULT hr = GetGame()->Direct3DDevice()->CreateInputLayout(inputElementDescriptions, inputElementDescriptionCount, shaderBytecodeWithInputSignature, byteCodeLength, &mInputLayout);
		if (FAILED(hr))
			throw GameException("ID3D11Device::CreateInputLayout() failed.", hr);
	}
}