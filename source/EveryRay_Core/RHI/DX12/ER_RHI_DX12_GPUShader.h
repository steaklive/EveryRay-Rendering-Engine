#pragma once
#include "ER_RHI_DX12.h"

namespace EveryRay_Core
{
	class ER_RHI_DX12_GPUShader : public ER_RHI_GPUShader
	{
	public:
		ER_RHI_DX12_GPUShader();
		virtual ~ER_RHI_DX12_GPUShader();

		virtual void CompileShader(ER_RHI* aRHI, const std::string& path, const std::string& shaderEntry, ER_RHI_SHADER_TYPE type, ER_RHI_InputLayout* aIL = nullptr) override;
		virtual void* GetShaderObject() override;

	private:
		HRESULT CompileBlob(_In_ LPCWSTR srcFile, _In_ LPCSTR entryPoint, _In_ LPCSTR profile, _Outptr_ ID3DBlob** blob);

		ID3DBlob* mShaderBlob = nullptr;
	};
}