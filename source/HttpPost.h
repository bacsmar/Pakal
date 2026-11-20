///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: HttpPost.h
// Purpose: A request to a server, done by the platform (win32/HttpPostImpl.cpp: WinHTTP; unix/HttpPostImpl.cpp: none). OSManager's
//          http_post and http_request use it when the platform set no handler of its own (Android sets one: the activity's).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <string>
#include <utility>
#include <vector>

namespace Pakal
{
	using HttpHeaders = std::vector<std::pair<std::string, std::string>>;

	// The HTTP status, or -1 when it could not be sent (or this platform has no way). Blocks.
	int platform_http_post(const std::string& url, const HttpHeaders& headers, const std::string& body, int timeoutMs);
	// A web address opened in the browser (false: no way here)
	bool platform_open_url(const std::string& url);
	// Any method (GET, POST, PUT, DELETE): the HTTP status (or -1), and the answer's body in 'response' (when given). Blocks.
	int platform_http_request(const std::string& method, const std::string& url, const HttpHeaders& headers, const std::string& body, int timeoutMs, std::string* response);
}
