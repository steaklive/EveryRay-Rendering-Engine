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

#define GPU_CULL_CLEAR_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX 0
#define GPU_CULL_CLEAR_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX 1

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
		DeleteObject(mIndirectCullingClearCS);
		DeleteObject(mIndirectCullingClearRS);

		mMeshConstantBuffer.Release();
		mCameraConstantBuffer.Release();
	}

	void ER_GPUCuller::Initialize()
	{
		auto rhi = mCore.GetRHI();

		mIndirectCullingCS = rhi->CreateGPUShader();
		mIndirectCullingCS->CompileShader(rhi, "content\\shaders\\IndirectCulling.hlsl", "CSMain", ER_COMPUTE);
		mIndirectCullingClearCS = rhi->CreateGPUShader();
		mIndirectCullingClearCS->CompileShader(rhi, "content\\shaders\\IndirectCullingClear.hlsl", "CSMain", ER_COMPUTE);

		//root signatures
		mIndirectCullingRS = rhi->CreateRootSignature(3, 0);
		if (mIndirectCullingRS)
		{
			mIndirectCullingRS->InitDescriptorTable(rhi, GPU_CULL_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_UAV }, { 0 }, { 2 });
			mIndirectCullingRS->InitDescriptorTable(rhi, GPU_CULL_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_SRV }, { 0 }, { 1 });
			mIndirectCullingRS->InitDescriptorTable(rhi, GPU_CULL_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 2 });
			mIndirectCullingRS->Finalize(rhi, "ER_RHI_GPURootSignature: Indirect Culling Main");
		}

		mIndirectCullingClearRS = rhi->CreateRootSignature(2, 0);
		if (mIndirectCullingClearRS)
		{
			mIndirectCullingClearRS->InitDescriptorTable(rhi, GPU_CULL_CLEAR_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_UAV }, { 0 }, { 1 });
			mIndirectCullingClearRS->InitDescriptorTable(rhi, GPU_CULL_CLEAR_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, { ER_RHI_DESCRIPTOR_RANGE_TYPE::ER_RHI_DESCRIPTOR_RANGE_TYPE_CBV }, { 0 }, { 1 });
			mIndirectCullingClearRS->Finalize(rhi, "ER_RHI_GPURootSignature: Indirect Culling Clear");
		}

		//cbuffers
		mMeshConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: GPU Culler Mesh CB");
		mCameraConstantBuffer.Initialize(rhi, "ER_RHI_GPUBuffer: GPU Culler Camera CB");
	}

	void ER_GPUCuller::ClearCounters(ER_Scene* aScene)
	{
		assert(aScene);
		auto rhi = mCore.GetRHI();

		rhi->SetRootSignature(mIndirectCullingClearRS, true);
		if (!rhi->IsPSOReady(mPSOClearName, true))
		{
			rhi->InitializePSO(mPSOClearName, true);
			rhi->SetShader(mIndirectCullingClearCS);
			rhi->SetRootSignatureToPSO(mPSOClearName, mIndirectCullingClearRS, true);
			rhi->FinalizePSO(mPSOClearName, true);
		}
		rhi->SetPSO(mPSOClearName, true);

		for (ER_SceneObject& obPair : aScene->objects)
		{
			ER_RenderingObject* aObj = obPair.second;

			if (!aObj->IsGPUIndirectlyRendered())
				continue;

			if (!aObj->GetIndirectArgsBuffer() || !aObj->GetIndirectNewInstanceBuffer())
				continue;

			rhi->ClearUAV(aObj->GetIndirectNewInstanceBuffer(), 0);

			rhi->SetUnorderedAccessResources(ER_COMPUTE,
				{
					aObj->GetIndirectArgsBuffer()
				}, 0, mIndirectCullingRS, GPU_CULL_CLEAR_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX, true);

			UpdateMeshesConstantBuffer(aObj);
			rhi->SetConstantBuffers(ER_COMPUTE, { mMeshConstantBuffer.Buffer() }, 0, mIndirectCullingClearRS, GPU_CULL_CLEAR_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, true);

			rhi->Dispatch(1u, 1u, 1u);
		}
		rhi->UnsetPSO();
		rhi->UnbindResourcesFromShader(ER_COMPUTE);
	}


	void ER_GPUCuller::PerformCull(ER_Scene* aScene)
	{
		assert(aScene);

		ClearCounters(aScene);

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
		mCameraConstantBuffer.Data.LodCameraDistances = XMFLOAT4(
			ER_Utility::DistancesLOD[0] * ER_Utility::DistancesLOD[0], 
			ER_Utility::DistancesLOD[1] * ER_Utility::DistancesLOD[1],
			ER_Utility::DistancesLOD[2] * ER_Utility::DistancesLOD[2], 0.0f);
		mCameraConstantBuffer.Data.CameraPos = XMFLOAT4(mCamera.Position().x, mCamera.Position().y, mCamera.Position().z, 1.0f);
		mCameraConstantBuffer.ApplyChanges(rhi);

		for (ER_SceneObject& obPair : aScene->objects)
		{
			ER_RenderingObject* aObj = obPair.second;

			if (!aObj->IsGPUIndirectlyRendered())
				continue;

			if (!aObj->GetIndirectArgsBuffer() || !aObj->GetIndirectNewInstanceBuffer() || !aObj->GetIndirectOriginalInstanceBuffer())
				continue;

			rhi->SetShaderResources(ER_COMPUTE, { aObj->GetIndirectOriginalInstanceBuffer() }, 0, mIndirectCullingRS, GPU_CULL_PASS_ROOT_DESCRIPTOR_TABLE_SRV_INDEX, true);
			
			rhi->SetUnorderedAccessResources(ER_COMPUTE,
				{ 
					aObj->GetIndirectNewInstanceBuffer(),
					aObj->GetIndirectArgsBuffer()
				}, 0, mIndirectCullingRS, GPU_CULL_PASS_ROOT_DESCRIPTOR_TABLE_UAV_INDEX, true);

			UpdateMeshesConstantBuffer(aObj);
			rhi->SetConstantBuffers(ER_COMPUTE, { mMeshConstantBuffer.Buffer(), mCameraConstantBuffer.Buffer() },
				0, mIndirectCullingRS, GPU_CULL_PASS_ROOT_DESCRIPTOR_TABLE_CBV_INDEX, true);

			rhi->Dispatch(ER_DivideByMultiple(static_cast<UINT>(aObj->GetInstanceCount()), 64u), 1u, 1u);
			mIndirectCullsCounterPerFrame++;
		}
		rhi->UnsetPSO();
		rhi->UnbindResourcesFromShader(ER_COMPUTE);
	}

	void ER_GPUCuller::UpdateMeshesConstantBuffer(const ER_RenderingObject* aObj)
	{
		auto rhi = mCore.GetRHI();
		const int totalObjLodCount = aObj->GetLODCount();

		int offset, indexCount, lastAvailableLod = 0;
		for (int lodI = 0; lodI < MAX_LOD; lodI++)
		{
			for (int meshI = 0; meshI < MAX_MESH_COUNT; meshI++)
			{
				offset = MAX_MESH_COUNT * lodI + meshI;
				if (lodI < totalObjLodCount)
					indexCount = (meshI < aObj->GetMeshCount(/*TODO ideally from lodI but we dont support meshes per LOD yet*/)) ? aObj->GetIndexCount(lodI, meshI) : INT_MAX;
				else
					indexCount = INT_MAX;
					// Uncomment this if you want to fallback into previous LOD (you have to adjust ER_RenderingObject::Draw())
					// indexCount = (meshI < aObj->GetMeshCount(/*TODO ideally from lastAvailableLod but we dont support meshes per LOD yet*/)) ? aObj->GetIndexCount(lastAvailableLod, meshI) : INT_MAX;
				
				mMeshConstantBuffer.Data.IndexCount_StartIndexLoc_BaseVtxLoc_StartInstLoc[offset] = XMINT4(indexCount, 0, 0, 0);
			}
			if (lodI < totalObjLodCount)
				lastAvailableLod = lodI;
		}
		mMeshConstantBuffer.Data.OriginalInstancesCount = aObj->GetInstanceCount();
		mMeshConstantBuffer.ApplyChanges(rhi);
	}
}