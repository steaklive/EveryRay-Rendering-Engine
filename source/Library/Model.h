#pragma once

#include "Common.h"

namespace Library
{
	class Game;
	class Mesh;
	class ModelMaterial;

	class Model
	{
	public:
		Model(Game& game, const std::string& filename, bool flipUVs = false);
		~Model();

		Game& GetGame();
		bool HasMeshes() const;
		bool HasMaterials() const;

		const std::vector<Mesh>& Meshes() const;
		const Mesh& GetMesh(int index) const;
		const std::vector<ModelMaterial>& Materials() const;
		const std::string& GetFileName() { return mFilename; }
		const char* GetFileNameChar() { return mFilename.c_str(); }
		const ER_AABB& GenerateAABB();

	private:
		Model(const Model& rhs);
		Model& operator=(const Model& rhs);

		Game& mGame;
		ER_AABB mAABB;
		std::vector<Mesh> mMeshes;
		std::vector<ModelMaterial> mMaterials;
		std::string mFilename;
	};
}