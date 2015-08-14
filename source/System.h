#pragma once
#include "Config.h"
#include <thread>
#include <atomic>

namespace Pakal
{
	class _PAKALExport System
	{

	public:
		explicit System(bool useThread)
		{
			m_threaded = useThread;
			m_thread = nullptr;
			m_running = false;
		}
		virtual ~System()
		{
			if (m_running)
				terminate();
		}

		virtual void initialize()
		{
			ASSERT(!m_running);

			m_running = true;

			if (m_threaded)
			{
				m_thread = new std::thread(&System::loop, this);
			}
			else
			{
				on_initialize();
			}
		};

		void terminate()
		{
			ASSERT(m_running);
			m_running = false;

			if (m_threaded)
			{
				m_thread->join();
				SAFE_DEL(m_thread);
			}
			else
			{
				on_terminate();
			}
		}

	protected:
		virtual void on_initialize() = 0;
		virtual void on_terminate() = 0;
		virtual void on_update() = 0;
	
		volatile bool m_running;
	private:
		std::thread* m_thread;
		std::atomic_bool m_threaded;

		void loop()
		{
			on_initialize();
			while(m_running){ on_update();}
			on_terminate();
		}
	};
}