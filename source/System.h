#pragma once
#include "Config.h"
#include <thread>
#include <atomic>

#include "ISystem.h"
#include "AsyncTaskDispatcher.h"
#include "FPSCounter.h"


namespace Pakal
{
	class _PAKALExport System : public ISystem
	{
		std::atomic<SystemState> m_state;
		std::thread*			m_thread;
		bool					m_threaded;
		AsyncTaskDispatcher		m_dispatcher;
		FPSCounter				m_fps_counter;

		std::atomic_bool		m_dispatcher_ready;

	protected:

		virtual void on_initialize() = 0;
		virtual void on_terminate() = 0;
		virtual void on_update(long long dt) = 0;
		virtual void on_pause() = 0;
		virtual void on_resume() = 0;

	private:

		void update_loop();

	public:

		virtual ~System();
		explicit System(bool usesThread);

		inline std::thread::id thread_id() { return m_dispatcher.thread_id(); };
		inline bool is_threaded() override final { return m_threaded; };
		inline SystemState get_state() override final { return m_state; };
		inline unsigned get_fps() override final { return m_fps_counter.get_fps(); };

		void update(long long dt) override final;

		BasicTaskPtr initialize() override final;
		BasicTaskPtr terminate() override final;
		BasicTaskPtr pause() override final;
		BasicTaskPtr resume() override final;

		BasicTaskPtr execute_block(const std::function<void()>& block);
	};

}