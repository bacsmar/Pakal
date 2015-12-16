#pragma once
#include "Config.h"
#include "TaskFwd.h"

namespace Pakal
{

	enum class _PAKALExport SystemState
	{
		Created,
		Running,
		Paused,
		Terminated,
	};


	class _PAKALExport ISystem
	{
	public:

		virtual ~ISystem(){ }

		virtual BasicTaskPtr initialize() = 0;
		virtual BasicTaskPtr terminate() = 0;
		virtual BasicTaskPtr resume() = 0;
		virtual BasicTaskPtr pause() = 0;
		virtual void		 update(long long dt) = 0;
		virtual unsigned     get_fps() = 0;
		virtual void set_target_fps(unsigned target_fps) = 0;

		virtual SystemState get_state() = 0;
		virtual bool is_threaded() = 0;
		virtual const char* get_system_name() = 0;
	};
}

