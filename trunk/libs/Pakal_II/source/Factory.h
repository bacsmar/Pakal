///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 The PakalEngine Open Source Project
// File: IComponentFactory.h
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: 01-11-2012
//
// Purpose: Interface to create Icomponent
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "config.h"
#include <functional>


namespace Pakal
{
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	template <class T>
	class IFactory
	{
	public:
		std::function<T*()> create;

		virtual const char* get_typename() = 0;
		explicit IFactory(const std::function<T*()>& fn): create(fn){}
		virtual ~IFactory() {}
	};
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	template <class TRoot, class TBase>
	inline IFactory<TRoot>* CreateFactory(const std::function<TRoot*()>& _fn)
	{
		static_assert(std::is_base_of < TRoot, TBase>::value, "Incompatible Types");
		struct Factory : IFactory<TRoot>
		{
			virtual const char* get_typename() override { return TypeInfo::get<TBase>().getName(); };
			explicit Factory(const std::function<TRoot*()>& fn) : IFactory<TRoot>(fn){}
		};
		return new Factory(_fn);
	}	
	///////////////////////////////////////////////////////////////////////////////////////////////////////
}