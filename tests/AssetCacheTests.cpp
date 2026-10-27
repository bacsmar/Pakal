///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: AssetCacheTests.cpp
// Purpose: The asset cache: one asset per file while it is held, decoded on a worker, uploaded on the
//          thread that pumps, failures reported, an asset nobody holds loaded again.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "AssetCache.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

namespace
{
	std::atomic<int> g_decodes{ 0 };
	std::thread::id g_uploadThread;

	// Decoded only: its "file" is its name; "bad" fails
	struct Text
	{
		std::string value;
		static std::unique_ptr<Text> decode(const std::string& path)
		{
			++g_decodes;
			if (path == "bad") return nullptr;
			auto text = std::make_unique<Text>();
			text->value = "decoded " + path;
			return text;
		}
	};

	// Decoded, then uploaded on the pumping thread
	struct Gpu
	{
		bool uploaded = false;
		static std::unique_ptr<Gpu> decode(const std::string&) { return std::make_unique<Gpu>(); }
		bool upload()
		{
			g_uploadThread = std::this_thread::get_id();
			uploaded = true;
			return true;
		}
	};

	template <class T>
	bool wait_for(const Pakal::AssetHandle<T>& asset, Pakal::AssetCache& cache)
	{
		for (int i = 0; i < 400 && asset->state() == Pakal::AssetState::Loading; ++i)
		{
			cache.pump(5.0);
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		return asset->state() != Pakal::AssetState::Loading;
	}
}

int main()
{
	int failures = 0;
	auto check = [&](bool ok, const char* message)
	{
		std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", message);
		if (!ok) ++failures;
	};

	Pakal::AssetCache cache(2);
	{
		auto a = cache.load<Text>("a.txt");
		auto again = cache.load<Text>("a.txt");
		check(a == again, "a second load of the same file is the same asset");
		check(wait_for(a, cache) && a->ready() && a->get() && a->get()->value == "decoded a.txt", "it is decoded on a worker and ready");
		check(g_decodes == 1, "and decoded once");
	}
	{
		auto bad = cache.load<Text>("bad");
		check(wait_for(bad, cache) && bad->failed() && bad->get() == nullptr, "a file that does not decode fails");
	}
	{
		g_decodes = 0;
		cache.load<Text>("b.txt"); // nobody keeps it
		for (int i = 0; i < 400 && cache.pending() > 0; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(5));
		auto b = cache.load<Text>("b.txt");
		wait_for(b, cache);
		check(g_decodes == 2, "an asset nobody held is loaded again");
	}
	{
		auto gpu = cache.load<Gpu>("model");
		std::this_thread::sleep_for(std::chrono::milliseconds(50)); // decoded by now, but nothing pumped
		check(gpu->state() == Pakal::AssetState::Loading, "an asset with an upload waits for pump()");
		check(wait_for(gpu, cache) && gpu->ready() && gpu->get()->uploaded, "pump() uploads it, and then it is ready");
		check(g_uploadThread == std::this_thread::get_id(), "the upload runs on the thread that pumps");
		check(cache.pending() == 0 && cache.pump(1.0), "nothing is left pending");
	}
	{
		auto text = cache.load<Text>("same");
		auto gpu = cache.load<Gpu>("same");
		check(static_cast<void*>(text.get()) != static_cast<void*>(gpu.get()), "two types of asset of one file are two assets");
		wait_for(text, cache);
		wait_for(gpu, cache);
	}
	cache.trim();

	std::printf("%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
	return failures == 0 ? 0 : 1;
}
