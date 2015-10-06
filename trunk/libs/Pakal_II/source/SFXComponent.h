#pragma once

#include "Config.h"
#include "TaskFwd.h"
#include "Component.h"
#include "path.h"
#include "IStream.h"
#include "SoundManager.h"

namespace Pakal
{
	class _PAKALExport SFXComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(SFXComponent, Component);
	public:

		virtual void initialize() = 0;
		virtual void terminate() = 0;

		virtual bool add(unsigned id, const path& resourcePath) = 0;
		virtual bool add(unsigned id, SharedPtr<IStream> resourceStream) = 0;

		virtual void remove(unsigned id) = 0;

		virtual void play(unsigned id,const SoundSettings& settings = SoundSettings()) = 0;
	};
}
