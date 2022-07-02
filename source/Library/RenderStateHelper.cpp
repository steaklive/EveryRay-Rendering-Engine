#include "stdafx.h"

#include "RenderStateHelper.h"
#include "ER_Core.h"

namespace Library
{
	RenderStateHelper::RenderStateHelper(ER_Core& game)
		: mCore(game), mRasterizerState(nullptr), mBlendState(nullptr), mBlendFactor(new FLOAT[4]), mSampleMask(UINT_MAX), mDepthStencilState(nullptr), mStencilRef(UINT_MAX)
	{
	}

	RenderStateHelper::~RenderStateHelper()
	{
		ReleaseObject(mRasterizerState);
		ReleaseObject(mBlendState);
		ReleaseObject(mDepthStencilState);
		DeleteObjects(mBlendFactor);
	}

	void RenderStateHelper::ResetAll(ID3D11DeviceContext* deviceContext)
	{
		deviceContext->RSSetState(nullptr);
		deviceContext->OMSetBlendState(nullptr, nullptr, UINT_MAX);
		deviceContext->OMSetDepthStencilState(nullptr, UINT_MAX);
	}

	void RenderStateHelper::SaveRasterizerState()
	{
		ReleaseObject(mRasterizerState);
		mCore.Direct3DDeviceContext()->RSGetState(&mRasterizerState);
	}

	void RenderStateHelper::RestoreRasterizerState() const
	{
		mCore.Direct3DDeviceContext()->RSSetState(mRasterizerState);
	}

	void RenderStateHelper::SaveBlendState()
	{
		ReleaseObject(mBlendState);
		mCore.Direct3DDeviceContext()->OMGetBlendState(&mBlendState, mBlendFactor, &mSampleMask);
	}

	void RenderStateHelper::RestoreBlendState() const
	{
		mCore.Direct3DDeviceContext()->OMSetBlendState(mBlendState, mBlendFactor, mSampleMask);
	}

	void RenderStateHelper::SaveDepthStencilState()
	{
		ReleaseObject(mDepthStencilState);
		mCore.Direct3DDeviceContext()->OMGetDepthStencilState(&mDepthStencilState, &mStencilRef);
	}

	void RenderStateHelper::RestoreDepthStencilState() const
	{
		mCore.Direct3DDeviceContext()->OMSetDepthStencilState(mDepthStencilState, mStencilRef);
	}

	void RenderStateHelper::SaveAll()
	{
		SaveRasterizerState();
		SaveBlendState();
		SaveDepthStencilState();
	}

	void RenderStateHelper::RestoreAll() const
	{
		RestoreRasterizerState();
		RestoreBlendState();
		RestoreDepthStencilState();
	}
}