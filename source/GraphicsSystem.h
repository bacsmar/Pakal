#pragma once
#include "Config.h"

namespace Pakal
{
	
	class _PAKALExport GraphicsSystem
	{
	public:
		inline bool isInitialized() const { return mInitialized; }

		virtual bool init();
		virtual void initWindow();
		virtual void restoreWindow();
		virtual void destroyWindow();

		virtual void setWindowCaption(const char* caption){}

		virtual void beginScene();
		virtual bool draw(float time);
		virtual void endScene();

	private:
		bool mInitialized;
		friend class Engine;
		static GraphicsSystem* createGraphicsSystem();
	protected:
		virtual ~GraphicsSystem(){}
	};
}