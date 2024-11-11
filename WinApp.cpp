#include "WinApp.h"

void WinApp::Initialize()
{
	WNDCLASS wc{};
	//ウインドウブロシージャ
	wc.lpfnWndProc = WindowProc;
	//ウインドウクラス名
	wc.lpszClassName = L"CG2WindowClass";
	//インスタンスハンドル
	wc.hInstance = GetModuleHandle(nullptr);
	//カーソル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	//ウインドウクラスを登録する
	RegisterClass(&wc);

	//クライアント領域のサイズ
	const int32_t kClientWidth = 1280;
	const int32_t kClientHeight = 720;

	//ウインドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = { 0, 0, kClientWidth, kClientHeight };

	//クライアント領域を元に実際のサイズにwrcを変更してもらう
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	//ウインドウの生成
	HWND hwnd = CreateWindow(
		wc.lpszClassName,   //利用するクラス化
		L"CG2",   //タイトルバーの文字
		WS_OVERLAPPEDWINDOW,   //よく見るウインドウスタイル
		CW_USEDEFAULT,   //表示X座標
		CW_USEDEFAULT,   //表示Y座標
		wrc.right - wrc.left,   //ウインドウ横幅
		wrc.bottom - wrc.top,   //ウインドウ縦幅
		nullptr,   //親ウインドウハンドル
		nullptr,   //メニューハンドル
		wc.hInstance,   //インスタンスハンドル
		nullptr);   //オプション

	//ウインドウを表示する
	ShowWindow(hwnd, SW_SHOW);
}

void WinApp::Update()
{

}
