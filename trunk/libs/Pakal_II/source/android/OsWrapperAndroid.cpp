#include "OsWrapperAndroid.h"
#include <android/configuration.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>
#include "ResourceManager.h"

namespace Pakal
{
	OsWrapperAndroid::~OsWrapperAndroid()
	{
	}

	OsWrapperAndroid::OsWrapperAndroid() : activity(nullptr)
	{
	}

	std::string OsWrapperAndroid::get_app_path()
	{
		return std::string(activity->obbPath) + "/";
	}

	void onNativeWindowCreated(ANativeActivity* activity, ANativeWindow* window)
	{
		Pakal::WindowArgs e;
		e.windowId = (unsigned)window;
		e.size_x = ANativeWindow_getWidth(window);
		e.size_y = ANativeWindow_getHeight(window);
		LOG_INFO("onNativeWindowCreated %X %d %d", e.windowId, e.size_x, e.size_y);
		((Pakal::OsWrapperAndroid&)Pakal::OsWrapperAndroid::instance()).activity = activity;
		OSMgr.on_window_created(e);
	}
	void onNativeWindowDestroyed(ANativeActivity* activity, ANativeWindow* window)
	{
		Pakal::WindowArgs e;
		e.windowId = (unsigned)window;
		LOG_INFO("onNativeWindowDestroyed %X ", e.windowId);
		OSMgr.on_window_destroyed(e);
	}
}