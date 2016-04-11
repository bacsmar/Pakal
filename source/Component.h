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
	class Entity;

	class _PAKALExport Component
	{
		DECLARE_RTTI(Component)
		Entity* m_parent = nullptr;

	public:		
		virtual ~Component(){}	
		// TODO: necesita un mutex en el get y set
		inline void set_parent_entity(Entity* entity) { m_parent = entity; };
		inline Entity* get_parent_entity() const { return m_parent; };

	};
}

