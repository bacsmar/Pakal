#pragma once
#include "Config.h"
#include <thread>
#include <atomic>

#include "ISystem.h"
#include "AsyncTaskDispatcher.h"

namespace Pakal
{
	class _PAKALExport System : public ISystem
	{
		std::atomic<SystemState> m_state;
		std::thread*			m_thread;
		bool					m_threaded;
		AsyncTaskDispatcher		m_dispatcher;

		std::atomic_bool		m_dispatcher_ready;

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
		explicit System(bool usesThread);

		inline const std::thread::id& get_thread_id() { return m_dispatcher.thread_id(); };
		inline bool is_threaded() override final { return m_threaded; };
		inline SystemState get_state() override final { return m_state; };

		void update() override final;

		BasicTaskPtr initialize() override final;
		BasicTaskPtr terminate() override final;
		BasicTaskPtr pause() override final;
		BasicTaskPtr resume() override final;
	};

}