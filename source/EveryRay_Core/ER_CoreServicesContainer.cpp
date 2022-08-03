#include "stdafx.h"

#include "ER_CoreServicesContainer.h"

namespace EveryRay_Core
{
	ER_CoreServicesContainer::ER_CoreServicesContainer()
		: mServices()
	{
	}

	void ER_CoreServicesContainer::AddService(UINT typeID, void* service)
	{
		mServices.insert(std::pair<UINT, void*>(typeID, service));
	}

	void ER_CoreServicesContainer::RemoveService(UINT typeID)
	{
		mServices.erase(typeID);
	}

	void* ER_CoreServicesContainer::FindService(UINT typeID) const
	{
		std::map<UINT, void*>::const_iterator it = mServices.find(typeID);

		return (it != mServices.end() ? it->second : nullptr);
	}
}