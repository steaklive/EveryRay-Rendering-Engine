#include "ER_BasicColorMaterial.h"
#include "ShaderCompiler.h"
#include "Utility.h"
#include "GameException.h"

namespace Library
{
	ER_BasicColorMaterial::ER_BasicColorMaterial()
	{
		ER_Material::CreateVertexShader("content\\shaders\\BasicColor.hlsl");
		ER_Material::CreatePixelShader("content\\shaders\\BasicColor.hlsl");

		mConstantBuffer.Initialize(ER_Material::GetGame()->Direct3DDevice());
	}

	ER_BasicColorMaterial::~ER_BasicColorMaterial()
	{
		mConstantBuffer.Release();
	}

}