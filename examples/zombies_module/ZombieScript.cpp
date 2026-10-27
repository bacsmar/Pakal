///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ZombieScript.cpp
// Purpose: The level scripts of the Zombies game - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ZombieScript.h"

#include "LogMgr.h"

namespace Zombies
{
	using Pakal::ScriptArgs;
	using Pakal::ScriptValue;

	namespace
	{
		// The original game's script functions written in Lua over the bound ones (Wait and EnableSkip are the host's)
		const char* const kPrelude = R"lua(
			function ShowAutoDialog(text, ms)
				ms = ms or 3000
				__showAutoDialog(text, ms)
				Wait(ms)
			end
			-- The screen goes to a color / comes back from it; the script waits for the fade to end
			function FadeOut(ms, color) ms = ms or 0 __fade(true, ms, color or "ff000000") Wait(ms) end
			function FadeIn(ms, color) ms = ms or 0 __fade(false, ms, color or "ff000000") Wait(ms) end
			-- The title card of the scene (it does not hold the script)
			function ShowClapper(seconds, text) __clapper(text or "", (seconds or 3) * 1000) end
			function PlayMusic(name) __music(name or "") end
			function ResumeBackgroundMusic() __music("") end
			-- The game has no hidden hero, business-zombie variant or objective counter apart from its HUD: nothing to do
			function ShowPlayer() end
			function SetBusinessZombieType() end
			function SetObjectiveCounter() end
			-- Functions the level does not implement yet: the call is logged and does nothing
			setmetatable(_G, { __index = function(_, name)
				return function() __unimplemented(name) end
			end })
		)lua";

		int integer(const ScriptArgs& args, size_t i) { return static_cast<int>(args.number(i)); }
	}

	ZombieScript::ZombieScript(Host host) : m_host(std::move(host))
	{
		bind_functions();
		m_runtime.run(kPrelude);
	}

	ZombieScript::~ZombieScript() = default;

	void ZombieScript::bind_functions()
	{
		// A function the script calls that only forwards to the level
		auto bind = [this](const char* name, std::function<void(const ScriptArgs&)> call)
		{
			m_runtime.bind(name, [call](const ScriptArgs& args) { call(args); return ScriptValue(); });
		};
		Host& host = m_host;

		bind("__fade", [this, &host](const ScriptArgs& args)
		{
			if (host.fade) host.fade(args.truth(0), m_runtime.skipping() ? 0.0f : static_cast<float>(args.number(1)), args.text(2, "ff000000"));
		});
		bind("__clapper", [&host](const ScriptArgs& args)
		{
			if (host.showClapper) host.showClapper(args.text(0), static_cast<float>(args.number(1, 3000.0)));
		});
		bind("__music", [&host](const ScriptArgs& args)
		{
			if (host.playMusic) host.playMusic(args.text(0));
		});
		bind("__unimplemented", [&host](const ScriptArgs& args)
		{
			if (host.log) host.log("not implemented: " + args.text(0));
		});
		bind("SetEnemySpawn", [&host](const ScriptArgs& args)
		{
			if (host.setEnemySpawn) host.setEnemySpawn(integer(args, 0), integer(args, 1), static_cast<float>(args.number(2)));
		});
		bind("KillAllZombies", [&host](const ScriptArgs&) { if (host.killAllZombies) host.killAllZombies(); });
		bind("Done", [&host](const ScriptArgs&) { if (host.done) host.done(); });
		bind("DoneKillAll", [&host](const ScriptArgs&) { if (host.doneKillAll) host.doneKillAll(); });
		bind("__showAutoDialog", [&host](const ScriptArgs& args)
		{
			if (host.showDialog) host.showDialog(args.text(0), static_cast<float>(args.number(1, 3000.0)));
		});
		bind("SetObjectiveText", [&host](const ScriptArgs& args) { if (host.setObjectiveText) host.setObjectiveText(args.text(0)); });
		m_runtime.bind("GetObjectHealth", [&host](const ScriptArgs& args)
		{
			return ScriptValue::of(host.getObjectHealth ? host.getObjectHealth(integer(args, 0)) : -1.0);
		});
		for (const char* name : { "StartTimer", "ShowTimer" })
		{
			bind(name, [&host](const ScriptArgs& args) { if (host.startTimer) host.startTimer(static_cast<float>(args.number(0))); });
		}
		for (const char* name : { "StopTimer", "HideTimer" })
		{
			bind(name, [&host](const ScriptArgs&) { if (host.stopTimer) host.stopTimer(); });
		}
		bind("RemoveObjectiveText", [&host](const ScriptArgs&) { if (host.removeObjectiveText) host.removeObjectiveText(); });
		bind("WarpPlayer", [&host](const ScriptArgs& args) { if (host.warpPlayer) host.warpPlayer(integer(args, 0)); });
		bind("ShowObject", [&host](const ScriptArgs& args) { if (host.showObject) host.showObject(integer(args, 0), true); });
		bind("EnableObject", [&host](const ScriptArgs& args) { if (host.enableObject) host.enableObject(integer(args, 0), true); });
		bind("HideObject", [&host](const ScriptArgs& args) { if (host.showObject) host.showObject(integer(args, 0), false); });
		bind("DisableObject", [&host](const ScriptArgs& args) { if (host.enableObject) host.enableObject(integer(args, 0), false); });

		// EnableRandomObject(id1, id2, ..., enable, count): the source lists one or more object ids followed
		// by a boolean (enable) and a trailing count. We collect every leading integer as an id, read the first
		// boolean as the enabled flag and ignore the count (it is a selection hint, not a quantity of gifts).
		bind("EnableRandomObject", [&host](const ScriptArgs& args)
		{
			std::vector<int> ids;
			bool enabled = false;
			for (size_t i = 0; i < args.size(); ++i)
			{
				if (args.is_boolean(i)) { enabled = args.truth(i); break; }
				if (args.is_number(i)) ids.push_back(integer(args, i));
			}
			if (host.enableRandomObject) host.enableRandomObject(ids, enabled);
		});
		bind("DropPickup", [&host](const ScriptArgs& args)
		{
			if (host.dropPickup) host.dropPickup(integer(args, 0), integer(args, 1));
		});
		m_runtime.bind("IsPlayerNear", [&host](const ScriptArgs& args)
		{
			const float radius = static_cast<float>(args.number(1, 300.0));
			return ScriptValue::of_bool(host.isPlayerNear && host.isPlayerNear(integer(args, 0), radius));
		});
		bind("ShowMarker", [&host](const ScriptArgs& args)
		{
			const bool on = args.size() < 2 || args.truth(1);
			if (host.showMarker) host.showMarker(integer(args, 0), on);
		});
		bind("StartEscort", [&host](const ScriptArgs& args)
		{
			const std::string hero = args.is_text(0) ? args.text(0) : "";
			std::vector<int> points;
			for (size_t i = 1; i < args.size(); ++i)
			{
				if (args.is_number(i)) points.push_back(integer(args, i));
			}
			if (host.startEscort) host.startEscort(hero, points);
		});
		bind("Fail", [&host](const ScriptArgs& args)
		{
			if (host.fail) host.fail(args.is_text(0) ? args.text(0) : "");
		});
		bind("ReelObject", [&host](const ScriptArgs& args)
		{
			std::vector<int> ids;
			for (size_t i = 0; i < args.size(); ++i)
			{
				if (args.is_number(i)) ids.push_back(integer(args, i));
			}
			if (host.reelObject) host.reelObject(ids);
		});
		bind("EnableBerserk", [&host](const ScriptArgs& args) { if (host.enableBerserk) host.enableBerserk(args.truth(0)); });
		bind("ShakeCamera", [&host](const ScriptArgs& args)
		{
			if (host.shakeCamera) host.shakeCamera(static_cast<float>(args.number(0)), static_cast<float>(args.number(1, 0.0)));
		});
		bind("EnableControls", [&host](const ScriptArgs& args) { if (host.enableControls) host.enableControls(args.truth(0)); });
		bind("CameraSetManualPan", [&host](const ScriptArgs& args)
		{
			if (host.cameraPan) host.cameraPan(integer(args, 0), args.text(1), integer(args, 2), args.text(3), static_cast<float>(args.number(4)));
		});
		bind("CameraReturnToPlayer", [&host](const ScriptArgs&) { if (host.cameraReturnToPlayer) host.cameraReturnToPlayer(); });
	}
}
