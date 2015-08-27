///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The PakalEngine Open Source Project
// File: IEntity.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 13 jun 2015
// Purpose: Interface for Entities.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "IComponent.h"


namespace Pakal
{
	class GraphicsSystem;	

	class _PAKALExport GraphicComponent : public IComponent
	{
		friend class GraphicsSystem;
	public:
		DECLARE_RTTI_WITH_BASE(GraphicComponent,IComponent)

		virtual ~GraphicComponent();

		explicit GraphicComponent(GraphicsSystem *graphicSystem) : m_graphic_system(graphicSystem) {}
		
		BasicTaskPtr init() override final;	// hide init From derivated classes
		BasicTaskPtr destroy() override final;	// hide init From derivated classes
	protected:
		// is called when the component was initialized by the parent System
		virtual void on_init() = 0;
		// is called when the component was destroyed by the parent System
		virtual void on_destroy() = 0;

	protected:
		GraphicsSystem *m_graphic_system;		
	
	};
}