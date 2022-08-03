#include "stdafx.h"

#include "ER_Material.h"
#include "ER_Core.h"
#include "ER_CoreException.h"
#include "ER_Model.h"
#include "ER_RenderingObject.h"
#include "ER_Utility.h"
#include "ER_MaterialsCallbacks.h"

namespace EveryRay_Core
{
	static const std::string vertexShaderModel = "vs_5_0";
	static const std::string pixelShaderModel = "ps_5_0";
	static const std::string hullShaderModel = "hs_5_0";
	static const std::string domainShaderModel = "ds_5_0";
	static const std::string geometryShaderModel = "gs_5_0";

	ER_Material::ER_Material(ER_Core& game, const MaterialShaderEntries& shaderEntry, unsigned int shaderFlags, bool instanced)
		: ER_CoreComponent(game),
		mShaderFlags(shaderFlags),
		mShaderEntries(shaderEntry)
	{
	}

	ER_Material::~ER_Material()
	{
		DeleteObject(mInputLayout);
		DeleteObject(mVertexShader);
		DeleteObject(mGeometryShader);
		DeleteObject(mPixelShader);
	}

	// Setting up the pipeline before the draw call
	void ER_Material::PrepareForRendering(ER_MaterialSystems neededSystems, ER_RenderingObject* aObj, int meshIndex)
	{
		ER_RHI* rhi = GetCore()->GetRHI();

		rhi->SetInputLayout(mInputLayout);

		if (mShaderFlags & HAS_VERTEX_SHADER)
			rhi->SetShader(mVertexShader);
		if (mShaderFlags & HAS_GEOMETRY_SHADER)
			rhi->SetShader(mGeometryShader);
		if (mShaderFlags & HAS_PIXEL_SHADER)
			rhi->SetShader(mPixelShader);

		//override: apply constant buffers
		//override: set constant buffers in context
		//override: set samplers in context
		//override: set resources in context
	}

	void ER_Material::CreateVertexShader(const std::string& path, ER_RHI_INPUT_ELEMENT_DESC* inputElementDescriptions, UINT inputElementDescriptionCount)
	{
		ER_RHI* rhi = GetCore()->GetRHI();

		mInputLayout = rhi->CreateInputLayout(inputElementDescriptions, inputElementDescriptionCount);
		mVertexShader = rhi->CreateGPUShader();
		mVertexShader->CompileShader(GetCore()->GetRHI(), path, mShaderEntries.vertexEntry, ER_VERTEX, mInputLayout);
	}

	void ER_Material::CreatePixelShader(const std::string& path)
	{
		ER_RHI* rhi = GetCore()->GetRHI();

		mPixelShader = rhi->CreateGPUShader();
		mPixelShader->CompileShader(GetCore()->GetRHI(), path, mShaderEntries.pixelEntry, ER_PIXEL);
	}

	void ER_Material::CreateGeometryShader(const std::string& path)
	{
		ER_RHI* rhi = GetCore()->GetRHI();

		mGeometryShader = rhi->CreateGPUShader();
		mGeometryShader->CompileShader(GetCore()->GetRHI(), path, mShaderEntries.geometryEntry, ER_GEOMETRY);
	}

	void ER_Material::CreateTessellationShader(const std::string& path)
	{
		//TODO
	}
}