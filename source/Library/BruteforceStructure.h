#pragma once

#include "Common.h"
#include "SpatialElement.h"

namespace Library
{
	class BruteforceStructure
	{
	public:
		BruteforceStructure();
		~BruteforceStructure();

		void AddObjects(std::vector<SpatialElement*>& objects);
		void Update();

		bool& CheckPairs() { return mShouldCheckPairs; };

	private:
		std::vector<SpatialElement*> mObjects;
		bool mShouldCheckPairs = true;
	};
}