#pragma once
#include "Config.h"
#include <thread>
#include <atomic>
#include <condition_variable>

#include "ISystem.h"
#include "AsyncTaskDispatcher.h"

namespace Pakal
{
	class EventScheduler;



	class _PAKALExport System : public ISystem
	{
		std::atomic<SystemState> m_state;
		std::thread*			m_thread;
		EventScheduler*			m_scheduler;
		bool					m_threaded;
		AsyncTaskDispatcher		m_dispatcher;


		std::condition_variable	m_wait_condition;
		std::mutex				m_wait_mutex;
		std::atomic_bool		m_thread_ready;

	protected:

		virtual void on_initialize() = 0;
		virtual void on_terminate() = 0;
		virtual void on_update() = 0;
		virtual void on_pause() = 0;
		virtual void on_resume() = 0;

	private:

		void update_loop();		

	public:

		virtual ~System();
		explicit System(EventScheduler* scheduler, bool usesThread);

		inline EventScheduler* get_scheduler();
		inline std::thread::id get_thread_id();

		inline bool is_threaded() override final;
		inline SystemState get_state() override final;

		void update() override final;

		BasicTaskPtr initialize() override final;
		BasicTaskPtr terminate() override final;
		BasicTaskPtr pause() override final;
		BasicTaskPtr resume() override final;
	};

}