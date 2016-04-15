#include "DummyGraphicsSystem.h"
#include "LogMgr.h"
#include "ResourceManager.h"

#ifdef PAKAL_USE_DUMMY_GRAPHICS
#include "IUIManager.h"

using namespace Pakal;

void DummyGraphicsSystem::register_component_factories(std::vector<IComponentFactory*>& factories)
{
	LOG_INFO("DummyGraphicsSystem-register_component_factories");
}

const char* DummyGraphicsSystem::get_system_name()
{
	return "DummyGraphicsSystem";
}

void DummyGraphicsSystem::set_window_caption(const char* title)
{
	//OSMgr.get_window_manager()->set_title(title);
}

void DummyGraphicsSystem::add_debug_drawer(IDebugDrawerClient* debugDrawer)
{
	LOG_INFO("IDebugDrawerClient");
}

tmath::vector2du DummyGraphicsSystem::get_screen_resolution()
{
	return{0,0};
}

void DummyGraphicsSystem::add_to_update_list(IUpdatable* updatable)
{
	LOG_INFO("DummyGraphicsSystem-add_to_update_list");
}

void DummyGraphicsSystem::remove_from_update_list(IUpdatable* updatable)
{
	LOG_INFO("DummyGraphicsSystem-remove_from_update_list");
}

DummyGraphicsSystem::~DummyGraphicsSystem()
{
	LOG_INFO("destroying DummyGraphicsSystem");
}

class DirectorySourceFake : public DirectorySource
{
public:
	SharedPtr<std::istream> open_resource(const Path& pathToResource) override { return{}; };
	bool initialize(const Path& path, bool recursive) override { return false; };
	void terminate() override{};
};

class ZipSourceFake : public ZipSource
{
public:
	SharedPtr<std::istream> open_resource(const Path& pathToResource) override { return{}; };
	bool initialize(const Path& pathToZip, bool recursiveDirs, const std::string& password) override { return false; };
	void terminate() override{};
};

void DummyGraphicsSystem::on_init_graphics(const WindowArgs& args)
{		
	ResourceMgr.register_source<DirectorySource>([this]() { return new DirectorySourceFake(); });
	ResourceMgr.register_source<ZipSource>([this]() { return new ZipSourceFake(); });
	LOG_INFO("DummyGraphicsSystem-on_init_graphics %d %d %d", args.windowId, args.size_x, args.size_y);
}

void DummyGraphicsSystem::on_update_graphics(long long dt)
{
}

void DummyGraphicsSystem::on_terminate_graphics()
{
	LOG_INFO("DummyGraphicsSystem-on_terminate_graphics");
}

void DummyGraphicsSystem::on_pause_graphics()
{
	LOG_INFO("DummyGraphicsSystem-on_pause_graphics");
}

void DummyGraphicsSystem::on_resume_graphics()
{
	LOG_INFO("DummyGraphicsSystem-on_resume_graphics");
}

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
#endif
