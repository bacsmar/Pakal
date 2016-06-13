#include "OsWrapperAndroid.h"
#include <android/configuration.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>
#include "ResourceManager.h"

using namespace Pakal;

//
//// PAKAL
namespace Pakal
{
}

OsWrapperAndroid::~OsWrapperAndroid()
{
}

OsWrapperAndroid::OsWrapperAndroid(): activity(nullptr)
{
}

std::string OsWrapperAndroid::get_app_path()
{
	return std::string(activity->obbPath) + "/";
}