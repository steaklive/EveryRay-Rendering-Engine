#include "stdafx.h"
#include "MatrixHelper.h"

namespace Library
{
	const XMFLOAT4X4 MatrixHelper::Identity = XMFLOAT4X4
	   (1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);

	const XMFLOAT4X4 MatrixHelper::Zero = XMFLOAT4X4
	   (0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f);

	void MatrixHelper::GetForward(CXMMATRIX matrix, XMFLOAT3 &vector)
	{
		XMFLOAT4 m3;
		XMStoreFloat4(&m3, matrix.r[2]);

		vector.x = -m3.x;
		vector.y = -m3.y;
		vector.z = -m3.z;
	}

	void MatrixHelper::GetUp(CXMMATRIX matrix, XMFLOAT3 &vector)
	{
		XMFLOAT4 m2;
		XMStoreFloat4(&m2, matrix.r[1]);

		vector.x = m2.x;
		vector.y = m2.y;
		vector.z = m2.z;
	}

	void MatrixHelper::GetRight(CXMMATRIX matrix, XMFLOAT3 &vector)
	{
		XMFLOAT4 m1;
		XMStoreFloat4(&m1, matrix.r[0]);

		vector.x = m1.x;
		vector.y = m1.y;
		vector.z = m1.z;
	}

	void MatrixHelper::GetTranslation(CXMMATRIX matrix, XMFLOAT3 &vector)
	{
		XMFLOAT4 m4;
		XMStoreFloat4(&m4, matrix.r[3]);

		vector.x = m4.x;
		vector.y = m4.y;
		vector.z = m4.z;
	}

	void MatrixHelper::SetForward(XMMATRIX& matrix, XMFLOAT3 &forward)
	{
		XMFLOAT4 m3;
		XMStoreFloat4(&m3, matrix.r[2]);

		m3.x = -forward.x;
		m3.y = -forward.y;
		m3.z = -forward.z;

		matrix.r[2] = XMLoadFloat4(&m3);
	}

	void MatrixHelper::SetUp(XMMATRIX& matrix, XMFLOAT3 &up)
	{
		XMFLOAT4 m2;
		XMStoreFloat4(&m2, matrix.r[1]);

		m2.x = up.x;
		m2.y = up.y;
		m2.z = up.z;

		matrix.r[1] = XMLoadFloat4(&m2);
	}

	void MatrixHelper::SetRight(XMMATRIX& matrix, XMFLOAT3 &right)
	{
		XMFLOAT4 m1;
		XMStoreFloat4(&m1, matrix.r[0]);

		m1.x = right.x;
		m1.y = right.y;
		m1.z = right.z;

		matrix.r[0] = XMLoadFloat4(&m1);
	}

	void MatrixHelper::SetTranslation(XMMATRIX& matrix, XMFLOAT3 &translation)
	{
		XMFLOAT4 m4;
		XMStoreFloat4(&m4, matrix.r[3]);

		m4.x = translation.x;
		m4.y = translation.y;
		m4.z = translation.z;

		matrix.r[3] = XMLoadFloat4(&m4);
	}

	std::vector<XMFLOAT3> MatrixHelper::GetRows(XMFLOAT4X4& mat)
	{

		std::vector<XMFLOAT3> rowVector;
		rowVector.push_back(XMFLOAT3(mat._11, mat._12, mat._13));
		rowVector.push_back(XMFLOAT3(mat._21, mat._22, mat._23));
		rowVector.push_back(XMFLOAT3(mat._31, mat._32, mat._33));

		return rowVector;
	}
	std::vector<XMFLOAT3>& MatrixHelper::GetRows(XMMATRIX& pMat)
	{

		XMFLOAT4X4 mat;
		XMStoreFloat4x4(&mat, pMat);

		pMat.r[0];
		std::vector<XMFLOAT3> rowVector;

		rowVector.push_back(XMFLOAT3(mat._11, mat._12, mat._13));
		rowVector.push_back(XMFLOAT3(mat._21, mat._22, mat._23));
		rowVector.push_back(XMFLOAT3(mat._31, mat._32, mat._33));

		return rowVector;
	}

	void MatrixHelper::GetFloatArray(XMMATRIX & pMat, float* matrixArray)
	{
		XMFLOAT4X4 mat;
		XMStoreFloat4x4(&mat, pMat);

		matrixArray[0] = mat._11;
		matrixArray[1] = mat._12;
		matrixArray[2] = mat._13;
		matrixArray[3] = mat._14;
		matrixArray[4] = mat._21;
		matrixArray[5] = mat._22;
		matrixArray[6] = mat._23;
		matrixArray[7] = mat._24;
		matrixArray[8] = mat._31;
		matrixArray[9] = mat._32;
		matrixArray[10] = mat._33;
		matrixArray[11] = mat._34;
		matrixArray[12] = mat._41;
		matrixArray[13] = mat._42;
		matrixArray[14] = mat._43;
		matrixArray[15] = mat._44;

	}


	
}