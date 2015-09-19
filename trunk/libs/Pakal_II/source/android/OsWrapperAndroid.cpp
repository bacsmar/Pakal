#include "OsWrapperAndroid.h"
#include "OSManager.h"

using namespace Pakal;

// wrapped function
//extern int main(int argc, char *argv[]);

//// SFML 
//////////////////////////////////////////////////////////////
//static void onStart(ANativeActivity* activity);
//////////////////////////////////////////////////////////////
//static void onResume(ANativeActivity* activity);
//////////////////////////////////////////////////////////////
//static void onPause(ANativeActivity* activity);
//////////////////////////////////////////////////////////////
//static void onStop(ANativeActivity* activity);
//////////////////////////////////////////////////////////////
//static void onDestroy(ANativeActivity* activity);
//////////////////////////////////////////////////////////////
//static void onNativeWindowCreated(ANativeActivity* activity, ANativeWindow* window);
//////////////////////////////////////////////////////////////
//static void onNativeWindowDestroyed(ANativeActivity* activity, ANativeWindow* window);
//////////////////////////////////////////////////////////////
//static void onNativeWindowRedrawNeeded(ANativeActivity* activity, ANativeWindow* window);
//////////////////////////////////////////////////////////////
//static void onNativeWindowResized(ANativeActivity* activity, ANativeWindow* window);
//////////////////////////////////////////////////////////////
//static void onInputQueueCreated(ANativeActivity* activity, AInputQueue* queue);
//////////////////////////////////////////////////////////////
//static void onInputQueueDestroyed(ANativeActivity* activity, AInputQueue* queue);
//////////////////////////////////////////////////////////////
//static void onWindowFocusChanged(ANativeActivity* activity, int focused);
//////////////////////////////////////////////////////////////
//static void onContentRectChanged(ANativeActivity* activity, const ARect* rect);
//////////////////////////////////////////////////////////////
//static void onConfigurationChanged(ANativeActivity* activity);
//////////////////////////////////////////////////////////////
//static void* onSaveInstanceState(ANativeActivity* activity, size_t* outLen);
//////////////////////////////////////////////////////////////
//static void onLowMemory(ANativeActivity* activity);

// PAKAL
namespace Pakal {
	OSManager *get_osWrapper(ANativeActivity* activity)
	{
		ASSERT(activity);
		ASSERT(activity->instance);
		return static_cast<OSManager*>(activity->instance);
	}

	void onStart(ANativeActivity* activity)
	{
	}
	void onResume(ANativeActivity* activity)
	{
	}

	void onPause(ANativeActivity* activity)
	{
	}

	void onStop(ANativeActivity* activity)
	{
	}
	void onDestroy(ANativeActivity* activity)
	{
	}

	void onNativeWindowCreated(ANativeActivity* activity, ANativeWindow* window)
	{
		OSManager::WindowArgs e;
		e.windowId = (unsigned)window;
		get_osWrapper(activity)->on_window_created(e);
	}
	void onNativeWindowDestroyed(ANativeActivity* activity, ANativeWindow* window)
	{
		OSManager::WindowArgs e;
		e.windowId = (unsigned)window;
		get_osWrapper(activity)->event_window_destroyed.notify(e);
	}
	void onNativeWindowRedrawNeeded(ANativeActivity* activity, ANativeWindow* window)
	{
		OSManager::WindowArgs e;
		e.windowId = (unsigned)window;
		get_osWrapper(activity)->event_window_redraw_needed.notify(e);
	}
	void onNativeWindowResized(ANativeActivity* activity, ANativeWindow* window)
	{
		OSManager::WindowArgs e;
		e.windowId = (unsigned)window;
		e.size_x = ANativeWindow_getWidth(window);
		e.size_y = ANativeWindow_getHeight(window);
		get_osWrapper(activity)->event_window_resized.notify(e);
	}
	void onInputQueueCreated(ANativeActivity* activity, AInputQueue* queue)
	{
		
	}
	void onInputQueueDestroyed(ANativeActivity* activity, AInputQueue* queue)
	{
		
	}
	void onWindowFocusChanged(ANativeActivity* activity, int hasFocus)
	{
		get_osWrapper(activity)->event_window_focused.notify(hasFocus != 0);
	}
	void onContentRectChanged(ANativeActivity* activity, const ARect* rect)
	{
		
	}
	void onConfigurationChanged(ANativeActivity* activity)
	{
		
	}
	void* onSaveInstanceState(ANativeActivity* activity, size_t* outSize)
	{
		return nullptr;
	}
	void onLowMemory(ANativeActivity* activity)
	{
		
	}
}

OsWrapperAndroid::~OsWrapperAndroid()
{
}

OsWrapperAndroid::OsWrapperAndroid()
{
}
//
extern void SFML_ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize);

void OsWrapperAndroid::ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize)
{
	// let sfml register the activity and stuff.. but.. 
	SFML_ANativeActivity_onCreate(activity, savedState, savedStateSize);

	static auto os_wrapper = &OSManager::instance();
	activity->instance = os_wrapper;	
	// we are in charge of events
	// These functions will update the activity states and therefore, will allow	
	activity->callbacks->onStart   = Pakal::onStart;
	activity->callbacks->onResume  = Pakal::onResume;
	activity->callbacks->onPause   = Pakal::onPause;
	activity->callbacks->onStop    = Pakal::onStop;
	activity->callbacks->onDestroy = Pakal::onDestroy;

	activity->callbacks->onNativeWindowCreated = Pakal::onNativeWindowCreated;
	activity->callbacks->onNativeWindowDestroyed = Pakal::onNativeWindowDestroyed;
	activity->callbacks->onNativeWindowRedrawNeeded = Pakal::onNativeWindowRedrawNeeded;
	activity->callbacks->onNativeWindowResized = Pakal::onNativeWindowResized;

	activity->callbacks->onInputQueueCreated = Pakal::onInputQueueCreated;
	activity->callbacks->onInputQueueDestroyed = Pakal::onInputQueueDestroyed;

	activity->callbacks->onWindowFocusChanged = Pakal::onWindowFocusChanged;
	activity->callbacks->onContentRectChanged = Pakal::onContentRectChanged;
	activity->callbacks->onConfigurationChanged = Pakal::onConfigurationChanged;

	activity->callbacks->onSaveInstanceState = Pakal::onSaveInstanceState;
	activity->callbacks->onLowMemory = Pakal::onLowMemory;
}