#include "ER_RHI_DX11_GPUShader.h"
#include "..\..\ER_Utility.h"
#include "..\..\ER_CoreException.h"

namespace EveryRay_Core
{
	static const std::string vertexShaderModel = "vs_5_0";
	static const std::string pixelShaderModel = "ps_5_0";
	static const std::string hullShaderModel = "hs_5_0";
	static const std::string domainShaderModel = "ds_5_0";
	static const std::string geometryShaderModel = "gs_5_0";
	static const std::string computeShaderModel = "cs_5_0";

	ER_RHI_DX11_GPUShader::ER_RHI_DX11_GPUShader()
	{

	}

	ER_RHI_DX11_GPUShader::~ER_RHI_DX11_GPUShader()
	{
		//ReleaseObject(mInputLayout);
		ReleaseObject(mVS);
		ReleaseObject(mPS);
		ReleaseObject(mGS);
		ReleaseObject(mHS);
		ReleaseObject(mDS);
		ReleaseObject(mCS);
	}

	void ER_RHI_DX11_GPUShader::CompileShader(ER_RHI* aRHI, const std::string& path, const std::string& shaderEntry, ER_RHI_SHADER_TYPE type, ER_RHI_InputLayout* aIL)
	{
		mShaderType = type;

		assert(aRHI);
		ER_RHI_DX11* aDX11RHI = static_cast<ER_RHI_DX11*>(aRHI);
		assert(aDX11RHI);

		assert(!shaderEntry.empty());

		std::string compilerErrorMessage = "ER_RHI_DX11: Failed to compile blob from shader: " + path + " with shader entry: " + shaderEntry;
		std::string createErrorMessage = "ER_RHI_DX11: Failed to create shader from blob: " + path;

		ID3DBlob* blob = nullptr;
		switch (mShaderType)
		{
		case ER_VERTEX:
			if (FAILED(CompileBlob(ER_Utility::GetFilePath(ER_Utility::ToWideString(path)).c_str(), shaderEntry.c_str(), vertexShaderModel.c_str(), &blob)))
				throw ER_CoreException(compilerErrorMessage.c_str());
			if (FAILED(aDX11RHI->GetDevice()->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mVS)))
				throw ER_CoreException(createErrorMessage.c_str());
			break;
		case ER_PIXEL:
			if (FAILED(CompileBlob(ER_Utility::GetFilePath(ER_Utility::ToWideString(path)).c_str(), shaderEntry.c_str(), pixelShaderModel.c_str(), &blob)))
				throw ER_CoreException(compilerErrorMessage.c_str());
			if (FAILED(aDX11RHI->GetDevice()->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mPS)))
				throw ER_CoreException(createErrorMessage.c_str());
			break;
		case ER_COMPUTE:
			if (FAILED(CompileBlob(ER_Utility::GetFilePath(ER_Utility::ToWideString(path)).c_str(), shaderEntry.c_str(), computeShaderModel.c_str(), &blob)))
				throw ER_CoreException(compilerErrorMessage.c_str());
			if (FAILED(aDX11RHI->GetDevice()->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mCS)))
				throw ER_CoreException(createErrorMessage.c_str());
			break;
		case ER_GEOMETRY:
			if (FAILED(CompileBlob(ER_Utility::GetFilePath(ER_Utility::ToWideString(path)).c_str(), shaderEntry.c_str(), geometryShaderModel.c_str(), &blob)))
				throw ER_CoreException(compilerErrorMessage.c_str());
			if (FAILED(aDX11RHI->GetDevice()->CreateGeometryShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mGS)))
				throw ER_CoreException(createErrorMessage.c_str());
			break;
		case ER_TESSELLATION_HULL:
			if (FAILED(CompileBlob(ER_Utility::GetFilePath(ER_Utility::ToWideString(path)).c_str(), shaderEntry.c_str(), hullShaderModel.c_str(), &blob)))
				throw ER_CoreException(compilerErrorMessage.c_str());
			if (FAILED(aDX11RHI->GetDevice()->CreateHullShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mHS)))
				throw ER_CoreException(createErrorMessage.c_str());
			break;
		case ER_TESSELLATION_DOMAIN:
			if (FAILED(CompileBlob(ER_Utility::GetFilePath(ER_Utility::ToWideString(path)).c_str(), shaderEntry.c_str(), domainShaderModel.c_str(), &blob)))
				throw ER_CoreException(compilerErrorMessage.c_str());
			if (FAILED(aDX11RHI->GetDevice()->CreateDomainShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mDS)))
				throw ER_CoreException(createErrorMessage.c_str());
			break;
		}

		if (aIL && mShaderType == ER_VERTEX)
			aDX11RHI->CreateInputLayout(aIL, aIL->mInputElementDescriptions, aIL->mInputElementDescriptionCount, blob->GetBufferPointer(), static_cast<UINT>(blob->GetBufferSize()));

		blob->Release();
	}

	void* ER_RHI_DX11_GPUShader::GetShaderObject()
	{
		switch (mShaderType)
		{
		case ER_VERTEX:
			return mVS;
		case ER_PIXEL:
			return mPS;
		case ER_COMPUTE:
			return mCS;
		case ER_GEOMETRY:
			return mGS;
		case ER_TESSELLATION_HULL:
			return mHS;
		case ER_TESSELLATION_DOMAIN:
			return mDS;
		}
	}

	HRESULT ER_RHI_DX11_GPUShader::CompileBlob(_In_ LPCWSTR srcFile, _In_ LPCSTR entryPoint, _In_ LPCSTR profile, _Outptr_ ID3DBlob** blob)
	{
		if (!srcFile || !entryPoint || !profile || !blob)
			return E_INVALIDARG;

		*blob = nullptr;

		UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
		flags |= D3DCOMPILE_DEBUG;
#endif

		const D3D_SHADER_MACRO defines[] =
		{
			"EXAMPLE_DEFINE", "1",
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