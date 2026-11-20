// ZipSourceMiniz: a sealed pack (tools/pakal_pack.py's format) read by an entry's name, and by that name under the resource
// manager's root, written either way ('/' or '\', any case): the game asks <root>/Assets/... on Android, where the other
// sources strip the root.
#include "ResourceManager.h"
#include "bgfx/ZipSourceMiniz.h"

#include <miniz.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <sstream>
#include <string>

namespace
{
	int g_failures = 0;

	void check(bool ok, const std::string& what)
	{
		std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
		if (!ok) ++g_failures;
	}

	// tools/pakal_pack.py's seal: "PKP1", the open size, then the deflated bytes mixed with the key's stream
	std::string seal(const std::string& data, const std::string& key, const std::string& name)
	{
		mz_ulong size = mz_compressBound(static_cast<mz_ulong>(data.size()));
		std::string body(size, '\0');
		mz_compress2(reinterpret_cast<unsigned char*>(body.data()), &size, reinterpret_cast<const unsigned char*>(data.data()),
			static_cast<mz_ulong>(data.size()), 9);
		body.resize(size);
		uint64_t h = 1469598103934665603ull;
		for (char c : key + "|" + name) { h ^= static_cast<unsigned char>(c); h *= 1099511628211ull; }
		uint64_t s = h ? h : 0x9E3779B97F4A7C15ull;
		for (size_t i = 0; i < body.size(); i += 8)
		{
			s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
			const uint64_t k = s * 2685821657736338717ull;
			for (size_t j = 0; j < 8 && i + j < body.size(); ++j) body[i + j] = static_cast<char>(body[i + j] ^ static_cast<char>((k >> (8 * j)) & 0xFF));
		}
		const uint32_t open = static_cast<uint32_t>(data.size());
		std::string out = "PKP1";
		for (int b = 0; b < 4; ++b) out += static_cast<char>((open >> (8 * b)) & 0xFF);
		return out + body;
	}

	std::string read_all(Pakal::ZipSourceMiniz& pack, const std::string& path)
	{
		auto stream = pack.open_resource(Pakal::Path(path.c_str()));
		if (!stream) return "<none>";
		return std::string(std::istreambuf_iterator<char>(*stream), std::istreambuf_iterator<char>());
	}
}

int main()
{
	namespace fs = std::filesystem;
	const fs::path root = fs::temp_directory_path() / "Pakal_ZipSourceTests";
	fs::create_directories(root);
	const std::string key = "test-key";
	const std::string name = "assets/game/hello.txt";
	const std::string text = "hello from the sealed pack";
	const fs::path packPath = root / "game.pak";

	mz_zip_archive zip{};
	const bool written = mz_zip_writer_init_file(&zip, packPath.string().c_str(), 0)
		&& mz_zip_writer_add_mem(&zip, name.c_str(), seal(text, key, name).data(), seal(text, key, name).size(), MZ_NO_COMPRESSION)
		&& mz_zip_writer_finalize_archive(&zip);
	mz_zip_writer_end(&zip);
	check(written, "a sealed pack written");

	std::string rootText = root.generic_string();
	ResourceMgr.set_root_path(rootText);
	Pakal::ZipSourceMiniz pack;
	check(pack.initialize(Pakal::Path(packPath.string().c_str()), true, key), "the pack mounted");

	check(read_all(pack, "Assets/game/hello.txt") == text, "read by its name");
	check(read_all(pack, rootText + "/Assets/game/hello.txt") == text, "read under the root, '/'");
	std::string upper = rootText;
	std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
	std::replace(upper.begin(), upper.end(), '/', '\\');
	check(read_all(pack, upper + "\\Assets\\game\\hello.txt") == text, "read under the root, upper case and '\\'");
	check(pack.has_resource(Pakal::Path((rootText + "/Assets/game/hello.txt").c_str())), "found under the root");
	check(!pack.has_resource(Pakal::Path((rootText + "x/Assets/game/hello.txt").c_str())), "a longer folder is not the root");

	ResourceMgr.set_root_path(".");
	check(read_all(pack, "Assets/game/hello.txt") == text, "root '.': read by its name");

	pack.terminate();
	std::error_code ignored;
	fs::remove_all(root, ignored);
	std::printf("%s\n", g_failures ? "FAILED" : "all passed");
	return g_failures ? 1 : 0;
}
