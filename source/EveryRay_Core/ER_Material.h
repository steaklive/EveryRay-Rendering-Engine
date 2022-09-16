#pragma once

#include "Common.h"
#include "ER_CoreComponent.h"
#include "ER_VertexDeclarations.h"

#include "RHI/ER_RHI.h"

namespace EveryRay_Core
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

		// This method has to be overriden in standard material classes (not special like shadow map material, etc.) and can be used as a callback if bound to ER_RenderingObject
		virtual void PrepareResourcesForStandardMaterial(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex, ER_RHI_GPURootSignature* rs) = 0;
		virtual void CreateVertexBuffer(const ER_Mesh& mesh, ER_RHI_GPUBuffer* vertexBuffer) = 0;
		virtual int VertexSize() = 0;

		void CreateVertexShader(const std::string& path, ER_RHI_INPUT_ELEMENT_DESC* inputElementDescriptions, UINT inputElementDescriptionCount);
		void CreatePixelShader(const std::string& path);
		void CreateGeometryShader(const std::string& path);
		void CreateTessellationShader(const std::string& path);

		void PrepareShaders();

		bool IsStandard() { return mIsStandard; };
	protected:
		ER_RHI_InputLayout* mInputLayout = nullptr;
		ER_RHI_GPUShader* mVertexShader = nullptr;
		ER_RHI_GPUShader* mPixelShader = nullptr;
		ER_RHI_GPUShader* mGeometryShader = nullptr;

		unsigned int mShaderFlags;
		MaterialShaderEntries mShaderEntries;

		bool mIsStandard = true; // non-standard materials (like shadow map, voxelization, gbuffer, etc.) are processed in their systems (ER_ShadowMapper, ER_Illumination, etc.)
	};
}