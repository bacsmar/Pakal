///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: BuildIdentity.h
// Purpose: Which copy of a game is running: its version, build, commit and channel (docs/BUILD_IDENTITY.md). The packers
//          write it in the package (Assets/build.json); a desktop build writes it next to the executable (build.json).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Config.h"
#include <string>

namespace Pakal
{
	// Where the build's label is drawn: the corner of the game's version text, never over a button (C-ID-7)
	enum class BuildLabelCorner { TopLeft, TopRight, BottomLeft, BottomRight };

	struct _PAKALExport BuildIdentity
	{
		std::string edition;   // what the copy is when a game has more than one (a flavor: "demo mvp"); empty otherwise
		std::string version;   // the game's version ("0.1.0-beta.1"); empty when the build does not know it
		std::string channel;   // "dev", "qa" or "store"; empty for a production build
		std::string commit;    // the short commit it was built from
		std::string branch;
		int build = 0;         // the build number
		int code = 0;          // the store's version code
		bool dirty = false;    // built from uncommitted changes
		// Production: where a store bundle asks, at start, whether it was promoted to production (its label then hidden). Empty: never
		std::string label_flag_url;
		bool known = false;    // read from a build.json

		// The label: "<edition> v<version>-<channel>  <build>.<commit>[.dirty]"; empty for production (no channel)
		std::string label() const;
		// One line for the log
		std::string describe() const;

		// From build.json's text; an empty or broken text gives an unknown dev build
		static BuildIdentity parse(const std::string& json);
		// The running copy's: the package's Assets/build.json, else build.json next to the executable, else an unknown dev build
		static BuildIdentity load();
	};

	// Production: what a label flag answer says about a bundle: {"hide_label": [531, 532]} lists the version codes promoted to
	// production. True when 'code' is in it; false for any other answer (a broken one included: the label stays)
	_PAKALExport bool label_hidden_by_flag(const std::string& answer, int code);
}
