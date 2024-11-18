#pragma once
#include <Windows.h>
#include <wrl.h>
#define DIRECTINPUT_VERSION  0x0800
#include <dinput.h>
#include "WinApp.h"

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

	bool PushKey(BYTE keyNumber) const;

	bool TriggerKey(BYTE keyNumber) const;

public: //メンバ変数
	//初期化
	void Initialize(WinApp* winApp);
	//更新
	void Update();

private: //メンバ関数
	//キーボードのデバイス
	ComPtr<IDirectInputDevice8> keyboard;
	//windowsAPI
	WinApp* winApp = nullptr;
};

