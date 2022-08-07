#pragma once
#include "ER_RHI_DX11.h"

namespace EveryRay_Core
{
	class ER_RHI_DX11_GPUShader : public ER_RHI_GPUShader
	{
	public:
		ER_RHI_DX11_GPUShader();
		virtual ~ER_RHI_DX11_GPUShader();

		virtual void CompileShader(ER_RHI* aRHI, const std::string& path, const std::string& shaderEntry, ER_RHI_SHADER_TYPE type, ER_RHI_InputLayout* aIL = nullptr) override;
		virtual void* GetShaderObject() override;

	private:
		HRESULT CompileBlob(_In_ LPCWSTR srcFile, _In_ LPCSTR entryPoint, _In_ LPCSTR profile, _Outptr_ ID3DBlob** blob);

		ID3D11VertexShader* mVS = nullptr;
		ID3D11GeometryShader* mGS = nullptr;
		ID3D11HullShader* mHS = nullptr;
		ID3D11DomainShader* mDS = nullptr;
		ID3D11PixelShader* mPS = nullptr;
		ID3D11ComputeShader* mCS = nullptr;
	};
}