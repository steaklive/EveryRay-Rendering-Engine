#pragma once
#include "Common.h"
#include "RHI/ER_RHI.h"

namespace Library
{
	template<typename T> class ConstantBuffer
	{
	public:
		T Data;
	protected:
		ER_RHI_GPUBuffer* buffer;
		bool initialized;
	public:
		ConstantBuffer() : initialized(false)
		{
			ZeroMemory(&Data, sizeof(T));
		}

		void Release() {
			DeleteObject(buffer);
		}

		ER_RHI_GPUBuffer* Buffer() const
		{
			return buffer;
		}

		void Initialize(ER_RHI* rhi)
		{
			buffer = rhi->CreateGPUBuffer();
			buffer->CreateGPUBufferResource(rhi, nullptr, 1, static_cast<UINT32>(sizeof(T) + (16 - (sizeof(T) % 16))), true, ER_BIND_CONSTANT_BUFFER);
			initialized = true;
		}

		void ApplyChanges(ER_RHI* rhi)
		{
			assert(initialized);
			assert(rhi);
			assert(buffer);

			rhi->UpdateBuffer(buffer, &Data, sizeof(T));
		}
	};

}