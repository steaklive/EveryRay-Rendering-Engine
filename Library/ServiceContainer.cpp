#include "stdafx.h"

#include "ServiceContainer.h"

namespace Library
{
	ServiceContainer::ServiceContainer()
		: mServices()
	{
	}

	void ServiceContainer::AddService(UINT typeID, void* service)
	{
		mServices.insert(std::pair<UINT, void*>(typeID, service));
	}

	void ServiceContainer::RemoveService(UINT typeID)
	{
		mServices.erase(typeID);
	}

	void* ServiceContainer::GetService(UINT typeID) const
	{
		std::map<UINT, void*>::const_iterator it = mServices.find(typeID);

		return (it != mServices.end() ? it->second : nullptr);
	}
}