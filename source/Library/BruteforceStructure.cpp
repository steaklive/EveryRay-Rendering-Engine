#include "BruteforceStructure.h"


namespace Library
{
	BruteforceStructure::BruteforceStructure()
	{

	}

	BruteforceStructure::~BruteforceStructure()
	{

	}

	void BruteforceStructure::AddObjects(std::vector<SpatialElement*>& objects)
	{
		std::vector<SpatialElement*>::const_iterator iter_object;
		for (iter_object = objects.begin(); iter_object != objects.end(); iter_object++)
		{
			mObjects.push_back(*iter_object);
		}
	}


	void BruteforceStructure::Update()
	{
		std::vector<SpatialElement*>::iterator iter_object1;
		std::vector<SpatialElement*>::iterator iter_object2;

		for (iter_object1 = mObjects.begin(); iter_object1 != mObjects.end(); iter_object1++)
		{
			for (iter_object2 = mObjects.begin(); iter_object2 != mObjects.end(); iter_object2++)
			{
				if (!mShouldCheckPairs)
				{
					if ((*iter_object1)->GetID() != (*iter_object2)->GetID()) {
						//  collision test
						if ((*iter_object1)->CheckCollision((*iter_object2)))
						{
							(*iter_object1)->IsColliding(true);
							(*iter_object2)->IsColliding(true);

						}
					}
				}
				else
				{
					if ((*iter_object1)->GetID() < (*iter_object2)->GetID())
					{
						// collision test
						if ((*iter_object1)->CheckCollision((*iter_object2)))
						{
							(*iter_object1)->IsColliding(true);
							(*iter_object2)->IsColliding(true);

						}
					}
				}
			}
		}
	}
}