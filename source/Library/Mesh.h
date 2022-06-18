#pragma once

#include "Common.h"

struct aiMesh;

namespace Library
{
	class Model;
	class ModelMaterial;

	class Mesh
	{
	public:
		Mesh(Model& model, ModelMaterial& material, aiMesh& mesh);
		~Mesh();

		Model& GetModel();
		const ModelMaterial& GetMaterial() const;
		const std::string& Name() const;

		const std::vector<XMFLOAT3>& Vertices() const;
		const std::vector<XMFLOAT3>& Normals() const;
		const std::vector<XMFLOAT3>& Tangents() const;
		const std::vector<XMFLOAT3>& BiNormals() const;
		const std::vector<std::vector<XMFLOAT3>>& TextureCoordinates() const;
		const std::vector<std::vector<XMFLOAT4>>& VertexColors() const;
		const std::vector<UINT>& Indices() const;
		UINT FaceCount() const;

		void CreateIndexBuffer(ID3D11Buffer** indexBuffer) const;

		void CreateVertexBuffer_Position(ID3D11Buffer** vertexBuffer) const;
		void CreateVertexBuffer_PositionUv(ID3D11Buffer** vertexBuffer, int uvChannel = 0) const;
		void CreateVertexBuffer_PositionUvNormal(ID3D11Buffer** vertexBuffer, int uvChannel = 0) const;
		void CreateVertexBuffer_PositionUvNormalTangent(ID3D11Buffer** vertexBuffer, int uvChannel = 0) const;

	private:
		Model& mModel;
		ModelMaterial& mMaterial;
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