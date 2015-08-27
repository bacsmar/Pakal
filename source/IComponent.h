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
#include "TaskFwd.h"

namespace Pakal
{	
	class IEntity;

	class _PAKALExport IComponent
	{
	public:
		DECLARE_RTTI(IComponent)

		virtual ~IComponent(){}	
		IComponent()  {}		

		virtual BasicTaskPtr initialize() = 0;
		virtual BasicTaskPtr destroy() = 0;

		virtual void set_parent_entity(IEntity* entity) = 0;
		virtual IEntity* get_parent_entity() = 0;
	};
}

