#include "DummyUIManager.h"
#include "Task.h"

using namespace Pakal;

void DummyUIManager::register_component_factories(std::vector<IComponentFactory*>& factories)
{
}

TaskPtr<bool> DummyUIManager::load_document_async(unsigned id, const Path& resourcePath)
{
	static TaskPtr<bool> t = std::make_shared<Task<bool>>(false);
	return t;	
}

TaskPtr<bool> DummyUIManager::unload_document_async(unsigned id)
{
	static TaskPtr<bool> t = std::make_shared<Task<bool>>(false);
	return t;
}

bool DummyUIManager::load_document(unsigned id, const Path& resourcePath)
{
	return false;
}

bool DummyUIManager::unload_document(unsigned id)
{
	return false;
}

void DummyUIManager::show_document(unsigned id, bool autoresize)
{
}

void DummyUIManager::hide_document(unsigned id)
{
}

bool DummyUIManager::load_font(const Path& resourcePath)
{
	return false;
}

void DummyUIManager::set_theme(const Path& themePath)
{
}

void DummyUIManager::draw_ui()
{
}

void DummyUIManager::initialize()
{
}

void DummyUIManager::terminate()
{
}
