#include "stdafx.h"

#include "ER_GPUCuller.h"
#include "ER_Core.h"
#include "ER_CoreException.h"
#include "ER_CoreTime.h"
#include "ER_Camera.h"
#include "ER_MatrixHelper.h"
#include "ER_Model.h"
#include "ER_Mesh.h"
#include "ER_Scene.h"
#include "ER_RenderingObject.h"
#include "ER_Utility.h"

#define GPU_CULL_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX 0
#define GPU_CULL_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX 1
#define GPU_CULL_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 2

namespace EveryRay_Core
{

	ER_GPUCuller::ER_GPUCuller(ER_Core& game, ER_Camera& camera)
		: mCamera(camera), mCore(game)
	{
	}

	ER_GPUCuller::~ER_GPUCuller()
	{
		DeleteObject(mIndirectCullingCS);
		DeleteObject(mIndirectCullingRS);

		mMeshConstantBuffer.Release();
		mCameraConstantBuffer.Release();
	}

	void ER_GPUCuller::Initialize()
	{
		auto rhi = mCore.GetRHI();

		mIndirectCullingCS = rhi->CreateGPUShader();
		mIndirectCullingCS->CompileShader(rhi, "content\\shaders\\IndirectCulling.hlsl", "CSMain", ER_COMPUTE);

		//TODO root signature

		//cbuffers
		mMeshConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: GPU Culler Mesh CB");
		mCameraConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: GPU Culler Camera CB");
	}

	void ER_GPUCuller::PerformCull(ER_Scene* aScene)
	{
		assert(aScene);

		mIndirectCullsCounterPerFrame = 0;
		auto rhi = mCore.GetRHI();

		rhi->SetRootSignature(mIndirectCullingRS, true);
		if (!rhi->IsPSOReady(mPSOName, true))
		{
			rhi->InitializePSO(mPSOName, true);
			rhi->SetShader(mIndirectCullingCS);
			rhi->SetRootSignatureToPSO(mPSOName, mIndirectCullingRS, true);
			rhi->FinalizePSO(mPSOName, true);
		}
		rhi->SetPSO(mPSOName, true);

		for (int i = 0; i < 6; ++i)
			mCameraConstantBuffer.Data.FrustumPlanes[i] = mCamera.GetFrustum().Planes()[i];
		mCameraConstantBuffer.Data.LodCameraDistances = XMFLOAT4(ER_Utility::DistancesLOD[0], ER_Utility::DistancesLOD[1], ER_Utility::DistancesLOD[2], 0.0f);
		mCameraConstantBuffer.ApplyChanges(rhi);

		for (ER_SceneObject& obPair : aScene->objects)
		{
			ER_RenderingObject* aObj = obPair.second;

			if (!aObj->IsInstanced())
				return; //not interested in non-instanced objects

			rhi->SetShaderResources(ER_COMPUTE, { aObj->GetIndirectOriginalInstanceBuffer() }, 0, mIndirectCullingRS, GPU_CULL_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, true);
			
			rhi->SetUnorderedAccessResources(ER_COMPUTE, { aObj->GetIndirectAppendInstanceBuffer() }, 0, mIndirectCullingRS, GPU_CULL_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX, true);

			for (int i = 0; i < aObj->GetMeshCount(); i++)
			{
				rhi->SetUnorderedAccessResources(ER_COMPUTE, { aObj->GetIndirectArgsBuffer(i) }, 1, mIndirectCullingRS, GPU_CULL_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX, true);

				mMeshConstantBuffer.Data.IndexCount_StartIndexLoc_BaseVtxLoc_StartInstLoc = XMINT4(aObj->GetIndexCount(0, i), 0, 0, 0);
				mMeshConstantBuffer.ApplyChanges(rhi);
				rhi->SetConstantBuffers(ER_COMPUTE, { mMeshConstantBuffer.Buffer(), mCameraConstantBuffer.Buffer() },
					0, mIndirectCullingRS, GPU_CULL_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, true);

				rhi->Dispatch(ER_DivideByMultiple(static_cast<UINT>(aObj->GetInstanceCount()), 64u), 1u, 1u);
				mIndirectCullsCounterPerFrame++;
			}

		}
		rhi->UnsetPSO();
		rhi->UnbindResourcesFromShader(ER_COMPUTE);
	}

}