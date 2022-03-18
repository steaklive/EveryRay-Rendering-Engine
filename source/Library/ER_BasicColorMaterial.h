#pragma once
#include "ER_Material.h"

namespace Rendering
{
	class RenderingObject;
}
namespace Library
{
	namespace BasicMaterial_CBufferData {
		struct BasicMaterialCB
		{
			XMFLOAT4 Color;
		};
	}
	class ER_BasicColorMaterial : public ER_Material
	{
	public:
		ER_BasicColorMaterial(Game& game, unsigned int shaderFlags);
		~ER_BasicColorMaterial();

		virtual void PrepareForRendering(Rendering::RenderingObject* aObj, int meshIndex) override;

		ConstantBuffer<BasicMaterial_CBufferData::BasicMaterialCB> mConstantBuffer;
	};
}