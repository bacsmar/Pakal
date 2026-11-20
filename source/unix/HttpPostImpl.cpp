///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: HttpPostImpl.cpp (unix)
// Purpose: No POST of its own here: Android sets the activity's (OSManager::set_http_post_handler); Linux has none yet
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "HttpPost.h"

namespace Pakal
{
	int platform_http_post(const std::string& url, const HttpHeaders& headers, const std::string& body, int timeoutMs)
	{
		(void)url; (void)headers; (void)body; (void)timeoutMs;
		return -1;
	}

	bool platform_open_url(const std::string& url)
	{
		(void)url;
		return false;
	}

	int platform_http_request(const std::string& method, const std::string& url, const HttpHeaders& headers, const std::string& body, int timeoutMs, std::string* response)
	{
		(void)method; (void)url; (void)headers; (void)body; (void)timeoutMs; (void)response;
		return -1;
	}
}
