//

#include "ER_DebugProxyObject.h"
#include "ER_Core.h"
#include "ER_CoreTime.h"
#include "ER_CoreException.h"
#include "ER_MatrixHelper.h"
#include "ER_VectorHelper.h"
#include "ER_Model.h"
#include "ER_Mesh.h"
#include "ER_Utility.h"
#include "ER_Camera.h"
#include "ER_BasicColorMaterial.h"
#include "ER_MaterialsCallbacks.h"

namespace EveryRay_Core
{
	static const std::string psoName = "ER_RHI_GPUPipelineStateObject: BasicColorMaterial";

	ER_DebugProxyObject::ER_DebugProxyObject(ER_Core& game, ER_Camera& camera, const std::string& modelFileName, float scale)
		:
		mCore(game),
		mModelFileName(modelFileName), mMaterial(nullptr),
		mVertexBuffer(nullptr), mIndexBuffer(nullptr), mIndexCount(0),
		mWorldMatrix(ER_MatrixHelper::Identity), mScaleMatrix(ER_MatrixHelper::Identity),
		mPosition(ER_Vector3Helper::Zero), mDirection(ER_Vector3Helper::Forward), mUp(ER_Vector3Helper::Up), mRight(ER_Vector3Helper::Right)
	{
		XMStoreFloat4x4(&mScaleMatrix, XMMatrixScaling(scale, scale, scale));
	}

	ER_DebugProxyObject::~ER_DebugProxyObject()
	{
		DeleteObject(mMaterial);
		DeleteObject(mVertexBuffer);
		DeleteObject(mIndexBuffer);
	}

	const XMFLOAT3& ER_DebugProxyObject::Position() const
	{
		return mPosition;
	}

	const XMFLOAT3& ER_DebugProxyObject::Direction() const
	{
		return mDirection;
	}

	const XMFLOAT3& ER_DebugProxyObject::Up() const
	{
		return mUp;
	}

	const XMFLOAT3& ER_DebugProxyObject::Right() const
	{
		return mRight;
	}

	XMVECTOR ER_DebugProxyObject::PositionVector() const
	{
		return XMLoadFloat3(&mPosition);
	}

	XMVECTOR ER_DebugProxyObject::DirectionVector() const
	{
		return XMLoadFloat3(&mDirection);
	}

	XMVECTOR ER_DebugProxyObject::UpVector() const
	{
		return XMLoadFloat3(&mUp);
	}

	XMVECTOR ER_DebugProxyObject::RightVector() const
	{
		return XMLoadFloat3(&mRight);
	}

	void ER_DebugProxyObject::SetPosition(FLOAT x, FLOAT y, FLOAT z)
	{
		XMVECTOR position = XMVectorSet(x, y, z, 1.0f);
		SetPosition(position);
	}

	void ER_DebugProxyObject::SetPosition(FXMVECTOR position)
	{
		XMStoreFloat3(&mPosition, position);
	}

	void ER_DebugProxyObject::SetPosition(const XMFLOAT3& position)
	{
		mPosition = position;
	}

	void ER_DebugProxyObject::SetColor(const XMFLOAT4& aColor)
	{
		mColor = aColor;
	}

	void ER_DebugProxyObject::ApplyRotation(CXMMATRIX transform)
	{
		XMVECTOR direction = XMLoadFloat3(&mDirection);
		XMVECTOR up = XMLoadFloat3(&mUp);

		direction = XMVector3TransformNormal(direction, transform);
		direction = XMVector3Normalize(direction);

		up = XMVector3TransformNormal(up, transform);
		up = XMVector3Normalize(up);

		XMVECTOR right = XMVector3Cross(direction, up);
		up = XMVector3Cross(right, direction);

		XMStoreFloat3(&mDirection, direction);
		XMStoreFloat3(&mUp, up);
		XMStoreFloat3(&mRight, right);
	}
	void ER_DebugProxyObject::ApplyTransform(XMMATRIX transformMatrix)
	{
		XMVECTOR direction = XMVECTOR{ 0.0f, 0.0, -1.0f };
		XMVECTOR up = XMVECTOR{ 0.0f, 1.0, 0.0f };

		direction = XMVector3TransformNormal(direction, transformMatrix);
		direction = XMVector3Normalize(direction);

		up = XMVector3TransformNormal(up, transformMatrix);
		up = XMVector3Normalize(up);

		XMVECTOR right = XMVector3Cross(direction, up);
		up = XMVector3Cross(right, direction);

		XMStoreFloat3(&mDirection, direction);
		XMStoreFloat3(&mUp, up);
		XMStoreFloat3(&mRight, right);
	}
	void ER_DebugProxyObject::ApplyRotation(const XMFLOAT4X4& transform)
	{
		XMMATRIX transformMatrix = XMLoadFloat4x4(&transform);
		ApplyRotation(transformMatrix);
	}

	void ER_DebugProxyObject::ApplyRotaitonAroundPoint(float radius, float angle)
	{
		XMMATRIX rotation = XMMatrixRotationX(angle);
		XMMATRIX translate = XMMatrixTranslation(0.0f, 0.0f, radius);
		XMMATRIX matFinal = rotation * translate;
	}

	void ER_DebugProxyObject::Initialize()
	{
		ER_Model* model = mCore.AddOrGet3DModelFromCache(mModelFileName, nullptr, false);
		if (!model)
			return;

		mMaterial = new ER_BasicColorMaterial(mCore, {}, HAS_VERTEX_SHADER | HAS_PIXEL_SHADER);

		auto rhi = mCore.GetRHI();
		auto& meshes = model->Meshes();
		mVertexBuffer = rhi->CreateGPUBuffer("ER_RHI_GPUBuffer: Debug Proxy Object - Vertex Buffer");
		mMaterial->CreateVertexBuffer(meshes[0], mVertexBuffer);
		mIndexBuffer = rhi->CreateGPUBuffer("ER_RHI_GPUBuffer: Debug Proxy Object - Index Buffer");
		meshes[0].CreateIndexBuffer(mIndexBuffer);
		mIndexCount = static_cast<UINT>(meshes[0].Indices().size());
	}

	void ER_DebugProxyObject::Update(const ER_CoreTime& gameTime)
	{
		XMMATRIX worldMatrix = XMMatrixIdentity();
		ER_MatrixHelper::SetForward(worldMatrix, mDirection);
		ER_MatrixHelper::SetUp(worldMatrix, mUp);
		ER_MatrixHelper::SetRight(worldMatrix, mRight);
		ER_MatrixHelper::SetTranslation(worldMatrix, mPosition);
		XMStoreFloat4x4(&mWorldMatrix, XMLoadFloat4x4(&mScaleMatrix) * worldMatrix);
	}

	void ER_DebugProxyObject::Draw(ER_RHI_GPUTexture* aRenderTarget, ER_RHI_GPUTexture* aDepth, const ER_CoreTime& gametime, ER_RHI_GPURootSignature* rs)
	{
		auto rhi = mCore.GetRHI();

		rhi->SetVertexBuffers({ mVertexBuffer });
		rhi->SetIndexBuffer({ mIndexBuffer });

		if (!rhi->IsPSOReady(psoName))
		{
			rhi->InitializePSO(psoName);
			mMaterial->PrepareShaders();
			rhi->SetRenderTargetFormats({ aRenderTarget }, aDepth);
			rhi->SetRasterizerState(ER_NO_CULLING);
			rhi->SetBlendState(ER_NO_BLEND);
			rhi->SetDepthStencilState(ER_DEPTH_ONLY_WRITE_COMPARISON_LESS_EQUAL);
			rhi->SetTopologyTypeToPSO(psoName, ER_RHI_PRIMITIVE_TYPE::ER_PRIMITIVE_TOPOLOGY_LINELIST);
			rhi->SetRootSignatureToPSO(psoName, rs);
			rhi->FinalizePSO(psoName);
		}
		rhi->SetPSO(psoName);
		mMaterial->PrepareForRendering(XMLoadFloat4x4(&mWorldMatrix), mColor, rs);
		rhi->DrawIndexed(mIndexCount);
		rhi->UnsetPSO();
	}
}