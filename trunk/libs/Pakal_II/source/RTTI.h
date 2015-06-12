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
#include "Platform.h"

namespace Pakal{

	class RTTI
	{
	public:
		RTTI(const char* name) : m_name(name), m_parent(0){}
		RTTI(const char* name, const RTTI& rtti) : m_name(name), m_parent(&rtti){}

		inline const char * getName() const { return m_name; }

		// usage  object.isType( ClassToCompare::getRTTI() )
		inline bool isType(const RTTI &rtti) const
		{
			return &rtti == this;
		}
		// usage  object.isType<ClassToCompare>(  )
		template <class T>
		bool isType() const
		{
			return &T::getRTTI() == this;
		}

		bool isDerivedFrom(const RTTI &from) const
		{
			const RTTI *base = this;
			while ( base != 0)
			{
				if( base == &from)
					return true;
				base = base->m_parent;
			}
			return false;
		}

		inline bool operator==(const RTTI &other)
		{
			return isDerivedFrom(other);
		}

		friend bool operator==(const RTTI &left, const RTTI &right);
		
	private:
		const char *m_name;		
		const RTTI *m_parent;
	};

	inline bool operator==(const RTTI &left, const RTTI &right)
	{
		return &left == &right;
	}
}

#define DECLARE_RTTI(x) public: \
	static const Pakal::RTTI &getRTTI()\
	{\
		x *_##x = 0;\
		static Pakal::RTTI s_RTTI_Info(#x);\
		return s_RTTI_Info;\
	}\
	virtual const Pakal::RTTI &getType()\
	{\
		return x::getRTTI();\
	}

#define DECLARE_RTTI_WITH_BASE(x,base) public: \
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
 
