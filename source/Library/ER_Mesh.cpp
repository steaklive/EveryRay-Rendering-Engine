#include "stdafx.h"

#include "ER_Mesh.h"
#include "ER_Model.h"
#include "ER_ModelMaterial.h"
#include "ER_Core.h"
#include "ER_CoreException.h"
#include "ER_VertexDeclarations.h"

#include "assimp\scene.h"

namespace Library
{
	ER_Mesh::ER_Mesh(ER_Model& model, ER_ModelMaterial& material, aiMesh& mesh) : mModel(model), mMaterial(material), mName(mesh.mName.C_Str()), mVertices(), mNormals(), mTangents(), mBiNormals(), mTextureCoordinates(), mVertexColors(), mFaceCount(0), mIndices()
	{
		// Vertices
		mVertices.reserve(mesh.mNumVertices);
		for (UINT i = 0; i < mesh.mNumVertices; i++)
		{
			mVertices.push_back(XMFLOAT3(reinterpret_cast<const float*>(&mesh.mVertices[i])));
		}

		// Normals
		if (mesh.HasNormals())
		{
			mNormals.reserve(mesh.mNumVertices);
			for (UINT i = 0; i < mesh.mNumVertices; i++)
			{
				mNormals.push_back(XMFLOAT3(reinterpret_cast<const float*>(&mesh.mNormals[i])));
			}
		}

		// Tangents and Binormals
		if (mesh.HasTangentsAndBitangents())
		{
			mTangents.reserve(mesh.mNumVertices);
			mBiNormals.reserve(mesh.mNumVertices);
			for (UINT i = 0; i < mesh.mNumVertices; i++)
			{
				mTangents.push_back(XMFLOAT3(reinterpret_cast<const float*>(&mesh.mTangents[i])));
				mBiNormals.push_back(XMFLOAT3(reinterpret_cast<const float*>(&mesh.mBitangents[i])));
			}
		}

		// Texture Coordinates
		UINT uvChannelCount = mesh.GetNumUVChannels();
		for (UINT i = 0; i < uvChannelCount; i++)
		{
			std::vector<XMFLOAT3> textureCoordinates;
			textureCoordinates.reserve(mesh.mNumVertices);

			aiVector3D* aiTextureCoordinates = mesh.mTextureCoords[i];
			for (UINT j = 0; j < mesh.mNumVertices; j++)
				textureCoordinates.push_back(XMFLOAT3(reinterpret_cast<const float*>(&aiTextureCoordinates[j])));

			mTextureCoordinates.push_back(textureCoordinates);
		}

		// Vertex Colors
		UINT colorChannelCount = mesh.GetNumColorChannels();
		for (UINT i = 0; i < colorChannelCount; i++)
		{
			std::vector<XMFLOAT4> vertexColors;
			vertexColors.reserve(mesh.mNumVertices);

			aiColor4D* aiVertexColors = mesh.mColors[i];
			for (UINT j = 0; j < mesh.mNumVertices; j++)
				vertexColors.push_back(XMFLOAT4(reinterpret_cast<const float*>(&aiVertexColors[j])));

			mVertexColors.push_back(vertexColors);
		}

		// Faces (note: could pre-reserve if we limit primitive types)
		if (mesh.HasFaces())
		{
			mFaceCount = mesh.mNumFaces;
			for (UINT i = 0; i < mFaceCount; i++)
			{
				aiFace* face = &mesh.mFaces[i];

				for (UINT j = 0; j < face->mNumIndices; j++)
				{
					mIndices.push_back(face->mIndices[j]);
				}
			}
		}
	}

	/*ER_Mesh::ER_Mesh(Model & model, ER_ModelMaterial * material)
	{
	}*/

	ER_Mesh::~ER_Mesh()
	{
	}

	ER_Model& ER_Mesh::GetModel()
	{
		return mModel;
	}

	const ER_ModelMaterial& ER_Mesh::GetMaterial() const
	{
		return mMaterial;
	}

	const std::string& ER_Mesh::Name() const
	{
		return mName;
	}

	const std::vector<XMFLOAT3>& ER_Mesh::Vertices() const
	{
		return mVertices;
	}

	const std::vector<XMFLOAT3>& ER_Mesh::Normals() const
	{
		return mNormals;
	}

	const std::vector<XMFLOAT3>& ER_Mesh::Tangents() const
	{
		return mTangents;
	}

	const std::vector<XMFLOAT3>& ER_Mesh::BiNormals() const
	{
		return mBiNormals;
	}

	const std::vector<std::vector<XMFLOAT3>>& ER_Mesh::TextureCoordinates() const
	{
		return mTextureCoordinates;
	}

	const std::vector<std::vector<XMFLOAT4>>& ER_Mesh::VertexColors() const
	{
		return mVertexColors;
	}

	UINT ER_Mesh::FaceCount() const
	{
		return mFaceCount;
	}

	const std::vector<UINT>& ER_Mesh::Indices() const
	{
		return mIndices;
	}

	void ER_Mesh::CreateIndexBuffer(ER_RHI_GPUBuffer* indexBuffer) const
	{
		assert(indexBuffer);
		indexBuffer->CreateGPUBufferResource(mModel.GetCore().GetRHI(), &mIndices[0], static_cast<UINT>(mIndices.size()), sizeof(UINT), false, ER_BIND_INDEX_BUFFER);
	}

	void ER_Mesh::CreateVertexBuffer_Position(ER_RHI_GPUBuffer* vertexBuffer) const
	{
		const std::vector<XMFLOAT3>& sourceVertices = Vertices();
		std::vector<VertexPosition> vertices;
		vertices.reserve(sourceVertices.size());

		for (UINT i = 0; i < sourceVertices.size(); i++)
		{
			XMFLOAT3 position = sourceVertices.at(i);
			vertices.push_back(VertexPosition(XMFLOAT4(position.x, position.y, position.z, 1.0f)));
		}

		assert(vertexBuffer);
		vertexBuffer->CreateGPUBufferResource(mModel.GetCore().GetRHI(), &vertices[0], static_cast<UINT>(vertices.size()), sizeof(VertexPosition), false, ER_BIND_VERTEX_BUFFER);
	}

	void ER_Mesh::CreateVertexBuffer_PositionUv(ER_RHI_GPUBuffer* vertexBuffer, int uvChannel) const
	{
		const std::vector<XMFLOAT3>& sourceVertices = Vertices();
		const std::vector<XMFLOAT3>& textureCoordinates = mTextureCoordinates[uvChannel];
		assert(textureCoordinates.size() == sourceVertices.size());

		std::vector<VertexPositionTexture> vertices;
		vertices.reserve(sourceVertices.size());
		for (UINT i = 0; i < sourceVertices.size(); i++)
		{
			XMFLOAT3 position = sourceVertices.at(i);
			XMFLOAT3 uv = textureCoordinates.at(i);
			vertices.push_back(VertexPositionTexture(XMFLOAT4(position.x, position.y, position.z, 1.0f), XMFLOAT2(uv.x, uv.y)));
		}

		assert(vertexBuffer);
		vertexBuffer->CreateGPUBufferResource(mModel.GetCore().GetRHI(), &vertices[0], static_cast<UINT>(vertices.size()), sizeof(VertexPositionTexture), false, ER_BIND_VERTEX_BUFFER);
	}

	void ER_Mesh::CreateVertexBuffer_PositionUvNormal(ER_RHI_GPUBuffer* vertexBuffer, int uvChannel) const
	{
		const std::vector<XMFLOAT3>& sourceVertices = Vertices();
		const std::vector<XMFLOAT3>& textureCoordinates = mTextureCoordinates[uvChannel];
		assert(textureCoordinates.size() == sourceVertices.size());

		const std::vector<XMFLOAT3>& normals = Normals();
		assert(normals.size() == sourceVertices.size());

		std::vector<VertexPositionTextureNormal> vertices;
		vertices.reserve(sourceVertices.size());

		for (UINT i = 0; i < sourceVertices.size(); i++)
		{
			XMFLOAT3 position = sourceVertices.at(i);
			XMFLOAT3 uv = textureCoordinates.at(i);
			XMFLOAT3 normal = normals.at(i);

			vertices.push_back(VertexPositionTextureNormal(XMFLOAT4(position.x, position.y, position.z, 1.0f), XMFLOAT2(uv.x, uv.y), normal));
		}

		assert(vertexBuffer);
		vertexBuffer->CreateGPUBufferResource(mModel.GetCore().GetRHI(), &vertices[0], static_cast<UINT>(vertices.size()), sizeof(VertexPositionTextureNormal), false, ER_BIND_VERTEX_BUFFER);
	}

	void ER_Mesh::CreateVertexBuffer_PositionUvNormalTangent(ER_RHI_GPUBuffer* vertexBuffer, int uvChannel) const
	{
		const std::vector<XMFLOAT3>& sourceVertices = Vertices();
		const std::vector<XMFLOAT3>& textureCoordinates = mTextureCoordinates[uvChannel];
		assert(textureCoordinates.size() == sourceVertices.size());

		const std::vector<XMFLOAT3>& normals = Normals();
		assert(normals.size() == sourceVertices.size());

		const std::vector<XMFLOAT3>& tangents = Tangents();
		assert(tangents.size() == sourceVertices.size());

		std::vector<VertexPositionTextureNormalTangent> vertices;
		vertices.reserve(sourceVertices.size());

		for (UINT i = 0; i < sourceVertices.size(); i++)
		{
			XMFLOAT3 position = sourceVertices.at(i);
			XMFLOAT3 uv = textureCoordinates.at(i);
			XMFLOAT3 normal = normals.at(i);
			XMFLOAT3 tangent = tangents.at(i);

			vertices.push_back(VertexPositionTextureNormalTangent(XMFLOAT4(position.x, position.y, position.z, 1.0f), XMFLOAT2(uv.x, uv.y), normal, tangent));
		}

		assert(vertexBuffer);
		vertexBuffer->CreateGPUBufferResource(mModel.GetCore().GetRHI(), &vertices[0], static_cast<UINT>(vertices.size()), sizeof(VertexPositionTextureNormalTangent), false, ER_BIND_VERTEX_BUFFER);
	}
}