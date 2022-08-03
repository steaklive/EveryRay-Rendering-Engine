#pragma once

#include "Common.h"
#include "RHI/ER_RHI.h"

struct aiMesh;

namespace EveryRay_Core
{
	class ER_Model;
	class ER_ModelMaterial;

	class ER_Mesh
	{
	public:
		ER_Mesh(ER_Model& model, ER_ModelMaterial& material, aiMesh& mesh);
		~ER_Mesh();

		ER_Model& GetModel();
		const ER_ModelMaterial& GetMaterial() const;
		const std::string& Name() const;

		const std::vector<XMFLOAT3>& Vertices() const;
		const std::vector<XMFLOAT3>& Normals() const;
		const std::vector<XMFLOAT3>& Tangents() const;
		const std::vector<XMFLOAT3>& BiNormals() const;
		const std::vector<std::vector<XMFLOAT3>>& TextureCoordinates() const;
		const std::vector<std::vector<XMFLOAT4>>& VertexColors() const;
		const std::vector<UINT>& Indices() const;
		UINT FaceCount() const;

		void CreateIndexBuffer(ER_RHI_GPUBuffer* indexBuffer) const;

		void CreateVertexBuffer_Position(ER_RHI_GPUBuffer* vertexBuffer) const;
		void CreateVertexBuffer_PositionUv(ER_RHI_GPUBuffer* vertexBuffer, int uvChannel = 0) const;
		void CreateVertexBuffer_PositionUvNormal(ER_RHI_GPUBuffer* vertexBuffer, int uvChannel = 0) const;
		void CreateVertexBuffer_PositionUvNormalTangent(ER_RHI_GPUBuffer* vertexBuffer, int uvChannel = 0) const;

	private:
		ER_Model& mModel;
		ER_ModelMaterial& mMaterial;
		std::string mName;
		std::vector<XMFLOAT3> mVertices;
		std::vector<XMFLOAT3> mNormals;
		std::vector<XMFLOAT3> mTangents;
		std::vector<XMFLOAT3> mBiNormals;
		std::vector<std::vector<XMFLOAT3>> mTextureCoordinates;
		std::vector<std::vector<XMFLOAT4>> mVertexColors;
		UINT mFaceCount;
		std::vector<UINT> mIndices;
	};
}