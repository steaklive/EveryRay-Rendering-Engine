#pragma once
#include "ER_Material.h"

namespace Library
{
	class RenderingObject;

	namespace BasicMaterial_CBufferData {
		struct BasicMaterialCB
		{
			XMFLOAT4 Color;
		};
	}
	class ER_BasicColorMaterial : public ER_Material
	{
	public:
		ER_BasicColorMaterial();
		~ER_BasicColorMaterial();

		virtual void PrepareForRendering(Rendering::RenderingObject* aObj) override;
		virtual void CreateInputLayout(D3D11_INPUT_ELEMENT_DESC* inputElementDescriptions, UINT inputElementDescriptionCount, const void* shaderBytecodeWithInputSignature, UINT byteCodeLength) override;

		ConstantBuffer<BasicMaterial_CBufferData::BasicMaterialCB> mConstantBuffer;
	};
}