#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <dxcapi.h>
#include <array>
#include "WinApp.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

class DirectXCommon
{
public: //メンバ変数
	//初期化
	void Initialize();

	//SRVの指定番号のCPUデスクリプタハンドルを取得する
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index);

	//RTVの指定番号のCPUデスクリプタハンドルを取得する
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTVCPUDescriptorHandle(uint32_t index);

	//DSVの指定番号のCPUデスクリプタハンドルを取得する
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVCPUDescriptorHandle(uint32_t index);

	//SRVの指定番号のGPUデスクリプタハンドルを取得する
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index);
	
	//RTVの指定番号のGPUデスクリプタハンドルを取得する
	D3D12_GPU_DESCRIPTOR_HANDLE GetRTVGPUDescriptorHandle(uint32_t index);

	//DSVの指定番号のGPUデスクリプタハンドルを取得する
	D3D12_GPU_DESCRIPTOR_HANDLE GetDSVGPUDescriptorHandle(uint32_t index);

private:
	//デバイスの初期化
	void Device();

	//コマンド関連の初期化
	void CommandQueue();

	//スワップチェーンの生成
	void SwapChain();

	//深度バッファの生成
	void DepthStencil();

	//各種デスクリプタヒープの生成
	void DescriptorHeap();

	//レンダーターゲットビューの初期化
	void RenderTargetView();

	//深度ステンシルビューの初期化
	void DepthStenvcilView();

	//フェンスの生成
	void Fence();

	//ビューポート矩形の初期化
	void Viewport();

	//シザリング矩形の初期化
	void ScissorRect();

	//DXCコンパイラの生成
	void DxcCompiler();

	//ImGuiの初期化
	void ImGui();

	//デスクリプタヒープの生成
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

	//CPUデスクリプタハンドルを取得する
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);

	//GPUデスクリプタハンドルを取得する
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);

	/// <summary>
	/// メンバ変数
	/// </summary>
	ID3D12Debug1* debugController = nullptr;

	IDXGIFactory7* dxgiFactory = nullptr;
	ID3D12Device* device = nullptr;

	ID3D12CommandQueue* commandQueue = nullptr;
	ID3D12CommandAllocator* commandAllocator = nullptr;
	ID3D12GraphicsCommandList* commandList = nullptr;

	IDXGISwapChain4* swapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};

	ID3D12Resource* depthStencilResource = nullptr;

	ID3D12DescriptorHeap* descriptorHeap = nullptr;

	ID3D12DescriptorHeap* rtvDescriptorHeap = nullptr;
	ID3D12DescriptorHeap* srvDescriptorHeap = nullptr;
	ID3D12DescriptorHeap* dsvDescriptorHeap = nullptr;
	uint32_t descriptorSizeSRV;
	uint32_t descriptorSizeRTV;
	uint32_t descriptorSizeDSV;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};

	//スワップチェーンリソース
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> swapChainResources;

	ID3D12Fence* fence = nullptr;

	//ビューボート
	D3D12_VIEWPORT viewport{};

	//シザー矩形
	D3D12_RECT scissorRect{};

	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;
	IDxcIncludeHandler* includeHandler = nullptr;

	//WindowsAPI
	WinApp* winApp = nullptr;

	//DirectX12デバイス
	Microsoft::WRL::ComPtr<ID3D12Device> device;
	//DXGIファクトリ
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory;
};

