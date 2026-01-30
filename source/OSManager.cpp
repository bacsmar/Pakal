#include "OSManager.h"
#include "SingletonHolder.h"
#include "IInputManager.h"

using namespace Pakal;

#ifdef PAKAL_ANDROID_PLATFORM
	#include "android/OsWrapperAndroid.h"
	using OSManagerImpl = OsWrapperAndroid;
#else
	using OSManagerImpl = OSManager;
#endif

OSManager& OSManager::instance()
{
	static SingletonHolder<OSManagerImpl> sh;
	return *sh.get();
}

void OSManager::initialize(const Settings& settings)
{
	m_input_manager = settings.input_manager_allocator(this);
	m_window_impl = settings.window_manager_allocator(this);
	m_windows_setup_task = TaskCompletionSource<WindowArgs>();
}

void OSManager::terminate()
{
	SAFE_DEL(m_window_impl);
	SAFE_DEL(m_input_manager);
	event_app_finished();
}

TaskPtr<WindowArgs> OSManager::setup_window(unsigned windowId, const tmath::vector2di& dimensions, bool fullscreen, unsigned bitsPerPixel)
{
	WindowArgs args = m_window_impl->setup_window(windowId, dimensions, fullscreen, bitsPerPixel);
	if (args.windowId)
	{
		args.size_x = dimensions.x;
		args.size_y = dimensions.y;
		on_window_created(args);
	}
	return m_windows_setup_task.get_task();
}

void OSManager::flush_window()
{
	m_window_impl->flush();
}

void OSManager::close_window()
{
	m_window_impl->close_window();
}

void OSManager::process_window_events()
{
	m_window_impl->process_window_events();
}

void OSManager::on_window_created(const WindowArgs& arg)
{
	event_window_created.notify(arg);
	m_windows_setup_task.set_completed(arg);
}

void OSManager::on_window_destroyed(const WindowArgs& arg)
{
	m_windows_setup_task = TaskCompletionSource<WindowArgs>();
	event_window_destroyed.notify(arg);
}