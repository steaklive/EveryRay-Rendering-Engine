// Simple "generic" event system in EveryRay Rendering Engine
// works with named/anonymous listeners

#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

template<typename T>
class GeneralEvent
{
public:
	void AddListener(const std::string& pName, T pEventHandlerMethod)
	{
		if (mNamedListeners.find(pName) == mNamedListeners.end())
			mNamedListeners[pName] = pEventHandlerMethod;
	}
	void AddListener(T pEventHandlerMethod)
	{
		mAnonymousListeners.push_back(pEventHandlerMethod);
	}
	void RemoverListener(const std::string& pName)
	{
		if (mNamedListeners.find(pName) != mNamedListeners.end())
			mNamedListeners.erase(pName);
	}
	void RemoverAllListeners()
	{
		mNamedListeners.clear();
		mAnonymousListeners.clear();
	}

	std::vector<T> GetListeners()
	{
		std::vector<T> allListeners;
		for (auto listener : mNamedListeners)
		{
			allListeners.push_back(listener.second);
		}
		allListeners.insert(allListeners.end(), mAnonymousListeners.begin(), mAnonymousListeners.end());
		return allListeners;
	}


private:
	std::unordered_map<std::string, T> mNamedListeners;
	std::vector<T> mAnonymousListeners;
};