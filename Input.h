#pragma once
#include <Windows.h>
#include <wrl.h>
#define DIRECTINPUT_VERSION  0x0800
#include <dinput.h>

//入力
class Input
{
public:
	//全キーの状態
	BYTE key[256] = {};
	//前回の前キーの状態
	BYTE keyPre[256] = {};

	//namespace省略
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

	//DirextInputのインスタンス
	ComPtr<IDirectInput8> directInput;

	bool PushKey(BYTE keyNumber);

	bool TriggerKey(BYTE keyNumber);

public: //メンバ変数
	//初期化
	void Initialize(HINSTANCE hInstance, HWND hwnd);
	//更新
	void Update();

private: //メンバ関数
	//キーボードのデバイス
	ComPtr<IDirectInputDevice8> keyboard;
};

