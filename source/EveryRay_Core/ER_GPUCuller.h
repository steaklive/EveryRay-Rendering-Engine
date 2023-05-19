#pragma once
#include "Common.h"
#include "ER_CoreComponent.h"
#include "RHI/ER_RHI.h"

namespace EveryRay_Core
{
	class ER_Camera;
	class ER_Scene;

	namespace IndirectCullingCBufferData
	{
		struct ER_ALIGN_GPU_BUFFER MeshConstants
		{
			XMINT4 IndexCount_StartIndexLoc_BaseVtxLoc_StartInstLoc;
		};

		struct ER_ALIGN_GPU_BUFFER CameraConstants
		{
			XMFLOAT4 FrustumPlanes[6];
			XMFLOAT4 LodCameraDistances;
		};
	}

	class ER_GPUCuller : public ER_CoreComponent
	{
	public:
		ER_GPUCuller(ER_Core& game, ER_Camera& camera);
		~ER_GPUCuller();

		void Initialize();
		void PerformCull(ER_Scene* aScene);

	private:
		ER_Core& mCore;
		ER_Camera& mCamera;

		ER_RHI_GPUShader* mIndirectCullingCS = nullptr;
		ER_RHI_GPUConstantBuffer<IndirectCullingCBufferData::MeshConstants> mMeshConstantBuffer;
		ER_RHI_GPUConstantBuffer<IndirectCullingCBufferData::CameraConstants> mCameraConstantBuffer;
		ER_RHI_GPURootSignature* mIndirectCullingRS = nullptr;
		const std::string mPSOName = "ER_RHI_GPUPipelineStateObject: Indirect Cull Pass";
		
		int mIndirectCullsCounterPerFrame = 0;
	};
}