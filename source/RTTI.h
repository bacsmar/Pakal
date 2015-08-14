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
#include "config.h"

namespace Pakal{

	class RTTI
	{
	public:
		RTTI(const char* name) : m_name(name), m_parent(nullptr){}
		RTTI(const char* name, const RTTI& rtti) : m_name(name), m_parent(&rtti){}

		inline const char * getName() const { return m_name; }

		inline bool isType(const RTTI &rtti) const { return &rtti == this; }

		template <class T>
		bool isType() const { return isType(T::getRTTI()); }

		bool isDerivedFrom(const RTTI &from) const
		{
			const RTTI *base = this;
			while ( base != nullptr)
			{
				if( base == &from)
					return true;
				base = base->m_parent;
			}
			return false;
		}

		template <class T>
		bool isDerivedFrom() const { return isDerivedFrom(T::getRTTI()); }


		friend bool operator==(const RTTI &left, const RTTI &right);
		
	private:
		const char *m_name;		
		const RTTI *m_parent;

		void operator=(const RTTI &other){}
		RTTI(const RTTI &other){}
	};

	inline bool operator==(const RTTI &left, const RTTI &right)
	{
		return &left == &right;
	}
}

#define DECLARE_RTTI(x)  \
	static const Pakal::RTTI &getRTTI()\
	{\
		static Pakal::RTTI s_RTTI_Info(#x);\
		return s_RTTI_Info;\
	}\
	virtual const Pakal::RTTI &getType()\
	{\
		return x::getRTTI();\
	}


#define DECLARE_RTTI_WITH_BASE(x,base)  \
	static const Pakal::RTTI &getRTTI()\
	{\
		x *_##x = 0;\
		static Pakal::RTTI s_RTTI_Info(#x,base::getRTTI());\
		return s_RTTI_Info;\
	}\
	virtual const Pakal::RTTI &getType()\
	{\
		return x::getRTTI();\
	}
 


#define REQUIRE_RTTI() \
	virtual const Pakal::RTTI &getType() = 0;\
	static const Pakal::RTTI &getRTTI();