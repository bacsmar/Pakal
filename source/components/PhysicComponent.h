///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The PakalEngine Open Source Project
// File: IComponent.h
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: 26-11-2015
// Purpose: Component physics.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "config.h"
#include "Component.h"
namespace Pakal
{	
	class IEntity;

	class _PAKALExport PhysicComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(PhysicComponent, Component);
	public:		
		virtual ~PhysicComponent(){}
		virtual bool on_collide(const PhysicComponent& other) { return true; }
	};
}

