#pragma once

#include "Common.h"

namespace Library
{
	class ER_CoreServicesContainer
	{
	public:
		ER_CoreServicesContainer();

		void AddService(UINT typeID, void* service);
		void RemoveService(UINT typeID);
		void* FindService(UINT typeID) const;

	private:
		ER_CoreServicesContainer(const ER_CoreServicesContainer& rhs);
		ER_CoreServicesContainer& operator=(const ER_CoreServicesContainer& rhs);

		std::map<UINT, void*> mServices;
	};
}