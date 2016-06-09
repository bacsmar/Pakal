#include "OsWrapperAndroid.h"
#include <android/configuration.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>
#include "ResourceManager.h"

using namespace Pakal;


static pthread_key_t mThreadKey;
static JavaVM* mJavaVM;
JNIEnv* Android_JNI_GetEnv(void);
static void Android_JNI_ThreadDestroyed(void*);
//
//// PAKAL
namespace Pakal
{
//	static OsWrapperAndroid* OSManagerInstance = nullptr;
//
//	inline OsWrapperAndroid* get_osWrapper()
//	{
//		ASSERT(OSManagerInstance);
//		return OSManagerInstance;
//	}
//
//	static void print_cur_config()
//	{
//		char lang[2], country[2];
//		AConfiguration_getLanguage(get_osWrapper()->configuration, lang);
//		AConfiguration_getCountry(get_osWrapper()->configuration, country);
//
//		LOG_ERROR("Config: mcc=%d mnc=%d lang=%c%c cnt=%c%c orien=%d touch=%d dens=%d "
//			"keys=%d nav=%d keysHid=%d navHid=%d sdk=%d size=%d long=%d "
//			"modetype=%d modenight=%d",
//			AConfiguration_getMcc(get_osWrapper()->configuration),
//			AConfiguration_getMnc(get_osWrapper()->configuration),
//			lang[0], lang[1], country[0], country[1],
//			AConfiguration_getOrientation(get_osWrapper()->configuration),
//			AConfiguration_getTouchscreen(get_osWrapper()->configuration),
//			AConfiguration_getDensity(get_osWrapper()->configuration),
//			AConfiguration_getKeyboard(get_osWrapper()->configuration),
//			AConfiguration_getNavigation(get_osWrapper()->configuration),
//			AConfiguration_getKeysHidden(get_osWrapper()->configuration),
//			AConfiguration_getNavHidden(get_osWrapper()->configuration),
//			AConfiguration_getSdkVersion(get_osWrapper()->configuration),
//			AConfiguration_getScreenSize(get_osWrapper()->configuration),
//			AConfiguration_getScreenLong(get_osWrapper()->configuration),
//			AConfiguration_getUiModeType(get_osWrapper()->configuration),
//			AConfiguration_getUiModeNight(get_osWrapper()->configuration));
//	}
//
//	void onStart(ANativeActivity* activity)
//	{
//		get_osWrapper()->on_app_started();
//	}
//
//	void onResume(ANativeActivity* activity)
//	{
//		get_osWrapper()->on_app_resumed();
//	}
//
//	void onPause(ANativeActivity* activity)
//	{
//		get_osWrapper()->on_app_paused();
//	}
//
//	void onStop(ANativeActivity* activity)
//	{
//		get_osWrapper()->on_app_stoped();
//	}
//
//	void onDestroy(ANativeActivity* activity)
//	{
//		int orientation = AConfiguration_getOrientation(get_osWrapper()->configuration);
//		if (orientation == get_osWrapper()->orientation)
//		{
//			get_osWrapper()->on_app_finished();
//		}		
//	}
//
//	void onNativeWindowCreated(ANativeActivity* activity, ANativeWindow* window)
//	{
//		Pakal::WindowArgs e;
//		e.windowId = (unsigned)window;
//		e.size_x = ANativeWindow_getWidth(window);
//		e.size_y = ANativeWindow_getHeight(window);
//		get_osWrapper()->on_window_created(e);
//	}
//
//	void onNativeWindowDestroyed(ANativeActivity* activity, ANativeWindow* window)
//	{
//		Pakal::WindowArgs e;
//		e.windowId = (unsigned)window;
//		get_osWrapper()->on_window_destroyed(e);
//	}
//
//	void onNativeWindowRedrawNeeded(ANativeActivity* activity, ANativeWindow* window)
//	{
//		Pakal::WindowArgs e;
//		e.windowId = (unsigned)window;
//		get_osWrapper()->on_window_redraw_needed(e);
//	}
//
//	void onNativeWindowResized(ANativeActivity* activity, ANativeWindow* window)
//	{
//		Pakal::WindowArgs e;
//		e.windowId = (unsigned)window;
//		e.size_x = ANativeWindow_getWidth(window);
//		e.size_y = ANativeWindow_getHeight(window);
//		get_osWrapper()->on_window_resized(e);
//	}
//
//	void onInputQueueCreated(ANativeActivity* activity, AInputQueue* queue)
//	{
//		//sf::onInputQueueCreated(activity, queue); //SFML
//	}
//
//	void onInputQueueDestroyed(ANativeActivity* activity, AInputQueue* queue)
//	{
//		//sf::onInputQueueDestroyed(activity, queue); //SFML
//	}
//
//	void onWindowFocusChanged(ANativeActivity* activity, int hasFocus)
//	{
//		get_osWrapper()->on_window_focused(hasFocus != 0);
//	}
//
//	void onContentRectChanged(ANativeActivity* activity, const ARect* rect)
//	{
//		//sf::onContentRectChanged(activity, rect);	//SFML
//	}
//
//	void onConfigurationChanged(ANativeActivity* activity)
//	{
//		AConfiguration* config = get_osWrapper()->configuration;
//
//		AConfiguration_getOrientation(config);
//		AConfiguration_fromAssetManager(config, activity->assetManager);
//		print_cur_config();
//	}
//
//	void* onSaveInstanceState(ANativeActivity* activity, size_t* outSize)
//	{
//		return nullptr;
//	}
//
//	void onLowMemory(ANativeActivity* activity)
//	{
//	}
}

OsWrapperAndroid::~OsWrapperAndroid()
{
//	AConfiguration_delete(configuration);
}

OsWrapperAndroid::OsWrapperAndroid(): activity(nullptr)
{
	//configuration = AConfiguration_new();
}

//void OsWrapperAndroid::ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize)
//{
//	LOG_INFO("OsWrapperAndroid::ANativeActivity_onCreate");	
//
//	Pakal::OSManagerInstance = &static_cast<OsWrapperAndroid&>(OsWrapperAndroid::instance()); // set OSManager Instance	
//	
//	LOG_INFO("OsWrapperAndroid, setting activity %X", activity);
//	get_osWrapper()->activity = activity;
//
//	LOG_INFO("OsWrapperAndroid, setting vm %X", activity->vm);
//	mJavaVM = activity->vm;
//
//	LOG_INFO("creating pthread key");
//	if (pthread_key_create(&mThreadKey, Android_JNI_ThreadDestroyed) != 0) 
//	{
//		LOG_ERROR("error initializing pthread key");
//	}
//	LOG_INFO("starting env");
//	Android_JNI_GetEnv();
//	//get_osWrapper()->configuration = AConfiguration_new();
//
//	int orientation = AConfiguration_getOrientation(get_osWrapper()->configuration);
//	get_osWrapper()->orientation = orientation;
//	LOG_INFO("OsWrapperAndroid::ANativeActivity_onCreate 3");
//
//	//AConfiguration_fromAssetManager(get_osWrapper()->configuration, get_osWrapper()->activity->assetManager);
//	//LOG_INFO("OsWrapperAndroid::ANativeActivity_onCreate 2");
//	//print_cur_config();
//	//LOG_INFO("OsWrapperAndroid::ANativeActivity_onCreate 2");
//	//// we are in charge of events
//	//// These functions will update the activity states and therefore, will allow	
//	//activity->callbacks->onStart   = Pakal::onStart;
//	//activity->callbacks->onResume  = Pakal::onResume;
//	//activity->callbacks->onPause   = Pakal::onPause;
//	//activity->callbacks->onStop    = Pakal::onStop;
//	//activity->callbacks->onDestroy = Pakal::onDestroy;
//
//	//activity->callbacks->onNativeWindowCreated = Pakal::onNativeWindowCreated;
//	//activity->callbacks->onNativeWindowDestroyed = Pakal::onNativeWindowDestroyed;
//	//activity->callbacks->onNativeWindowRedrawNeeded = Pakal::onNativeWindowRedrawNeeded;
//	//activity->callbacks->onNativeWindowResized = Pakal::onNativeWindowResized;
//
//	//activity->callbacks->onInputQueueCreated = Pakal::onInputQueueCreated;
//	//activity->callbacks->onInputQueueDestroyed = Pakal::onInputQueueDestroyed;
//
//	//activity->callbacks->onWindowFocusChanged = Pakal::onWindowFocusChanged;
//	//activity->callbacks->onContentRectChanged = Pakal::onContentRectChanged;
//	//activity->callbacks->onConfigurationChanged = Pakal::onConfigurationChanged;
//
//	//activity->callbacks->onSaveInstanceState = Pakal::onSaveInstanceState;
//	//activity->callbacks->onLowMemory = Pakal::onLowMemory;
//}

static void Android_JNI_ThreadDestroyed(void* value)
{
	/* The thread is being destroyed, detach it from the Java VM and set the mThreadKey value to NULL as required */
	JNIEnv *env = (JNIEnv*)value;
	if (env != NULL) 
	{
		(mJavaVM)->DetachCurrentThread();
		//(*mJavaVM)->DetachCurrentThread(mJavaVM);
		pthread_setspecific(mThreadKey, NULL);
	}
}

JNIEnv* Android_JNI_GetEnv(void)
{
	/* From http://developer.android.com/guide/practices/jni.html
	* All threads are Linux threads, scheduled by the kernel.
	* They're usually started from managed code (using Thread.start), but they can also be created elsewhere and then
	* attached to the JavaVM. For example, a thread started with pthread_create can be attached with the
	* JNI AttachCurrentThread or AttachCurrentThreadAsDaemon functions. Until a thread is attached, it has no JNIEnv,
	* and cannot make JNI calls.
	* Attaching a natively-created thread causes a java.lang.Thread object to be constructed and added to the "main"
	* ThreadGroup, making it visible to the debugger. Calling AttachCurrentThread on an already-attached thread
	* is a no-op.
	* Note: You can call this function any number of times for the same thread, there's no harm in it
	*/

	JNIEnv *env;
	//int status = (*mJavaVM)->AttachCurrentThread(mJavaVM, &env, NULL);
	int status = (mJavaVM)->AttachCurrentThread(&env, NULL);
	if (status < 0) {
		return 0;
	}

	/* From http://developer.android.com/guide/practices/jni.html
	* Threads attached through JNI must call DetachCurrentThread before they exit. If coding this directly is awkward,
	* in Android 2.0 (Eclair) and higher you can use pthread_key_create to define a destructor function that will be
	* called before the thread exits, and call DetachCurrentThread from there. (Use that key with pthread_setspecific
	* to store the JNIEnv in thread-local-storage; that way it'll be passed into your destructor as the argument.)
	* Note: The destructor is not called unless the stored value is != NULL
	* Note: You can call this function any number of times for the same thread, there's no harm in it
	*       (except for some lost CPU cycles)
	*/
	pthread_setspecific(mThreadKey, (void*)env);

	return env;
}
//
//int OsWrapperAndroid::get_window_from_surface(int surfacePtr)
//{
//	LOG_INFO("get_window_from_surface: %X", surfacePtr);
//	LOG_INFO("get_window_from_surface: activity: %X", get_osWrapper()->activity);
//	auto env = Android_JNI_GetEnv();
//	LOG_INFO("get_window_from_surface: env: %X", env);
//	return reinterpret_cast<int>(ANativeWindow_fromSurface(env, (jobject)surfacePtr));
//}

std::string OsWrapperAndroid::get_app_path()
{
	return std::string(activity->obbPath) + "/";
}