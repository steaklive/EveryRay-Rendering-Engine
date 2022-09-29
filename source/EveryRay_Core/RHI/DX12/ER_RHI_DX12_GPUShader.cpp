#include "ER_RHI_DX12_GPUShader.h"
#include "..\..\ER_Utility.h"
#include "..\..\ER_CoreException.h"

namespace EveryRay_Core
{
	static const std::string vertexShaderModel = "vs_5_1";
	static const std::string pixelShaderModel = "ps_5_1";
	static const std::string hullShaderModel = "hs_5_1";
	static const std::string domainShaderModel = "ds_5_1";
	static const std::string geometryShaderModel = "gs_5_1";
	static const std::string computeShaderModel = "cs_5_1";

	ER_RHI_DX12_GPUShader::ER_RHI_DX12_GPUShader()
	{
	}

	ER_RHI_DX12_GPUShader::~ER_RHI_DX12_GPUShader()
	{
		ReleaseObject(mShaderBlob);
	}

	void ER_RHI_DX12_GPUShader::CompileShader(ER_RHI* aRHI, const std::string& path, const std::string& shaderEntry, ER_RHI_SHADER_TYPE type, ER_RHI_InputLayout* aIL)
	{
		mShaderType = type;

		assert(aRHI);
		ER_RHI_DX12* aDX12RHI = static_cast<ER_RHI_DX12*>(aRHI);
		assert(aDX12RHI);

		assert(!shaderEntry.empty());

		std::string compilerErrorMessage = "ER_RHI_DX12: Failed to compile blob from shader: " + path + " with shader entry: " + shaderEntry;

		switch (mShaderType)
		{
		case ER_VERTEX:
			if (FAILED(CompileBlob(ER_Utility::GetFilePath(ER_Utility::ToWideString(path)).c_str(), shaderEntry.c_str(), vertexShaderModel.c_str(), &mShaderBlob)))
				throw ER_CoreException(compilerErrorMessage.c_str());
			break;
		case ER_PIXEL:
			if (FAILED(CompileBlob(ER_Utility::GetFilePath(ER_Utility::ToWideString(path)).c_str(), shaderEntry.c_str(), pixelShaderModel.c_str(), &mShaderBlob)))
				throw ER_CoreException(compilerErrorMessage.c_str());
			break;
		case ER_COMPUTE:
			if (FAILED(CompileBlob(ER_Utility::GetFilePath(ER_Utility::ToWideString(path)).c_str(), shaderEntry.c_str(), computeShaderModel.c_str(), &mShaderBlob)))
				throw ER_CoreException(compilerErrorMessage.c_str());
			break;
		case ER_GEOMETRY:
			if (FAILED(CompileBlob(ER_Utility::GetFilePath(ER_Utility::ToWideString(path)).c_str(), shaderEntry.c_str(), geometryShaderModel.c_str(), &mShaderBlob)))
				throw ER_CoreException(compilerErrorMessage.c_str());
			break;
		case ER_TESSELLATION_HULL:
			if (FAILED(CompileBlob(ER_Utility::GetFilePath(ER_Utility::ToWideString(path)).c_str(), shaderEntry.c_str(), hullShaderModel.c_str(), &mShaderBlob)))
				throw ER_CoreException(compilerErrorMessage.c_str());
			break;
		case ER_TESSELLATION_DOMAIN:
			if (FAILED(CompileBlob(ER_Utility::GetFilePath(ER_Utility::ToWideString(path)).c_str(), shaderEntry.c_str(), domainShaderModel.c_str(), &mShaderBlob)))
				throw ER_CoreException(compilerErrorMessage.c_str());
			break;
		}
	}

	void* ER_RHI_DX12_GPUShader::GetShaderObject()
	{
		return mShaderBlob;
	}

	HRESULT ER_RHI_DX12_GPUShader::CompileBlob(_In_ LPCWSTR srcFile, _In_ LPCSTR entryPoint, _In_ LPCSTR profile, _Outptr_ ID3DBlob** blob)
	{
		if (!srcFile || !entryPoint || !profile || !blob)
			return E_INVALIDARG;

		*blob = nullptr;

		UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
		flags |= D3DCOMPILE_ALL_RESOURCES_BOUND;
#if defined( DEBUG ) || defined( _DEBUG )
		flags |= D3DCOMPILE_DEBUG;
#endif

		const D3D_SHADER_MACRO defines[] =
		{
			"ER_PLATFORM_DX12", "1",
			NULL, NULL
		};

		ID3DBlob* shaderBlob = nullptr;
		ID3DBlob* errorBlob = nullptr;
		HRESULT hr = D3DCompileFromFile(srcFile, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			entryPoint, profile,
			flags, 0, &shaderBlob, &errorBlob);
		if (FAILED(hr))
		{
			if (errorBlob)
			{
				OutputDebugStringA((char*)errorBlob->GetBufferPointer());
				errorBlob->Release();
			}

			if (shaderBlob)
				shaderBlob->Release();

			return hr;
		}

		*blob = shaderBlob;

		return hr;
	}
}