#pragma once

#include "Common.h"
#include "ER_CoreComponent.h"
#include "ER_VertexDeclarations.h"

#include "RHI/ER_RHI.h"

namespace Library
{
	class ER_RenderingObject;
	class ER_Mesh;
	class ER_CoreComponent;
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

	class ER_Material : public ER_CoreComponent
	{
	public:
		ER_Material(ER_Core& game, const MaterialShaderEntries& shaderEntry, unsigned int shaderFlags, bool instanced = false);
		~ER_Material();

		virtual void PrepareForRendering(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex);

		virtual void CreateVertexBuffer(const ER_Mesh& mesh, ER_RHI_GPUBuffer* vertexBuffer) = 0;

		void CreateVertexShader(const std::string& path, ER_RHI_INPUT_ELEMENT_DESC* inputElementDescriptions, UINT inputElementDescriptionCount);
		void CreatePixelShader(const std::string& path);
		void CreateGeometryShader(const std::string& path);
		void CreateTessellationShader(const std::string& path);

		virtual int VertexSize() = 0;

		bool IsSpecial() { return mIsSpecial; };
	protected:
		ER_RHI_InputLayout* mInputLayout = nullptr;
		ER_RHI_GPUShader* mVertexShader = nullptr;
		ER_RHI_GPUShader* mPixelShader = nullptr;
		ER_RHI_GPUShader* mGeometryShader = nullptr;

		unsigned int mShaderFlags;
		MaterialShaderEntries mShaderEntries;

		bool mIsSpecial = false; //special materials (like shadow map, voxelization, etc.) do not use callbacks in RenderingObjects
	};
}