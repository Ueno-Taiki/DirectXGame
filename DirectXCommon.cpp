#include <cassert>
#include <format>
#include <thread>
#include "DirectXCommon.h"
#include "Logger.h"
#include "StringUtility.h"
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using namespace Microsoft::WRL;

void DirectXCommon::Initialize(WinApp* winApp)
{
	//NULL検出
	assert(winApp);

	//メンバ変数に記録
	this->winApp = winApp;

	//FPS固定初期化
	InitializeFixFPS();

	//デバイスの初期化
	Device();
	//コマンド関連の初期化
	CommandQueue();
	//スワップチェーンの生成
	SwapChain();
	//深度バッファの生成
	DepthStencil();
	//各種デスクリプタヒープの生成
	DescriptorHeap();
	//レンダーターゲットビューの初期化
	RenderTargetView();
	//深度ステンシルビューの初期化
	DepthStenvcilView();
	//フェンスの生成
	Fence();
	//ビューポート矩形の初期化
	Viewport();
	//シザリンク矩形
	ScissorRect();
	//DXCコンパイラの生成
	DxcCompiler();
	//ImGuiの初期化
	ImGui();
}

//描画前処理
void DirectXCommon::PreDraw()
{
	//バックバッファのインデックスを取得
	UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

	//TransitionBarrierの設定
	D3D12_RESOURCE_BARRIER barrier{};
	//今回のバリアはTransition
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	//Noneにしておく
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	//バリアを張る対象のリソース。
	barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
	//遷移前のResourceState
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	//遷移後のResourceState
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	//TransitionBarrierを張る
	commandList->ResourceBarrier(1, &barrier);

	//描画先のRTVとDSVを設定する
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);

	//指定した色で画面全体をクリアする
	float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };   //青っぽい色。
	commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

	//指定した深度で画面全体をクリアする
	commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	//描画用のDescriptorHeapの設定
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeaps[] = { srvDescriptorHeap.Get()};
	commandList->SetDescriptorHeaps(1, descriptorHeaps->GetAddressOf());

	//Viewportの設定
	commandList->RSSetViewports(1, &viewport);

	//Scirssorを設定
	commandList->RSSetScissorRects(1, &scissorRect);
}

//描画後処理
void DirectXCommon::PostDraw()
{
	HRESULT hr;

	//バックバッファのインデックスを取得
	UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

	//TransitionBarrierの設定
	D3D12_RESOURCE_BARRIER barrier{};
	//今回のバリアはTransition
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	//Noneにしておく
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	//バリアを張る対象のリソース。
	barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
	//状態を遷移
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	//TransitionBarrierをはる
	commandList->ResourceBarrier(1, &barrier);

	//コマンドリストの内容を確定させる。
	hr = commandList->Close();
	assert(SUCCEEDED(hr));

	//FPS固定
	UpdateFixFPS();

	//GPUにコマンドリストの実行を行わせる
	Microsoft::WRL::ComPtr<ID3D12CommandList> commandLists[] = { commandList.Get()};
	commandQueue->ExecuteCommandLists(1, commandLists->GetAddressOf());

	//GPUとOSに画面の交換を行うよう通知する
	swapChain->Present(1, 0);

	//Fenceの値を更新
	fenceValue++;

	//GPUがここまでたどり着いたときに、Fenceの値を指定した値を代入するようにSignalを送る
	commandQueue->Signal(fence.Get(), fenceValue);
	//GetCompletedValueの初期化はFence作成時に渡した初期値
	if (fence->GetCompletedValue() < fenceValue)
	{
		//FenceのSignalを待つためのイベントを作成する
		assert(fenceEvent != nullptr);
		//指定したSignalにたどりついていないので、たどり着くまで待つようにイベントを設定する
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		//イベントを待つ
		WaitForSingleObject(fenceEvent, INFINITE);
	}
	//次のフレーム用のコマンドリストを準備
	hr = commandAllocator->Reset();
	assert(SUCCEEDED(hr));

	hr = commandList->Reset(commandAllocator.Get(), nullptr);
	assert(SUCCEEDED(hr));
}

//終了処理
void DirectXCommon::Finalize()
{
	CloseHandle(fenceEvent);
}

//デバイスの初期化
void DirectXCommon::Device()
{
#ifdef _DEBUG
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		//デバックレイヤーを有効化する
		debugController->EnableDebugLayer();
		//さらにGPU側でもチェックを行うようにする
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif

	//DXGIファクトリーの生成
	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
	assert(SUCCEEDED(hr));

	//使用するアダプタ用の変数。
	Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter = nullptr;
	//良い順にアダプタを頼む
	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i,
		DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) !=
		DXGI_ERROR_NOT_FOUND; ++i) {
		//アダプターの情報を取得する
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter.Get()->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));
		//ソフトウェアアダプタでなければ採用
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			//採用したアダプタの情報をログに出力。
			Logger::Log(StringUtility::ConvertString(std::format(L"USE Adapater:{}\n", adapterDesc.Description)));
			break;
		}
		useAdapter = nullptr;
	}
	//適正なアダプタが見つからなかったので起動出来ない
	assert(useAdapter != nullptr);

	//機能レベルとログ出力用の文字列
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0
	};
	const char* featureLevelStrings[] = { "12.2", "12.1", "12.0" };
	//高い順に生成できるか試していく
	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		//採用したアダプターでデバイスを生成
		hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&device));
		//指定した機能レベルでデバイスが生成できたかを確認
		if (SUCCEEDED(hr)) {
			//生成できたのでログ出力を行ってループを抜ける
			Logger::Log(std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
			break;
		}
	}
	assert(device != nullptr);
	Logger::Log("Complete create D3D12Device!!!\n");

#ifdef _DEBUG
	ID3D12InfoQueue* infoQueue = nullptr;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
		//ヤバイエラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		//エラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		//警告時に止まる
		//infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
		//解放
		infoQueue->Release();
		//抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] = {
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};
		//抑制するレベル
		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		//指定したメッセージの表示を制御
		infoQueue->PushStorageFilter(&filter);
	}
#endif 
}

//コマンド関連の初期化
void DirectXCommon::CommandQueue()
{
	HRESULT hr;

	//コマンドキューを生成する
	D3D12_COMMAND_QUEUE_DESC commandQueueDesu{};
	hr = device->CreateCommandQueue(&commandQueueDesu, IID_PPV_ARGS(&commandQueue));
	//コマンドキューの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	//コマンドアロケータを生成する
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
	//コマンドアロケータの生成がうまくいかなかったので移動できない
	assert(SUCCEEDED(hr));

	//コマンドリストを生成する
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr,
		IID_PPV_ARGS(&commandList));
	//コマンドリストの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));
}

//スワップチェーンの生成
void DirectXCommon::SwapChain()
{
	HRESULT hr;

	//スワップチェーンを生成する
	swapChainDesc.Width = WinApp::kClientWidth;   //画面の幅
	swapChainDesc.Height = WinApp::kClientHeight;   //画面の高さ。
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;   //色の形式
	swapChainDesc.SampleDesc.Count = 1;   //マルチサンプルしない
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;   //描画のターゲットとして利用する
	swapChainDesc.BufferCount = 2;   //ダブルバッファ
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;   //モニタうつしたら、中身を破棄
	//コマンドキュー、ウインドウハンドル、設定を渡して生成する
	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue.Get(), winApp->GetHwnd(), &swapChainDesc, nullptr, nullptr,
		reinterpret_cast<IDXGISwapChain1**>(swapChain.GetAddressOf()));

	assert(SUCCEEDED(hr));
}

//深度バッファの生成
void DirectXCommon::DepthStencil()
{
	//生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = WinApp::kClientWidth;  //Textureの幅
	resourceDesc.Height = WinApp::kClientHeight;  //Textureの高さ
	resourceDesc.MipLevels = 1;  //mipmapの数
	resourceDesc.DepthOrArraySize = 1;  //奥行き or 配列Textureの配列数
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;  //DepthStencilとして利用可能なフォーマット
	resourceDesc.SampleDesc.Count = 1;  //サンプリングカウント。
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;  //2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;  //DepthStencilとして使う通知

	//利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;  //VRAM上に作る

	//深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;  //1.0fでクリア
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;  //フォーマット

	//Resourceの生成
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,  //Heapの設定
		D3D12_HEAP_FLAG_NONE,  //Heapの特殊な設定。
		&resourceDesc,  //Resourceの設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE,  //深度値を書き込む状態にしておく
		&depthClearValue,  //Clear最適地
		IID_PPV_ARGS(&depthStencilResource));  //作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));
}

//各種デスクリプタヒープの生成
void DirectXCommon::DescriptorHeap()
{
	//RTV用のDescriptorSizeを取得しておく
	descriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	//SRV用のDescriptorSizeを取得しておく
	descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//DSV用のDescriptorSizeを取得しておく
	descriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	//RTV用のヒープでディスクリプタの数は2。
	rtvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
	//SRV用のヒープでディスクリプタの数は128。
	srvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);
	//DSV用のヒープでディスクリプタの数は1。
	dsvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
}

//レンダーターゲットビューの初期化
void DirectXCommon::RenderTargetView()
{
	HRESULT hr;

	//SwapChainからResourceを引っ張ってくる
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
	//うまく取得できなければ起動できない
	assert(SUCCEEDED(hr));
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	assert(SUCCEEDED(hr));

	//RTVの設定
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;   //出力結果をSRGBに変換して書き込む
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;   //2dテクスチャとして書き込む
	//ディスクリプタの先頭を取得する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	//まず1つ目を作る。
	rtvHandles[0] = rtvStartHandle;
	device->CreateRenderTargetView(swapChainResources[0].Get(), &rtvDesc, rtvHandles[0]);
	//2つ目のディスクリプタハンドルを得る
	rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	//2つ目を作る
	device->CreateRenderTargetView(swapChainResources[1].Get(), &rtvDesc, rtvHandles[1]);
}

//深度ステンシルビューの初期化
void DirectXCommon::DepthStenvcilView()
{
	//DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;  //Format。
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;  //2dTexture

	//DSVHeapの先頭にDSVを作る
	device->CreateDepthStencilView(depthStencilResource.Get(), &dsvDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
}

//フェンスの生成
void DirectXCommon::Fence()
{
	HRESULT hr;

	//初期値0でFenceを作る
	uint64_t fenceValue = 0;
	hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));
}

//ビューポート矩形の初期化
void DirectXCommon::Viewport()
{
	//クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = WinApp::kClientWidth;
	viewport.Height = WinApp::kClientHeight;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}

//シザリング矩形の初期化
void DirectXCommon::ScissorRect()
{
	//基本的にビューボートと同じ矩形が構成されるようにする
	scissorRect.left = 0;
	scissorRect.right = WinApp::kClientWidth;
	scissorRect.top = 0;
	scissorRect.bottom = WinApp::kClientHeight;
}

//DXCコンパイラの生成
void DirectXCommon::DxcCompiler()
{
	HRESULT hr;

	//dxcCompilerを初期化
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));

	//現時点でincludeはしないが、includeに対応するための設定を行っておく
	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));
}

//ImGuiの初期化
void DirectXCommon::ImGui()
{
	//ImGuiの初期化。
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(winApp->GetHwnd());
	ImGui_ImplDX12_Init(device.Get(),
		swapChainDesc.BufferCount,
		rtvDesc.Format,
		srvDescriptorHeap.Get(),
		srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
}

//FPS固定初期化
void DirectXCommon::InitializeFixFPS()
{
	//現在時間を記録する
	reference_ = std::chrono::steady_clock::now();
}

//FPS固定更新
void DirectXCommon::UpdateFixFPS()
{
	//1/60秒ぴったりの時間
	const std::chrono::microseconds kMinTime(uint64_t(1000000.0f / 60.0f));
	//1/60秒よりわずかに短い時間
	const std::chrono::microseconds kMinCheckTime(uint64_t(1000000.0f / 65.0f));

	//現在時間
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	//前回記録からの経過時間を取得する
	std::chrono::microseconds elapsed =
		std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

	//1/60秒(よりわずかに短い時間)経っていない場合
	if (elapsed < kMinTime) {
		while (std::chrono::steady_clock::now() - reference_ < kMinTime) {
			//1マイクロ秒スリープ
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
	}
	//現在の時間を記録する
	reference_ = std::chrono::steady_clock::now();
}

//デスクリプタヒープの生成
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DirectXCommon::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible)
{
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));
	return descriptorHeap;
}

//シェーダーのコンパイル
Microsoft::WRL::ComPtr<IDxcBlob> DirectXCommon::CompileShader(const std::wstring& filePath, const wchar_t* profile)
{
	//1.hlslファイルを読み込む
	//これからシェーダーをコンパイルする旨をログに出す
	Logger::Log(StringUtility::ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile)));
	//hlslファイルを読み込む
	Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	//読めなかったら止める
	assert(SUCCEEDED(hr));
	//読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer = {};
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;  //UTF8の文字コードであることを通知

	//2.Compileする
	LPCWSTR arguments[] = {
		filePath.c_str(),
		L"-E", L"main",  //コンパイル対象のhlslファイル名
		L"-T", profile,  //ShaderProfileの設定
		L"-Zi", L"-Qembed_debug",  //デバック用の情報を埋め込む
		L"-Od",  //最適解を外しておく
		L"-Zpr",  //メモリレイアウトは行優先
	};
	//実際にshaderをコンパイルする
	Microsoft::WRL::ComPtr<IDxcResult> shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer,  //読み込んだファイル
		arguments,  //コンパイルオプション
		_countof(arguments),  //コンパイルオプションの数
		includeHandler.Get(),  //includeが含まれた諸々
		IID_PPV_ARGS(&shaderResult)  //コンパイル結果
	);
	//コンパイルエラーではなくdxcが起動できないなど致命的な状況
	assert(SUCCEEDED(hr));

	//3.警告・エラーがでていないか確認する
	Microsoft::WRL::ComPtr<IDxcBlobUtf8> shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Logger::Log(shaderError->GetStringPointer());
		assert(false);
	}

	//4.Compile結果を受け取って返す
	//コンパイル結果から実行用のバイナリ部分を取得
	Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));
	//成功したログを出す
	Logger::Log(StringUtility::ConvertString(std::format(L"Begin CompileShader, path:{}, profile : {}\n", filePath, profile)));
	//実行用のバイナリを返却
	return shaderBlob;
}

//バッファリソースの生成
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t sizeInBytes)
{
	//頂点リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;  //UploadHeapを使う
	//頂点リソースの設定
	D3D12_RESOURCE_DESC vertexResourceDesc{};
	//バッファリソース
	vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexResourceDesc.Width = sizeInBytes;  //リソースのサイズ
	//バッファの場合はこれらは1にする決まり
	vertexResourceDesc.Height = 1;
	vertexResourceDesc.DepthOrArraySize = 1;
	vertexResourceDesc.MipLevels = 1;
	vertexResourceDesc.SampleDesc.Count = 1;
	//バッファの場合はこれにする決まり
	vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	//実際に頂点リソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
		&vertexResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));
	return resource;
}

//テクスチャリソースの生成
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata)
{
	//1.metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);  //Textureの幅
	resourceDesc.Height = UINT(metadata.height);  //Textureの高さ
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);  //mipmapの数
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);  //奥行き or 配列Textureの配列数
	resourceDesc.Format = metadata.format;  //TextureのFormat
	resourceDesc.SampleDesc.Count = 1;  //サンプリングカウント。1固定。
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);  //Textureの次元数。
	//2.利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;  //細かい設定を行う
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;  //WriteBackポリシーでCPUアクセス可能
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;  //プロセッサの近くに配置
	//3.Resourceを生成する
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,  //Heapの設定
		D3D12_HEAP_FLAG_NONE,  //Heapの特殊な設定。
		&resourceDesc,  //Resourceの設定
		D3D12_RESOURCE_STATE_GENERIC_READ,  //初回のResourceState。
		nullptr,  //Clear最適地。
		IID_PPV_ARGS(&resource));  //作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));
	return resource;
}

//テクスチャデータの転送
void DirectXCommon::UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages)
{
	//Meta情報を取得
	const DirectX::TexMetadata metadata = mipImages.GetMetadata();
	//全MipMapを取得
	for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel) {
		//MipMapLevelを指定して各Imageを取得
		const DirectX::Image* img = mipImages.GetImage(mipLevel, 0, 0);
		//Textureに転移
		HRESULT hr = texture->WriteToSubresource(
			UINT(mipLevel),
			nullptr,  //全領域へコピー
			img->pixels,  //元データアドレス
			UINT(img->rowPitch),  //1ラインサイズ
			UINT(img->slicePitch)  //1枚サイズ
		);
	}
}

//テクスチャファイルの読み込み
DirectX::ScratchImage DirectXCommon::LoadTexture(const std::string& filePath)
{
	//テクスチャファイルを読んでプログラムで扱えるようにする
	DirectX::ScratchImage image{};
	std::wstring filePathW = StringUtility::ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	//ミニマップの作成
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	//ミニマップ付きのデータを渡す
	return mipImages;
}

//CPUデスクリプタハンドルを取得する
D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

//GPUデスクリプタハンドルを取得する
D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}

//SRVの指定番号のCPUデスクリプタハンドルを取得する
D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVCPUDescriptorHandle(uint32_t index)
{
	return GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, index);
}

//SRVの指定番号のGPUデスクリプタハンドルを取得する
D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVGPUDescriptorHandle(uint32_t index)
{
	return GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, index);
}
