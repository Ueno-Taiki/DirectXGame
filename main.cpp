#include <Windows.h>
#include <cstdint>

//ウインドウブロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	//メッセージに応じてゲーム固有の処理を行う
	switch (msg)
	{
		//ウインドウが破壊された
	case WM_DESTROY:
		//OSに対して、アプリの終了を伝える
		PostQuitMessage(0);
		return 0;
	}

	//標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

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
	const int32_t kclientHeight = 720;

	//ウインドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = { 0, 0, kClientWidth, kclientHeight };

	//クライアント領域を元に実際のサイズにwrcを変更してもらう
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	//ウインドウの生成
	HWND hwnd = CreateWindow(
		wc.lpszClassName,   //利用するクラス化
		L"CG2",   //タイトルバーの文字
		WS_EX_OVERLAPPEDWINDOW,   //よく見るウインドウスタイル
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

	MSG msg{};
	//ウインドウのxボタンが押されるまでループ
	while (msg.message != WM_QUIT) {
		//WINdowにメッセージが来てたら最優先で処理させる
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {

		}
	}

	return 0;
}