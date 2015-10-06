#include "OsWrapperAndroid.h"
#include "OSManager.h"
#include <android/configuration.h>
#include "ResourceManager.h"

using namespace Pakal;

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
	inline OSManager *get_osWrapper()
	{		
		ASSERT(OSManagerInstance);
		return OSManagerInstance;
	}

	static void print_cur_config()
	{
		char lang[2], country[2];
		AConfiguration_getLanguage(get_osWrapper()->configuration, lang);
		AConfiguration_getCountry(get_osWrapper()->configuration, country);

		LOG_ERROR("Config: mcc=%d mnc=%d lang=%c%c cnt=%c%c orien=%d touch=%d dens=%d "
			"keys=%d nav=%d keysHid=%d navHid=%d sdk=%d size=%d long=%d "
			"modetype=%d modenight=%d",
			AConfiguration_getMcc(get_osWrapper()->configuration),
			AConfiguration_getMnc(get_osWrapper()->configuration),
			lang[0], lang[1], country[0], country[1],
			AConfiguration_getOrientation(get_osWrapper()->configuration),
			AConfiguration_getTouchscreen(get_osWrapper()->configuration),
			AConfiguration_getDensity(get_osWrapper()->configuration),
			AConfiguration_getKeyboard(get_osWrapper()->configuration),
			AConfiguration_getNavigation(get_osWrapper()->configuration),
			AConfiguration_getKeysHidden(get_osWrapper()->configuration),
			AConfiguration_getNavHidden(get_osWrapper()->configuration),
			AConfiguration_getSdkVersion(get_osWrapper()->configuration),
			AConfiguration_getScreenSize(get_osWrapper()->configuration),
			AConfiguration_getScreenLong(get_osWrapper()->configuration),
			AConfiguration_getUiModeType(get_osWrapper()->configuration),
			AConfiguration_getUiModeNight(get_osWrapper()->configuration));
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
		int orientation = AConfiguration_getOrientation( get_osWrapper()->configuration);
		if( orientation == get_osWrapper()->orientation)
		{
			get_osWrapper()->on_app_finished();
		}else
		{
			get_osWrapper()->is_processing_config_change = true;
		}
		
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
		print_cur_config();
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
	is_processing_config_change = false;
}
//
#include <condition_variable>
// wrapped function
//extern int main(int argc, char *argv[]);
//std::condition_variable main_condition_variable;
//std::mutex main_mutex;
//
//int _main(int argc, char *argv[])
//{	
//	static bool finished = false;
//	std::unique_lock<std::mutex> lock(main_mutex);
//	if (Pakal::OSManagerInstance == nullptr)
//	{
//		::main(0, NULL);
//		finished = true;
//		main_condition_variable.notify_one();
//	}
//	main_condition_variable.wait(lock, [&]() { return finished; });	
//	return 0;
//}

void OsWrapperAndroid::ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize)
{
	if( Pakal::OSManagerInstance != nullptr && Pakal::OSManagerInstance->is_processing_config_change )
	{
		Pakal::OSManagerInstance->is_processing_config_change = false;
		return;
	}
	// let sfml register the activity and stuff.. but.. 
	sf::ANativeActivity_onCreate(activity, savedState, savedStateSize);	
		
	Pakal::OSManagerInstance = &OSManager::instance();	// set OSManager Instance

	get_osWrapper()->activity = activity;
	//OSManager::instance().configuration = AConfiguration_new();
	int orientation = AConfiguration_getOrientation(get_osWrapper()->configuration );
	get_osWrapper()->orientation = orientation;

	AConfiguration_fromAssetManager(get_osWrapper()->configuration, get_osWrapper()->activity->assetManager);
	print_cur_config();

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