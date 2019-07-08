#pragma once
#include "Common.h"

namespace Library
{
	class MatrixHelper
	{
	public:
		static const XMFLOAT4X4 Identity;
		static const XMFLOAT4X4 Zero;

		static void GetForward(CXMMATRIX matrix, XMFLOAT3 &vector);
		static void GetUp(CXMMATRIX matrix, XMFLOAT3 &vector);
		static void GetRight(CXMMATRIX matrix, XMFLOAT3 &vector);
		static void GetTranslation(CXMMATRIX matrix, XMFLOAT3 &vector);

		static void SetForward(XMMATRIX& matrix, XMFLOAT3 &forward);
		static void SetUp(XMMATRIX& matrix, XMFLOAT3 &up);
		static void SetRight(XMMATRIX& matrix, XMFLOAT3 &right);
		static void SetTranslation(XMMATRIX& matrix, XMFLOAT3 &translation);
		static std::vector<XMFLOAT3> GetRows(XMFLOAT4X4& mat);
		static std::vector<XMFLOAT3>& GetRows(XMMATRIX& pMat);
		//static XMMATRIX LookAtTransform(const XMFLOAT3 & eye, const XMFLOAT3 & at, const XMFLOAT3 & up);

	private:
		MatrixHelper();
		MatrixHelper(const MatrixHelper& rhs);
		MatrixHelper& operator=(const MatrixHelper& rhs);
	};
}