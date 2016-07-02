
#ifdef __ANDROID__

#include <android/window.h>
#include <android/native_activity.h>
#include <android/configuration.h>
#include <cstdio>

namespace sf
{
	void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize);
}

void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize)
{
	sf::ANativeActivity_onCreate(activity, savedState, savedStateSize);
}

#endif