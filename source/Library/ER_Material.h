#pragma once

#include "Common.h"
#include "ConstantBuffer.h"
#include "GameComponent.h"
#include "VertexDeclarations.h"
namespace Rendering
{
	class RenderingObject;
}

namespace Library
{
	class Mesh;
	class GameComponent;
	struct ER_MaterialSystems;

	struct MaterialShaderEntries 
	{
		std::string vertexEntry = "VSMain";
		std::string geometryEntry = "GSMain";
		std::string hullEntry = "HSMain";
		std::string domainEntry = "DSMain";
		std::string pixelEntry = "PSMain";
	};

	enum MaterialShaderFlags
	{
		HAS_VERTEX_SHADER		= 0x1L,
		HAS_PIXEL_SHADER		= 0x2L,
		HAS_GEOMETRY_SHADER		= 0x4L,
		HAS_TESSELLATION_SHADER = 0x8L
	};

	class ER_Material : public GameComponent
	{
	public:
		ER_Material(Game& game, const MaterialShaderEntries& shaderEntry, unsigned int shaderFlags);
		~ER_Material();

		virtual void PrepareForRendering(ER_MaterialSystems neededSystems, Rendering::RenderingObject* aObj, int meshIndex);
		
		virtual void CreateInputLayout(D3D11_INPUT_ELEMENT_DESC* inputElementDescriptions, UINT inputElementDescriptionCount, const void* shaderBytecodeWithInputSignature, UINT byteCodeLength);
		
		virtual void CreateVertexBuffer(Mesh& mesh, ID3D11Buffer** vertexBuffer) = 0;

		/*virtual*/ void CreateVertexShader(const std::string& path, D3D11_INPUT_ELEMENT_DESC* inputElementDescriptions, UINT inputElementDescriptionCount);
		/*virtual*/ void CreatePixelShader(const std::string& path);
		/*virtual*/ void CreateGeometryShader(const std::string& path);
		/*virtual*/ void CreateTessellationShader(const std::string& path);

		virtual int VertexSize() = 0;

	protected:

		ID3D11VertexShader* mVS = nullptr;
		ID3D11GeometryShader* mGS = nullptr;
		ID3D11HullShader* mHS = nullptr;
		ID3D11DomainShader* mDS = nullptr;
		ID3D11PixelShader* mPS = nullptr;

		ID3D11InputLayout* mInputLayout = nullptr;

		unsigned int mShaderFlags;
		MaterialShaderEntries mShaderEntries;
	};
}