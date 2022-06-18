#pragma once

#include "Common.h"

namespace Library
{
	class Game;
	class ER_Mesh;
	class ER_ModelMaterial;

	class ER_Model
	{
	public:
		ER_Model(Game& game, const std::string& filename, bool flipUVs = false);
		~ER_Model();

		Game& GetGame();
		bool HasMeshes() const;
		bool HasMaterials() const;

		const std::vector<ER_Mesh>& Meshes() const;
		const ER_Mesh& GetMesh(int index) const;
		const std::vector<ER_ModelMaterial>& Materials() const;
		const std::string& GetFileName() { return mFilename; }
		const char* GetFileNameChar() { return mFilename.c_str(); }
		const ER_AABB& GenerateAABB();

	private:
		ER_Model(const ER_Model& rhs);
		ER_Model& operator=(const ER_Model& rhs);

		Game& mGame;
		ER_AABB mAABB;
		std::vector<ER_Mesh> mMeshes;
		std::vector<ER_ModelMaterial> mMaterials;
		std::string mFilename;
	};
}