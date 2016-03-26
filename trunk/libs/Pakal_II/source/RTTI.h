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
#include <type_traits>
#include <string>

namespace Pakal
{
	class RTTI;
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	struct TypeInfo
	{
		TypeInfo() = delete;
		TypeInfo(const TypeInfo& other) = delete;
		TypeInfo& operator=(const TypeInfo& other) = delete;

		template<class T>
		static const RTTI& get(T * obj)
		{
			ASSERT(obj != nullptr);
			ASSERT((is_RTTI_valid<typename T::__RootClass>(obj)));
			static_assert(!(std::is_same< typename T::__RootClass, T>::value), "RTTI missing");
			return T::getRTTI();
		}

		template<class T>
		static const RTTI& get()
		{
			ASSERT((is_RTTI_valid<T, typename T::__RootClass>()));
			return T::getRTTI();
		}

		template <class Derived, class Root>
		static bool is_derived()
		{
			// calling rootClassRTTI?  TypeInfo::is_derived<RootClass, Derived>() ?
			// do you have RTTI in Derived && Root?
			ASSERT(!(Derived::getRTTI() == Root::getRTTI()));

			static_assert(!(std::is_same< Derived, Root>::value), "RTTI missing");

			return Derived::getRTTI().template is_derived_from<Root>();
		}

		template <class T, class U>
		static bool is_derived(U *obj)
		{
			ASSERT(obj);
			static_assert(!(std::is_same< T, U>::value), "RTTI missing");
			return obj->getRTTI().template is_derived_from<T>();
		}

		template <typename T, typename Root>
		static bool is_RTTI_valid()
		{
			return (!(Root::getRTTI() == T::getRTTI())) || std::is_abstract<T>::value;
		}
		template <typename Root, typename T>
		static bool is_RTTI_valid(T*)
		{
			return is_RTTI_valid<T, Root>();
		}
	};
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	namespace crypt_utils
	{
		extern uint32_t hash_joaat(const std::string& key);	// from utils.h
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	class _PAKALExport RTTI
	{
	public:
		explicit RTTI(const char* name) : m_name(name), m_parent(nullptr), m_hash(crypt_utils::hash_joaat(m_name)) {}
		explicit RTTI(const char* name, const RTTI& rtti) : m_name(name), m_parent(&rtti), m_hash(crypt_utils::hash_joaat(m_name)) {}

		inline const std::string& get_name() const { return m_name; }

		inline bool is_type(const RTTI &rtti) const { return &rtti == this; }

		template <class T>
		inline bool is_type() const
		{
			ASSERT((TypeInfo::is_RTTI_valid<T, typename T::__RootClass>()));
			return is_type(T::getRTTI());
		}
		template <class T>
		inline bool is_type(T*) const
		{			
			return is_type<T>();
		}		

		template <class T>
		inline bool is_derived_from() const
		{
			ASSERT((TypeInfo::is_RTTI_valid<T, typename T::__RootClass>()));
			return is_derived_from(T::getRTTI());
		}
		template <class T>
		inline bool is_derived_from(T *) const
		{			
			return is_derived_from<T>();
		}

		friend bool operator==(const RTTI &left, const RTTI &right);
		
	private:

		bool is_derived_from(const RTTI &from) const
		{
			const RTTI *base = this;
			while (base != nullptr)
			{
				if (base->m_hash == from.m_hash)
					return true;
				base = base->m_parent;
			}
			return false;
		}

		std::string m_name;		
		const RTTI *m_parent;
		uint32_t m_hash;

		void operator=(const RTTI &other) = delete;
		RTTI(const RTTI &other) = delete;
	};
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	inline bool operator==(const RTTI &left, const RTTI &right)
	{
		return &left == &right;
	}	
}
// DECLARE_RTTI is intended for use with root ABSTRACT classes
#define DECLARE_RTTI(x)  \
	protected:\
	friend Pakal::TypeInfo; \
	friend Pakal::RTTI;\
	using __RootClass = x;\
	virtual void _implement_RTTI_please() = 0;\
	static const Pakal::RTTI &getRTTI()\
	{\
		static Pakal::RTTI s_RTTI_Info(#x);\
		return s_RTTI_Info;\
	}\
	template <class BaseClass>\
	static const bool is_RTTI_valid()\
	{\
		return !(BaseClass::getRTTI() == getRTTI());\
	}\
	public:\
	virtual const Pakal::RTTI &get_type() const\
	{\
		return x::getRTTI();\
	}\
	private:
	
#define DECLARE_RTTI_WITH_BASE(x,base)  \
	protected:\
	friend Pakal::TypeInfo;\
	friend Pakal::RTTI;\
	virtual void _implement_RTTI_please() override {};\
	static const Pakal::RTTI &getRTTI()\
	{\
		/*static x *_##x = 0;*/\
		static Pakal::RTTI s_RTTI_Info(#x,base::getRTTI());\
		return s_RTTI_Info;\
	}\
	public:\
	virtual const Pakal::RTTI &get_type() const override\
	{\
		return x::getRTTI();\
	}\
	private: