#pragma once
#include "Common.h"

namespace Library
{
	class ER_Core;

	class RenderStateHelper
	{
	public:
		RenderStateHelper(ER_Core& game);
		~RenderStateHelper();

		static void ResetAll(ID3D11DeviceContext* deviceContext);

		ID3D11RasterizerState* RasterizerState();
		ID3D11BlendState* BlendState();
		ID3D11DepthStencilState* DepthStencilState();

		void SaveRasterizerState();
		void RestoreRasterizerState() const;

		void SaveBlendState();
		void RestoreBlendState() const;

		void SaveDepthStencilState();
		void RestoreDepthStencilState() const;

		void SaveAll();
		void RestoreAll() const;

	private:
		RenderStateHelper(const RenderStateHelper& rhs);
		RenderStateHelper& operator=(const RenderStateHelper& rhs);

		ER_Core& mCore;

		ID3D11RasterizerState* mRasterizerState;
		ID3D11BlendState* mBlendState;
		FLOAT* mBlendFactor;
		UINT mSampleMask;
		ID3D11DepthStencilState* mDepthStencilState;
		UINT mStencilRef;
	};
}