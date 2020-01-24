#pragma once
#include <string>

namespace Library
{
	class RTTI
	{
	public:
		virtual const unsigned int& TypeIdInstance() const = 0;

		virtual RTTI* QueryInterface(const unsigned id) const
		{
			return nullptr;
		}

		virtual bool Is(const unsigned int id) const
		{
			return false;
		}

		virtual bool Is(const std::string& name) const
		{
			return false;
		}

		template <typename T>
		T* As() const
		{
			if (Is(T::TypeIdClass()))
			{
				return (T*)this;
			}

			return nullptr;
		}
	};

#define RTTI_DECLARATIONS(Type, ParentType)                                                                  \
        public:                                                                                              \
            typedef ParentType Parent;                                                                       \
            static std::string TypeName() { return std::string(#Type); }                                     \
            virtual const unsigned int& TypeIdInstance() const { return Type::TypeIdClass(); }               \
            static  const unsigned int& TypeIdClass() { return sRunTimeTypeId; }                             \
            virtual Library::RTTI* QueryInterface( const unsigned int id ) const                             \
            {                                                                                                \
                if (id == sRunTimeTypeId)                                                                    \
                    { return (RTTI*)this; }                                                                  \
                else                                                                                         \
                    { return Parent::QueryInterface(id); }                                                   \
            }                                                                                                \
            virtual bool Is(const unsigned int id) const                                                     \
            {                                                                                                \
                if (id == sRunTimeTypeId)                                                                    \
                    { return true; }                                                                         \
                else                                                                                         \
                    { return Parent::Is(id); }                                                               \
            }                                                                                                \
            virtual bool Is(const std::string& name) const                                                   \
            {                                                                                                \
                if (name == TypeName())                                                                      \
                    { return true; }                                                                         \
                else                                                                                         \
                    { return Parent::Is(name); }                                                             \
            }                                                                                                \
	   private:                                                                                              \
            static unsigned int sRunTimeTypeId;

#define RTTI_DEFINITIONS(Type) unsigned int Type::sRunTimeTypeId = (unsigned int)& Type::sRunTimeTypeId;
}