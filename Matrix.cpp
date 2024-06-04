#include <cmath>
#include "Matrix.h"

Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate)
{
	Matrix4x4 result = { 0 };

	result.m[0][0] = scale.x * (std::cos(rotate.y) * std::cos(rotate.z));
	result.m[0][1] = scale.x * (std::cos(rotate.y) * std::sin(rotate.z));
	result.m[0][2] = scale.x * -std::sin(rotate.y);
	result.m[0][3] = 0;

	result.m[1][0] = scale.y * (std::sin(rotate.x) * std::sin(rotate.y) * std::cos(rotate.z) - std::cos(rotate.x) * std::sin(rotate.z));
	result.m[1][1] = scale.y * (std::sin(rotate.x) * std::sin(rotate.y) * std::sin(rotate.z) + std::cos(rotate.x) * std::cos(rotate.z));
	result.m[1][2] = scale.y * (std::cos(rotate.x) * std::cos(rotate.y));
	result.m[1][3] = 0;

	result.m[2][0] = scale.z * (std::cos(rotate.x) * std::sin(rotate.y) * std::cos(rotate.z) + std::sin(rotate.x) * std::sin(rotate.z));
	result.m[2][1] = scale.z * (std::cos(rotate.x) * std::sin(rotate.y) * std::sin(rotate.z) - std::sin(rotate.x) * std::cos(rotate.z));
	result.m[2][2] = scale.z * (std::cos(rotate.x) * std::cos(rotate.y));
	result.m[2][3] = 0;

	result.m[3][0] = translate.x;
	result.m[3][1] = translate.y;
	result.m[3][2] = translate.z;
	result.m[3][3] = 1;

	return result;
}

Matrix4x4 MakeIdentityx4x4()
{
	Matrix4x4 result = { 0 };

	result.m[0][0] = 1;
	result.m[0][1] = 0;
	result.m[0][2] = 0;
	result.m[0][3] = 0;

	result.m[1][0] = 0;
	result.m[1][1] = 1;
	result.m[1][2] = 0;
	result.m[1][3] = 0;

	result.m[2][0] = 0;
	result.m[2][1] = 0;
	result.m[2][2] = 1;
	result.m[2][3] = 0;

	result.m[3][0] = 0;
	result.m[3][1] = 0;
	result.m[3][2] = 0;
	result.m[3][3] = 1;

	return result;
}