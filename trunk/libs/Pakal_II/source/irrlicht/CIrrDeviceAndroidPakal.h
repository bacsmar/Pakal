///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 PakalEngine
// File: ResourceManager.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 27 Agosto 2015
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "Config.h"

#ifndef PAKAL_ANDROID_PLATFORM
#pragma error("This file must be compiled in Android only");
#endif

//#pragma clang diagnostic ignored "-Winconsistent-missing-override"

#include <irrlicht.h>
#include <irrlicht/source/Irrlicht/CIrrDeviceStub.h>
#include <SIrrCreationParameters.h>

namespace Pakal
{
	class CIrrDeviceAndroidPakal : public irr::CIrrDeviceStub
	{
	private: 
		bool m_is_window_focused;
		bool m_is_window_active;
		bool m_is_window_minimized;	
		bool m_is_initialized;
		irr::video::SExposedVideoData ExposedVideoData;
	public:
		void yield() override;
		void sleep(irr::u32 timeMs, bool pauseTimer) override;
		void setWindowCaption(const wchar_t* text) override;
		bool isWindowActive() const override;
		bool isWindowFocused() const override;
		bool isWindowMinimized() const override;
		void setResizable(bool resize) override;
		void minimizeWindow() override;
		void maximizeWindow() override;
		void restoreWindow() override;
		irr::core::position2di getWindowPosition() override;
		irr::E_DEVICE_TYPE getType() const override;
	
		explicit CIrrDeviceAndroidPakal(const irr::SIrrlichtCreationParameters& param);
		~CIrrDeviceAndroidPakal() override;
		bool run() override;
		void closeDevice() override;

		void create_driver();
	};
}