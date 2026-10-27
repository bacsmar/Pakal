#pragma once
#include "Config.h"
#include <thread>
#include <atomic>
#include <chrono>

#include "ISystem.h"
#include "AsyncTaskDispatcher.h"
#include "FPSCounter.h"
#include "Clock.h"


namespace Pakal
{
	class _PAKALExport System : public ISystem
	{
		std::atomic<SystemState> m_state;// (SystemState::Created);
		std::thread*			m_thread = nullptr;
		std::chrono::nanoseconds	m_frame_period{0};

		bool					m_threaded;
		AsyncTaskDispatcher		m_dispatcher;
		FPSCounter				m_fps_counter;
		std::atomic_bool		m_dispatcher_ready;
		std::chrono::steady_clock::time_point m_next_frame{};

	protected:

		virtual void on_initialize() = 0;
		virtual void on_terminate() = 0;
		virtual void on_update(unsigned long dtMilliSeconds) = 0;
		virtual void on_pause() = 0;
		virtual void on_resume() = 0;

	private:

		void update_loop();
		void limit_fps();

	public:

		virtual ~System();
		explicit System(bool usesThread);

		inline std::thread::id thread_id() { return m_dispatcher.thread_id(); };
		inline bool is_threaded() override final { return m_threaded; };
		inline SystemState get_state() override final { return m_state; };
		inline unsigned get_fps() override final { return m_fps_counter.get_fps(); };
		inline void set_target_fps(unsigned target_fps) override final
		{
			m_frame_period = std::chrono::nanoseconds(target_fps ? 1000000000ll / target_fps : 0);
		};

		void update(unsigned long dtMilliSeconds) override final;

		BasicTaskPtr initialize() override final;
		BasicTaskPtr terminate() override final;
		BasicTaskPtr pause() override final;
		BasicTaskPtr resume() override final;

		BasicTaskPtr execute_block(const std::function<void()>& block) override final;
	};

}