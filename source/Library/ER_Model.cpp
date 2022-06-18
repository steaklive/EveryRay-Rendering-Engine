#include "stdafx.h"

#include "ER_Model.h"
#include "ER_Mesh.h"
#include "ER_ModelMaterial.h"
#include "Game.h"
#include "GameException.h"

#include "assimp\Importer.hpp"
#include "assimp\scene.h"
#include "assimp\postprocess.h"

namespace Library
{
	ER_Model::ER_Model(Game& game, const std::string& filename, bool flipUVs)
		: mGame(game), mMeshes(), mMaterials()
	{
		Assimp::Importer importer;

		UINT flags = aiProcess_Triangulate /*| aiProcess_JoinIdenticalVertices*/ | aiProcess_SortByPType | aiProcess_FlipWindingOrder;
		if (flipUVs)
		{
			flags |= aiProcess_FlipUVs;
		}

		const aiScene* scene = importer.ReadFile(filename, flags);
		
		if (scene == nullptr)
		{
			throw GameException(importer.GetErrorString());
		}

		if (scene->HasMaterials())
		{
			for (UINT i = 0; i < scene->mNumMaterials; i++)
				mMaterials.push_back(ER_ModelMaterial(*this, scene->mMaterials[i]));
		}

		if (scene->HasMeshes())
		{
			for (UINT i = 0; i < scene->mNumMeshes; i++)
				mMeshes.push_back(ER_Mesh(*this, mMaterials[scene->mMeshes[i]->mMaterialIndex], *(scene->mMeshes[i])));
		}

		mFilename = filename;
	}

	ER_Model::~ER_Model()
	{
	}

	Game& ER_Model::GetGame()
	{
		return mGame;
	}

	bool ER_Model::HasMeshes() const
	{
		return (mMeshes.size() > 0);
	}

	bool ER_Model::HasMaterials() const
	{
		return (mMaterials.size() > 0);
	}

	const std::vector<ER_Mesh>& ER_Model::Meshes() const
	{
		return mMeshes;
	}

	const ER_Mesh& ER_Model::GetMesh(int index) const
	{
		return mMeshes.at(index);
	}

	const std::vector<ER_ModelMaterial>& ER_Model::Materials() const
	{
		return mMaterials;
	}

	const ER_AABB& ER_Model::GenerateAABB()
	{
		std::vector<XMFLOAT3> vertices;

		for (ER_Mesh& mesh : mMeshes)
			vertices.insert(vertices.end(), mesh.Vertices().begin(), mesh.Vertices().end());

		XMFLOAT3 minVertex = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
		XMFLOAT3 maxVertex = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		for (UINT i = 0; i < vertices.size(); i++)
		{
			//Get the smallest vertex 
			minVertex.x = std::min(minVertex.x, vertices[i].x);    // Find smallest x value in model
			minVertex.y = std::min(minVertex.y, vertices[i].y);    // Find smallest y value in model
			minVertex.z = std::min(minVertex.z, vertices[i].z);    // Find smallest z value in model

			//Get the largest vertex 
			maxVertex.x = std::max(maxVertex.x, vertices[i].x);    // Find largest x value in model
			maxVertex.y = std::max(maxVertex.y, vertices[i].y);    // Find largest y value in model
			maxVertex.z = std::max(maxVertex.z, vertices[i].z);    // Find largest z value in model
		}

		mAABB = { minVertex, maxVertex };
		return mAABB;
	}
}