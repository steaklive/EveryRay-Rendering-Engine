#pragma once
#include "Common.h"

namespace Library
{
	class ShaderCompiler {
	
	public:
		//Compiling shaders
		static HRESULT CompileShader(_In_ LPCWSTR srcFile, _In_ LPCSTR entryPoint, _In_ LPCSTR profile, _Outptr_ ID3DBlob** blob)
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
	
	};

}