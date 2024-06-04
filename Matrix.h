#pragma once

//4×4の行列を表す
struct Matrix4x4
{
	float m[4][4];
};

//2次元ベクトルを表す
struct Vector3 {
	float x;
	float y;
	float z;
};

//3次元アフィン変換
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

//単位行列の作成
Matrix4x4 MakeIdentityx4x4();