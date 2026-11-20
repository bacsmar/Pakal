///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Gzip.h
// Purpose: Bytes compressed as a gzip file (RFC 1952: what `gzip -d`, Python's gzip and a server's Content-Encoding read), with the
//          deflate the engine already carries (miniz)
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Platform.h"

#include <string>

namespace Pakal
{
	// The gzip file of 'data' (level 1..9: faster..smaller); empty when it could not be made
	_PAKALExport std::string gzip(const std::string& data, int level = 6);
}
