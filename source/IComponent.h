///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The PakalEngine Open Source Project
// File: IComponent.h
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: 01-11-2012
// last modification: 13 jun 2015
// Purpose: Component Interface.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "config.h"

#include "RTTI.h"
#include "BasicTask.h"

namespace Pakal
{	
	class IEntity;	
	class BasicTask;
	//--------------------------------- IComponent ---------------------------------------------------
	class _PAKALExport IComponent
	{
		friend class IEntity;
	public:
		DECLARE_RTTI(IComponent)

		virtual ~IComponent(void){}	
		
		IComponent() : 
			m_parent_entity(nullptr),
			m_initialized(false)
		{}		

		inline const IEntity* get_parent_entity() const		{ return m_parent_entity; };
		inline void set_parent_entity(const IEntity *parent)	{ m_parent_entity = parent; };		
		inline bool is_initialized()	const					{ return m_initialized; }
		

		virtual BasicTaskPtr init()	= 0;
		virtual BasicTaskPtr destroy()	= 0;

	protected:		

		const IEntity* m_parent_entity;		
		bool		   m_initialized;		

	};
}

