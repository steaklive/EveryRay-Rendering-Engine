#pragma once

#include "Common.h"
#include "ConstantBuffer.h"
#include "GameComponent.h"
#include "VertexDeclarations.h"
#include "SamplerStates.h"

namespace Library
{
	class RenderingObject;
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
		ER_Material(Game& game, const MaterialShaderEntries& shaderEntry, unsigned int shaderFlags, bool instanced = false);
		~ER_Material();

		virtual void PrepareForRendering(ER_MaterialSystems neededSystems, RenderingObject* aObj, int meshIndex);

		virtual void CreateVertexBuffer(Mesh& mesh, ID3D11Buffer** vertexBuffer) = 0;

		/*virtual*/ void CreateVertexShader(const std::string& path, D3D11_INPUT_ELEMENT_DESC* inputElementDescriptions, UINT inputElementDescriptionCount);
		/*virtual*/ void CreatePixelShader(const std::string& path);
		/*virtual*/ void CreateGeometryShader(const std::string& path);
		/*virtual*/ void CreateTessellationShader(const std::string& path);

		virtual int VertexSize() = 0;

		bool IsSpecial() { return mIsSpecial; };
		//bool IsLayered() { return mIsSpecial; };
	protected:
		virtual void CreateInputLayout(D3D11_INPUT_ELEMENT_DESC* inputElementDescriptions, UINT inputElementDescriptionCount, const void* shaderBytecodeWithInputSignature, UINT byteCodeLength);

		ID3D11VertexShader* mVS = nullptr;
		ID3D11GeometryShader* mGS = nullptr;
		ID3D11HullShader* mHS = nullptr;
		ID3D11DomainShader* mDS = nullptr;
		ID3D11PixelShader* mPS = nullptr;

		ID3D11InputLayout* mInputLayout = nullptr;

		unsigned int mShaderFlags;
		MaterialShaderEntries mShaderEntries;

		bool mIsSpecial = false; //special materials (like shadow map, voxelization, etc.) do not use callbacks in RenderingObjects
	};
}