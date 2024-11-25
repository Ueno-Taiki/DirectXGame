#include "DirectXCommon.h"

void DirectXCommon::Initialize()
{
	//DXGIファクトリーの生成
	IDXGIFactory7* dxgiFactory = nullptr;
	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
	assert(SUCCEEDED(hr));
}
