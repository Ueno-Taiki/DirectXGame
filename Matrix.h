#pragma once

//4×4の行列を表す
struct Matrix4x4
{
	float m[4][4];
};

//3次元ベクトルを表す
struct Vector3 {
	float x;
	float y;
	float z;
};

static const int kRowHeight = 20;
static const int kColumnWidth = 60;

//3次元アフィン変換
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

//逆行列
Matrix4x4 Inverse(const Matrix4x4& m);

//透視投影行列
Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);

//平行射影行列
Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);

//行列の積
Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);

//単位行列の作成
Matrix4x4 MakeIdentityx4x4();