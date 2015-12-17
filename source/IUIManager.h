#pragma once

#include "Config.h"
#include "PakalPath.h"
#include "IComponentProvider.h"


namespace Pakal
{
	class _PAKALExport IUIManager
	{
	public:
		virtual ~IUIManager() {};

		virtual bool load_document(unsigned id, const Path& resourcePath) = 0;
		virtual bool unload_document(unsigned id) = 0;

		virtual void display_document(unsigned id, bool autoresize = true) = 0;
		virtual void conceal_document(unsigned id) = 0;

		virtual bool load_font(const Path& resourcePath) = 0;

		virtual void draw_ui() = 0;
		virtual void initialize() = 0;
		virtual void terminate() = 0;
	};
}