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
//#include <type_traits>
#include "Component.h"
#include "LogMgr.h"
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

		virtual ~IFactory() {}
	};
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	class IComponentFactory : public IFactory<Component>
	{
	public:
		virtual ~IComponentFactory(){}
	};	
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	template <class TBase>
	inline IComponentFactory * CreateComponentFactory( const std::function<Component*()>& _fn)
	{
		struct CF : IComponentFactory
		{
			virtual const char* get_typename() override { return TypeInfo::get<TBase>().getName(); };
			explicit CF(const std::function<Component*()>& fn) { create = fn; }
		};
		return new CF(_fn);
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	template <class TBase, class TImplementation, class TInitializer>
	IComponentFactory * CreateComponentFactory(TInitializer* initializer)
	{
		return CreateComponentFactory<TBase>([=]() -> Component* { return new TImplementation(initializer); });
	};
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	template <class TBase, class TImplementation>
	IComponentFactory * CreateComponentFactory()
	{
		return CreateComponentFactory<TBase>( []() -> Component* { return new TImplementation(); });
	};	
	///////////////////////////////////////////////////////////////////////////////////////////////////////
}