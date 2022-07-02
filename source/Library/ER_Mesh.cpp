#include "stdafx.h"

#include "ER_Mesh.h"
#include "ER_Model.h"
#include "ER_ModelMaterial.h"
#include "Game.h"
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

	void ER_Mesh::CreateIndexBuffer(ID3D11Buffer** indexBuffer) const
	{
		assert(indexBuffer != nullptr);

		D3D11_BUFFER_DESC indexBufferDesc;
		ZeroMemory(&indexBufferDesc, sizeof(indexBufferDesc));
		indexBufferDesc.ByteWidth = sizeof(UINT) * mIndices.size();
		indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

		D3D11_SUBRESOURCE_DATA indexSubResourceData;
		ZeroMemory(&indexSubResourceData, sizeof(indexSubResourceData));
		indexSubResourceData.pSysMem = &mIndices[0];
		if (FAILED(mModel.GetGame().Direct3DDevice()->CreateBuffer(&indexBufferDesc, &indexSubResourceData, indexBuffer)))
		{
			throw ER_CoreException("ID3D11Device::CreateBuffer() failed during the creation of the index buffer in ER_Mesh.");
		}
	}

	void ER_Mesh::CreateVertexBuffer_Position(ID3D11Buffer** vertexBuffer) const
	{
		const std::vector<XMFLOAT3>& sourceVertices = Vertices();
		std::vector<VertexPosition> vertices;
		vertices.reserve(sourceVertices.size());

		for (UINT i = 0; i < sourceVertices.size(); i++)
		{
			XMFLOAT3 position = sourceVertices.at(i);
			vertices.push_back(VertexPosition(XMFLOAT4(position.x, position.y, position.z, 1.0f)));
		}

		ID3D11Device* device = mModel.GetGame().Direct3DDevice();

		D3D11_BUFFER_DESC vertexBufferDesc;
		ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
		vertexBufferDesc.ByteWidth = sizeof(VertexPosition) * static_cast<UINT>(vertices.size());
		vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA vertexSubResourceData;
		ZeroMemory(&vertexSubResourceData, sizeof(vertexSubResourceData));
		vertexSubResourceData.pSysMem = &vertices[0];
		if (FAILED(device->CreateBuffer(&vertexBufferDesc, &vertexSubResourceData, vertexBuffer)))
			throw ER_CoreException("ID3D11Device::CreateBuffer() failed during vertex buffer creation for CreateVertexBuffer_Position.");

	}

	void ER_Mesh::CreateVertexBuffer_PositionUv(ID3D11Buffer** vertexBuffer, int uvChannel) const
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

		ID3D11Device* device = mModel.GetGame().Direct3DDevice();

		D3D11_BUFFER_DESC vertexBufferDesc;
		ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
		vertexBufferDesc.ByteWidth = sizeof(VertexPositionTexture) * static_cast<UINT>(vertices.size());
		vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA vertexSubResourceData;
		ZeroMemory(&vertexSubResourceData, sizeof(vertexSubResourceData));
		vertexSubResourceData.pSysMem = &vertices[0];
		if (FAILED(device->CreateBuffer(&vertexBufferDesc, &vertexSubResourceData, vertexBuffer)))
			throw ER_CoreException("ID3D11Device::CreateBuffer() failed.");
	}

	void ER_Mesh::CreateVertexBuffer_PositionUvNormal(ID3D11Buffer** vertexBuffer, int uvChannel) const
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

		ID3D11Device* device = mModel.GetGame().Direct3DDevice();

		D3D11_BUFFER_DESC vertexBufferDesc;
		ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
		vertexBufferDesc.ByteWidth = sizeof(VertexPositionTextureNormal) * static_cast<UINT>(vertices.size());
		vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA vertexSubResourceData;
		ZeroMemory(&vertexSubResourceData, sizeof(vertexSubResourceData));
		vertexSubResourceData.pSysMem = &vertices[0];
		if (FAILED(device->CreateBuffer(&vertexBufferDesc, &vertexSubResourceData, vertexBuffer)))
			throw ER_CoreException("ID3D11Device::CreateBuffer() failed during vertex buffer creation for CreateVertexBuffer_PositionUvNormal.");
	}

	void ER_Mesh::CreateVertexBuffer_PositionUvNormalTangent(ID3D11Buffer** vertexBuffer, int uvChannel) const
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

		ID3D11Device* device = mModel.GetGame().Direct3DDevice();

		D3D11_BUFFER_DESC vertexBufferDesc;
		ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
		vertexBufferDesc.ByteWidth = sizeof(VertexPositionTextureNormalTangent) * static_cast<UINT>(vertices.size());
		vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA vertexSubResourceData;
		ZeroMemory(&vertexSubResourceData, sizeof(vertexSubResourceData));
		vertexSubResourceData.pSysMem = &vertices[0];
		if (FAILED(device->CreateBuffer(&vertexBufferDesc, &vertexSubResourceData, vertexBuffer)))
			throw ER_CoreException("ID3D11Device::CreateBuffer() failed during vertex buffer creation for CreateVertexBuffer_PositionUvNormalTangent.");

	}

}