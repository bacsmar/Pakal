///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The PakalEngine Open Source Project
// File: IEntity.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 13 jun 2015
// Purpose: Interface for Entities.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "IComponent.h"
#include "RTTI.h"
#include "TaskFwd.h"


namespace Pakal
{
	class GraphicsSystem;	

	class _PAKALExport GraphicComponent : public IComponent
	{
		friend class GraphicsSystem;
	public:

		virtual ~GraphicComponent();
		GraphicComponent(GraphicsSystem *graphicSystem) : m_GraphicSystem(graphicSystem)	{}		
		void setSystem(GraphicsSystem *graphicSystem)	{ m_GraphicSystem = graphicSystem; }		

		BasicTaskPtr init() override final;	// hide init From derivated classes
		BasicTaskPtr destroy() override final;	// hide init From derivated classes	


	protected:
		inline GraphicsSystem *getGraphicSystem() { return m_GraphicSystem; }
		// is called when the component was initialized by the parent System
		virtual void onInit() = 0;
		// is called when the component was destroyed by the parent System
		virtual void onDestroy() = 0;

		virtual void onUpdate() = 0;

		// notifies the parent System of work to do
		virtual void notify();

		GraphicsSystem *m_GraphicSystem;		
	
	};
}