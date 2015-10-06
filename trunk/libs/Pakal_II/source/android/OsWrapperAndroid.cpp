#include "OsWrapperAndroid.h"
#include "OSManager.h"
#include <android/configuration.h>
#include "ResourceManager.h"

using namespace Pakal;

// wrapped function
//extern int main(int argc, char *argv[]);

//// SFML 
////////////////////////////////////////////////////////////
namespace sf {
	extern void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize);
	//extern void onStart(ANativeActivity* activity);
	//extern void onResume(ANativeActivity* activity);
	//extern void onPause(ANativeActivity* activity);
	//extern void onStop(ANativeActivity* activity);
	//extern void onDestroy(ANativeActivity* activity);	
	extern void onNativeWindowCreated(ANativeActivity* activity, ANativeWindow* window);
	extern void onNativeWindowDestroyed(ANativeActivity* activity, ANativeWindow* window);
	//extern void onNativeWindowRedrawNeeded(ANativeActivity* activity, ANativeWindow* window);
	//extern void onNativeWindowResized(ANativeActivity* activity, ANativeWindow* window);
	extern void onInputQueueCreated(ANativeActivity* activity, AInputQueue* queue);
	extern void onInputQueueDestroyed(ANativeActivity* activity, AInputQueue* queue);
	//extern void onWindowFocusChanged(ANativeActivity* activity, int focused);
	extern void onContentRectChanged(ANativeActivity* activity, const ARect* rect);
	//extern void onConfigurationChanged(ANativeActivity* activity);
	//extern void* onSaveInstanceState(ANativeActivity* activity, size_t* outLen);
	//extern void onLowMemory(ANativeActivity* activity);
}

// PAKAL
namespace Pakal 
{
	static OSManager *OSManagerInstance;
	OSManager *get_osWrapper()
	{		
		ASSERT(OSManagerInstance);
		return OSManagerInstance;
	}

	void onStart(ANativeActivity* activity)
	{
		get_osWrapper()->on_app_started();
	}
	void onResume(ANativeActivity* activity)
	{		
		get_osWrapper()->on_app_resumed();
	}

	void onPause(ANativeActivity* activity)
	{
		get_osWrapper()->on_app_paused();
	}

	void onStop(ANativeActivity* activity)
	{
		get_osWrapper()->on_app_stoped();
	}
	void onDestroy(ANativeActivity* activity)
	{
		get_osWrapper()->on_app_finished();
	}

	void onNativeWindowCreated(ANativeActivity* activity, ANativeWindow* window)
	{		
		OSManager::WindowArgs e;
		e.windowId = (unsigned)window;
		e.size_x = ANativeWindow_getWidth(window);
		e.size_y = ANativeWindow_getHeight(window);
		get_osWrapper()->on_window_created(e);				
	}
	void onNativeWindowDestroyed(ANativeActivity* activity, ANativeWindow* window)
	{		
		OSManager::WindowArgs e;
		e.windowId = (unsigned)window;
		get_osWrapper()->on_window_destroyed(e);		
	}
	void onNativeWindowRedrawNeeded(ANativeActivity* activity, ANativeWindow* window)
	{
		OSManager::WindowArgs e;
		e.windowId = (unsigned)window;
		get_osWrapper()->on_window_redraw_needed(e);
	}
	void onNativeWindowResized(ANativeActivity* activity, ANativeWindow* window)
	{
		OSManager::WindowArgs e;
		e.windowId = (unsigned)window;
		e.size_x = ANativeWindow_getWidth(window);
		e.size_y = ANativeWindow_getHeight(window);
		get_osWrapper()->on_window_resized(e);
	}
	void onInputQueueCreated(ANativeActivity* activity, AInputQueue* queue)
	{
		sf::onInputQueueCreated(activity, queue); //SFML
	}
	void onInputQueueDestroyed(ANativeActivity* activity, AInputQueue* queue)
	{
		sf::onInputQueueDestroyed(activity, queue); //SFML
	}
	void onWindowFocusChanged(ANativeActivity* activity, int hasFocus)
	{
		get_osWrapper()->on_window_focused(hasFocus != 0);
	}
	void onContentRectChanged(ANativeActivity* activity, const ARect* rect)
	{
		sf::onContentRectChanged(activity, rect);	//SFML
	}
	void onConfigurationChanged(ANativeActivity* activity)
	{
		AConfiguration *config = get_osWrapper()->configuration;

		AConfiguration_getOrientation(config);
		AConfiguration_fromAssetManager(config, activity->assetManager);
		//print_cur_config(android_app);
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
	AConfiguration_delete(configuration);
}

OsWrapperAndroid::OsWrapperAndroid()
{
	configuration = AConfiguration_new();
}
//

void OsWrapperAndroid::ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize)
{
	// let sfml register the activity and stuff.. but.. 
	if (Pakal::OSManagerInstance == nullptr)
	{
		sf::ANativeActivity_onCreate(activity, savedState, savedStateSize);
	}
		
	Pakal::OSManagerInstance = &OSManager::instance();
	OSManager::instance().activity = activity;
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

std::string OsWrapperAndroid::get_system_sources()
{
	return std::string(activity->obbPath) + "/";
}