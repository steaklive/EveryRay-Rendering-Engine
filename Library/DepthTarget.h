#pragma once
#include "Common.h"

class DepthTarget
{
public:
	static DepthTarget* Create(ID3D11Device * device, UINT width, UINT height, UINT samples, DXGI_FORMAT format, uint32_t slices = 1);
	
	DepthTarget(ID3D11Resource * resource, ID3D11ShaderResourceView * srv, ID3D11DepthStencilView * dsv, ID3D11DepthStencilView * readonly_dsv, ID3D11UnorderedAccessView * uav);
	~DepthTarget();

	ID3D11ShaderResourceView * getSRV() { return mSRV; }

	ID3D11DepthStencilView * getDSV() { return mDSV; }
	ID3D11DepthStencilView * getReadOnlyDSV() { return mReadonlyDSV; }

protected:
	DepthTarget() {};
	ID3D11DepthStencilView* mDSV;
	ID3D11DepthStencilView* mReadonlyDSV;
	ID3D11ShaderResourceView* mSRV;

};