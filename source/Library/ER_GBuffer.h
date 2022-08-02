#pragma once

#include "Common.h"
#include "ER_CoreComponent.h"
#include "RHI/ER_RHI.h"

namespace Library
{
	class ER_Scene;
	class ER_Camera;

	class ER_GBuffer: public ER_CoreComponent
	{
	public:
		ER_GBuffer(ER_Core& game, ER_Camera& camera, int width, int height);
		~ER_GBuffer();
	
		void Initialize();
		void Update(const ER_CoreTime& time);

		void Start();
		void End();
		void Draw(const ER_Scene* scene);

		ER_RHI_GPUTexture* GetAlbedo() { return mAlbedoBuffer; }
		ER_RHI_GPUTexture* GetNormals() { return mNormalBuffer; }
		ER_RHI_GPUTexture* GetPositions() { return mPositionsBuffer; }
		ER_RHI_GPUTexture* GetExtraBuffer() { return mExtraBuffer; } // [reflection mask, roughness, metalness, foliage mask]
		ER_RHI_GPUTexture* GetExtra2Buffer() { return mExtra2Buffer; } // [global diffuse probe mask, height (for POM, etc.), SSS, skip deferred lighting]
		ER_RHI_GPUTexture* GetDepth() { return mDepthBuffer; }

	private:
		ER_RHI_GPUTexture* mDepthBuffer = nullptr;
		ER_RHI_GPUTexture* mAlbedoBuffer= nullptr;
		ER_RHI_GPUTexture* mNormalBuffer= nullptr;
		ER_RHI_GPUTexture* mPositionsBuffer = nullptr;
		ER_RHI_GPUTexture* mExtraBuffer = nullptr;
		ER_RHI_GPUTexture* mExtra2Buffer = nullptr;

		int mWidth;
		int mHeight;
	};
}