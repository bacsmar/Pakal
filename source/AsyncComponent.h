///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The PakalEngine Open Source Project
// File: IComponent.h
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: 01-11-2012
// last modification: 13 jun 2015
// Purpose: AsyncComponent Interface.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "config.h"

#include "RTTI.h"

namespace Pakal
{	
	class AsyncEntity;
	class ComponentManager;

	class _PAKALExport AsyncComponent
	{
		DECLARE_RTTI(AsyncComponent)
		AsyncEntity* m_parent = nullptr;
		bool m_pendingDispose = false;

	public:		
		virtual ~AsyncComponent(){}	
		virtual void prepare_dispose() {}
		// TODO: necesita un mutex en el get y set
		inline void set_parent_entity(AsyncEntity* entity) { m_parent = entity; };
		inline AsyncEntity* get_parent_entity() const { return m_parent; };
		void request_dispose();
		bool is_pending_dispose() const { return m_pendingDispose; }

		friend class ComponentManager;

	};
}

