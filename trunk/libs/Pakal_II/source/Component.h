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

namespace Pakal
{	
	class IEntity;

	class _PAKALExport Component
	{
		IEntity* m_parent;

	public:
		DECLARE_RTTI(Component)

		virtual ~Component(){}	
		Component()  {}		
		
		inline void set_parent_entity(IEntity* entity) { m_parent = entity; };
		inline IEntity* get_parent_entity() { return m_parent; };

	};
}

