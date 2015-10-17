#pragma once

#include "Config.h"
#include "IManager.h"
#include "IComponentProvider.h"
#include "InputDevice.h"

namespace Pakal
{
	class _PAKALExport IUIManager
	{
		friend class GraphicsSystem;
	protected:
		virtual void draw_ui() = 0;
		virtual void initialize() = 0;
		virtual void terminate() = 0;
	public:
		virtual ~IUIManager() {};
		virtual bool load_document(const char* documentPath, bool show = true, bool autoresize = true) = 0;
		virtual bool close_document(const char* documentId) = 0;
		virtual bool set_document_visible(const char* documentId, const bool visible) = 0;
		virtual bool load_font(const char* path) = 0;
	};
}