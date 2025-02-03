#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <dxcapi.h>
#include <array>
#include <chrono>
#include "WinApp.h"
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

class DirectXCommon
{
public: //メンバ変数
	//初期化
	void Initialize(WinApp* winApp);

	//描画前処理
	void PreDraw();

	//描画後処理
	void PostDraw();

	//終了処理
	void Finalize();

	//getter
	Microsoft::WRL::ComPtr<ID3D12Device> GetDevice() const { return device.Get(); }
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> GetCommandList() const { return commandList.Get(); }

	//SRVの指定番号のCPUデスクリプタハンドルを取得する
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index);

	//SRVの指定番号のGPUデスクリプタハンドルを取得する
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index);

	//シェーダーのコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const std::wstring& filePath, const wchar_t* profile);

	//バッファリソースの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

	//テクスチャリソースの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata);

	//テクスチャデータの転送
	void UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages);

	//テクスチャファイルの読み込み
	static DirectX::ScratchImage LoadTexture(const std::string& filePath);

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

	//FPS固定初期化
	void InitializeFixFPS();

	//FPS固定更新
	void UpdateFixFPS();

	//デスクリプタヒープの生成
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

	//CPUデスクリプタハンドルを取得する
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);

	//GPUデスクリプタハンドルを取得する
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);

	/// <summary>
	/// メンバ変数
	/// </summary>
	
	//WindowsAPI
	WinApp* winApp = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;

	//DXGIファクトリ
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory = nullptr;
	//DirectX12デバイス
	Microsoft::WRL::ComPtr<ID3D12Device> device = nullptr;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = nullptr;

	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};

	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource = nullptr;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap = nullptr;
	uint32_t descriptorSizeSRV;
	uint32_t descriptorSizeRTV;
	uint32_t descriptorSizeDSV;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};

	//スワップチェーンリソース
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> swapChainResources;
	//RTVを2つ作るのでディスクリプタを2つ用意
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2] = {};

	Microsoft::WRL::ComPtr<ID3D12Fence> fence = nullptr;

	//ビューボート
	D3D12_VIEWPORT viewport{};

	//シザー矩形
	D3D12_RECT scissorRect{};

	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils = nullptr;
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler = nullptr;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler = nullptr;

	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};

	//フェンス値
	UINT64 fenceValue = 0;

	//フェンスイベント
	HANDLE fenceEvent;

	//記録時間
	std::chrono::steady_clock::time_point reference_;
};

