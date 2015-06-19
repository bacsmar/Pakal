///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The PakalEngine Open Source Project
// File: IEntity.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 13 jun 2015
// Purpose: Interface for Entities.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Config.h"

namespace Pakal
{
	class IComponent;

	class _PAKALExport IEntity
	{
	public:	
		virtual ~IEntity(){}

		virtual void	initialize() = 0;		
		virtual int		addComponent(IComponent *c) = 0;
		virtual void	initializeComponents() = 0;
		//virtual void getComponentByName() = 0;
		//virtual void getComponentById() = 0;
	};

	
}