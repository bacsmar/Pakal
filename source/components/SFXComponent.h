#pragma once

#include "Config.h"
#include "TaskFwd.h"
#include "Component.h"
#include "PakalPath.h"
#include "SoundManager.h"

namespace Pakal
{
	class _PAKALExport SFXComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(SFXComponent, Component);
	public:

		virtual void initialize() = 0;
		virtual void terminate() = 0;
		
		inline bool add(unsigned id, const std::string& resourcePath) { return add(id, Path(resourcePath)); }
		virtual bool add(unsigned id, const Path& resourcePath) = 0;
		virtual bool add(unsigned id, SharedPtr<std::istream> resourceStream, const Path& resourceName) = 0;

		virtual void remove(unsigned id) = 0;

		inline void play(unsigned id) { play(id, SoundSettings()); }
		virtual void play(unsigned id,const SoundSettings& settings) = 0;
	};
}
