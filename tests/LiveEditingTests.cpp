///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LiveEditingTests.cpp
// Purpose: The way another program works in an open level editor (LiveEditing): no editor until one beats;
//          a request is taken once and answered; one nobody takes is withdrawn (not done later); a closed
//          editor leaves nothing behind.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "LiveEditing.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace Pakal;

namespace
{
	int g_failures = 0;

	void check(bool condition, const char* what)
	{
		if (!condition)
		{
			++g_failures;
			std::printf("FAILED: %s\n", what);
		}
	}
}

int main()
{
	setvbuf(stdout, nullptr, _IONBF, 0);
	namespace fs = std::filesystem;
	const fs::path root = fs::temp_directory_path() / "pakal_live_editing_tests";
	fs::remove_all(root);
	fs::create_directories(root);
	const std::string level = (root / "level.json").string();
	std::ofstream(level) << "{}";

	check(!LiveEditing::editor_open(level), "no editor before one opens");
	Json reply;
	std::string error;
	check(!LiveEditing::ask(level, { { "query", "state" } }, reply, error, 0.5) && !error.empty(), "asking nobody fails at once");

	LiveEditing::Host host;
	check(host.open(level), "an editor takes requests for the level");
	host.beat({ { "unsaved", true } });
	Json state;
	check(LiveEditing::editor_open(level, &state) && state.value("unsaved", false), "its heartbeat says it is open, and what it shows");

	// the editor, answering what it takes (as its frames do)
	std::atomic<bool> running{ true };
	std::atomic<int> taken{ 0 };
	std::thread editor([&]()
	{
		while (running)
		{
			for (const auto& [id, request] : host.take_requests())
			{
				++taken;
				host.answer(id, { { "ok", true }, { "echo", request } });
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
	});
	const Json request = { { "ops", Json::array({ { { "op", "move" }, { "object", "a" }, { "dx", 1 }, { "dy", 0 } } }) } };
	check(LiveEditing::ask(level, request, reply, error, 3.0) && reply.value("ok", false) && reply["echo"] == request, "a request is answered");
	check(LiveEditing::ask(level, { { "undo", true } }, reply, error, 3.0) && taken == 2, "each one taken once");
	running = false;
	editor.join();

	// nobody takes it: withdrawn, so a late editor does not do it unseen
	check(!LiveEditing::ask(level, request, reply, error, 0.3) && error.find("not done") != std::string::npos, "a request not taken is withdrawn");
	check(host.take_requests().empty(), "and is not there for the editor later");

	host.close();
	check(!fs::exists(LiveEditing::folder_of(level)) && !LiveEditing::editor_open(level), "a closed editor leaves nothing");

	fs::remove_all(root);
	if (g_failures == 0) std::printf("LiveEditingTests: all passed\n");
	return g_failures == 0 ? 0 : 1;
}
