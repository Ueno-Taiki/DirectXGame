#pragma once
#include "DirectXCommon.h"

//スプライト共通部
class SpriteCommon
{
public:// メンバ関数
	//初期化
	void Initialize(DirectXCommon* dxCommon);

	//共通描画設定
	void DrawCommon();

	DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:
	//ルートシグネチャの作成
	void RootSignature();
	//グラフィックパイプラインの生成
	void GraphicsPipeline();

	DirectXCommon* dxCommon_;
};

