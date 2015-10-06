///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The Pakal Engine Open Source Project
// File: RTTI.h
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: 01-11-2012
//
// Purpose: provide runtime type info
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "Config.h"

namespace Pakal
{

	class _PAKALExport RTTI
	{
	public:
		explicit RTTI(const char* name) : m_name(name), m_parent(nullptr){}
		explicit RTTI(const char* name, const RTTI& rtti) : m_name(name), m_parent(&rtti){}

		inline const char * getName() const { return m_name; }

		inline bool isType(const RTTI &rtti) const { return &rtti == this; }

		template <class T>
		bool isType() const
		{
			ASSERT_MSG(T::__BaseClass::is_RTTI_valid<T>(), "Missing RTTI for T");
			return isType(T::getRTTI());
		}
		template <class T>
		bool isType(T*) const
		{			
			return isType<T>();
		}

		template <class T>
		bool isDerivedFrom() const
		{
			ASSERT_MSG(T::__BaseClass::is_RTTI_valid<T>(), "Missing RTTI for T");
			return isDerivedFrom(T::getRTTI());
		}
		template <class T>
		bool isDerivedFrom(T *) const
		{			
			return isDerivedFrom<T>();
		}

		friend bool operator==(const RTTI &left, const RTTI &right);
		
	private:

		bool isDerivedFrom(const RTTI &from) const
		{
			const RTTI *base = this;
			while (base != nullptr)
			{
				if (base == &from)
					return true;
				base = base->m_parent;
			}
			return false;
		}

		const char *m_name;		
		const RTTI *m_parent;

		void operator=(const RTTI &other) = delete;
		RTTI(const RTTI &other) = delete;
	};

	inline bool operator==(const RTTI &left, const RTTI &right)
	{
		return &left == &right;
	}

	namespace TypeInfo
	{
		template<class T>
		static const RTTI& get( T * obj)
		{			
			ASSERT(obj != nullptr);
			ASSERT(obj->is_RTTI_valid<T>());
			static_assert(  !(std::is_same< T::__BaseClass, T>::value) , "RTTI missing");
			return T::getRTTI();
		}

		template<class T>
		static const RTTI& get()
		{
			ASSERT(T::__BaseClass::is_RTTI_valid<T>());
			return T::getRTTI();
		}

		template <class T, class U>
		static bool is_derived()
		{
			ASSERT( !(T::getRTTI() == U::getRTTI()) );

			static_assert(!(std::is_same< T, U>::value), "RTTI missing");

			return T::getRTTI().isDerivedFrom<U>();
		}

		template <class T, class U>
		static bool is_derived(U *obj)
		{
			ASSERT(obj);
			static_assert(!(std::is_same< T, U>::value), "RTTI missing");
			return obj->getRTTI().isDerivedFrom<T>();
		}
	};
}

#define DECLARE_RTTI(x)  \
	using __BaseClass = x;\
	static const Pakal::RTTI &getRTTI()\
	{\
		static Pakal::RTTI s_RTTI_Info(#x);\
		return s_RTTI_Info;\
	}\
	template <class BaseClass>\
	static const bool is_RTTI_valid()\
	{\
		return &BaseClass::getRTTI() != &getRTTI();\
	}\
	virtual const Pakal::RTTI &getType()\
	{\
		return x::getRTTI();\
	}
	
#define DECLARE_RTTI_WITH_BASE(x,base)  \
	static const Pakal::RTTI &getRTTI()\
	{\
		/*static x *_##x = 0;*/\
		static Pakal::RTTI s_RTTI_Info(#x,base::getRTTI());\
		return s_RTTI_Info;\
	}\
	virtual const Pakal::RTTI &getType() override\
	{\
		return x::getRTTI();\
	}