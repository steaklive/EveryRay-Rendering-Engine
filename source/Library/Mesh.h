#pragma once

#include "Common.h"
#include "Model.h"

struct aiMesh;

namespace Library
{
	class Material;
	class ModelMaterial;

	class Mesh
	{
		friend class Model;

	public:
		Mesh(Model& model, ModelMaterial* material);
		~Mesh();

		Model& GetModel();
		ModelMaterial* GetMaterial();
		const std::string& Name() const;

		const std::vector<XMFLOAT3>& Vertices() const;
		const std::vector<XMFLOAT3>& Normals() const;
		const std::vector<XMFLOAT3>& Tangents() const;
		const std::vector<XMFLOAT3>& BiNormals() const;
		const std::vector<std::vector<XMFLOAT3>*>& TextureCoordinates() const;
		const std::vector<std::vector<XMFLOAT4>*>& VertexColors() const;
		UINT FaceCount() const;
		const std::vector<UINT>& Indices() const;

		void CreateIndexBuffer(ID3D11Buffer** indexBuffer);

		void CreateVertexBuffer_Position(ID3D11Buffer** vertexBuffer);
		void CreateVertexBuffer_PositionUvNormalTangent(ID3D11Buffer** vertexBuffer);

	private:
		Mesh(Model& model, aiMesh& mesh);
		Mesh(const Mesh& rhs);
		Mesh& operator=(const Mesh& rhs);

		Model& mModel;
		ModelMaterial* mMaterial;
		std::string mName;
		std::vector<XMFLOAT3> mVertices;
		std::vector<XMFLOAT3> mNormals;
		std::vector<XMFLOAT3> mTangents;
		std::vector<XMFLOAT3> mBiNormals;
		std::vector<std::vector<XMFLOAT3>*> mTextureCoordinates;
		std::vector<std::vector<XMFLOAT4>*> mVertexColors;
		UINT mFaceCount;
		std::vector<UINT> mIndices;
	};
}