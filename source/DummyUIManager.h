#pragma once
#include "Config.h"
#include "IUIManager.h"

namespace Pakal
{
	class _PAKALExport DummyUIManager : public IUIManager
	{
	public:
		void register_component_factories(std::vector<IComponentFactory*>& factories) override;
		TaskPtr<bool> load_document_async(unsigned id, const Path& resourcePath) override;
		TaskPtr<bool> unload_document_async(unsigned id) override;
		bool load_document(unsigned id, const Path& resourcePath) override;
		bool unload_document(unsigned id) override;
		void show_document(unsigned id, bool autoresize) override;
		void hide_document(unsigned id) override;
		bool load_font(const Path& resourcePath) override;
		void set_theme(const Path& themePath) override;
		void draw_ui() override;
		void initialize() override;
		void terminate() override;
	};
}
