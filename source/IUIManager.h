#pragma once

#include "Config.h"
#include "TaskFwd.h"
#include "PakalPath.h"

namespace Pakal
{
	class _PAKALExport IUIManager
	{
	public:
		virtual ~IUIManager() {};

		virtual TaskPtr<bool> load_document_async(unsigned id, const Path& resourcePath) = 0;
		virtual TaskPtr<bool> unload_document_async(unsigned id) = 0;

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