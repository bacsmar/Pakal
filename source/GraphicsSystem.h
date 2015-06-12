#pragma once
#include "Config.h"

namespace Pakal
{
	
	class _PAKALExport GraphicsSystem
	{
	public:
		inline bool isInitialized() const { return m_Initialized; }

		virtual bool init();
		virtual void initWindow();
		virtual void restoreWindow();
		virtual void destroyWindow();

		virtual bool update();

		virtual void setWindowCaption(const char* caption){}
		virtual void showFps(bool val) { m_showFps = val; }

		virtual void beginScene();
		virtual bool draw(  );
		virtual void endScene();

		void run();
	protected:
		bool m_Initialized;
		bool m_showFps;
		friend class Engine;
		static GraphicsSystem* createGraphicsSystem();
		virtual ~GraphicsSystem(){}
	};
}