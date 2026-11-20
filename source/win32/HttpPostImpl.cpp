///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: HttpPostImpl.cpp (win32)
// Purpose: A request with WinHTTP (http and https, the system's proxy and certificates)
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "HttpPost.h"

#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")

namespace Pakal
{
	bool platform_open_url(const std::string& url)
	{
		if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) return false;   // (a web address only, never a program)
		return reinterpret_cast<intptr_t>(ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
	}

	int platform_http_post(const std::string& url, const HttpHeaders& headers, const std::string& body, int timeoutMs)
	{
		return platform_http_request("POST", url, headers, body, timeoutMs, nullptr);
	}

	// A request with WinHTTP (synchronous): the status, or -1; the answer's body read into 'response' when it is given
	int platform_http_request(const std::string& method, const std::string& url, const HttpHeaders& headers, const std::string& body, int timeoutMs, std::string* response)
	{
		if (response) response->clear();
		const std::wstring wide(url.begin(), url.end());
		URL_COMPONENTS parts = {};
		parts.dwStructSize = sizeof(parts);
		wchar_t host[256] = {}, path[2048] = {};
		parts.lpszHostName = host;
		parts.dwHostNameLength = 255;
		parts.lpszUrlPath = path;
		parts.dwUrlPathLength = 2047;
		wchar_t extra[2048] = {};
		parts.lpszExtraInfo = extra;
		parts.dwExtraInfoLength = 2047;
		if (!WinHttpCrackUrl(wide.c_str(), 0, 0, &parts)) return -1;
		const std::wstring object = std::wstring(path) + extra;
		int status = -1;
		HINTERNET session = WinHttpOpen(L"Pakal/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!session) return -1;
		WinHttpSetTimeouts(session, timeoutMs, timeoutMs, timeoutMs, timeoutMs);
		if (HINTERNET connection = WinHttpConnect(session, host, parts.nPort, 0))
		{
			const DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
			const std::wstring verb(method.begin(), method.end());
			if (HINTERNET request = WinHttpOpenRequest(connection, verb.c_str(), object.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags))
			{
				std::wstring header;
				for (const auto& [name, value] : headers) header += std::wstring(name.begin(), name.end()) + L": " + std::wstring(value.begin(), value.end()) + L"\r\n";
				if (WinHttpSendRequest(request, header.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : header.c_str(), header.empty() ? 0 : static_cast<DWORD>(-1L),
						body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()), static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0)
					&& WinHttpReceiveResponse(request, nullptr))
				{
					DWORD code = 0, size = sizeof(code);
					if (WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &size, WINHTTP_NO_HEADER_INDEX))
						status = static_cast<int>(code);
					// (the answer's body, as it comes)
					for (DWORD available = 0; response && WinHttpQueryDataAvailable(request, &available) && available > 0;)
					{
						const size_t at = response->size();
						response->resize(at + available);
						DWORD read = 0;
						if (!WinHttpReadData(request, response->data() + at, available, &read)) { response->resize(at); break; }
						response->resize(at + read);
						if (read == 0) break;
					}
				}
				WinHttpCloseHandle(request);
			}
			WinHttpCloseHandle(connection);
		}
		WinHttpCloseHandle(session);
		return status;
	}
}
