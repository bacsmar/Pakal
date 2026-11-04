///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: McpClient.cpp
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "McpClient.h"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include <cstdlib>
#include <cstring>

namespace Pakal
{
	namespace
	{
		using json = nlohmann::json;
#if defined(_WIN32)
		using Socket = SOCKET;
		const Socket kNone = INVALID_SOCKET;
		void close_socket(Socket s) { closesocket(s); }
		const int kSendFlags = 0;
		struct Winsock
		{
			bool ok = false;
			Winsock() { WSADATA wsa; ok = WSAStartup(MAKEWORD(2, 2), &wsa) == 0; }
			~Winsock() { if (ok) WSACleanup(); }
		};
#else
		using Socket = int;
		const Socket kNone = -1;
		void close_socket(Socket s) { close(s); }
		const int kSendFlags = MSG_NOSIGNAL;   // a closed peer is an error, not SIGPIPE
#endif

		// The body of an HTTP response: after its headers, by Content-Length or chunked
		bool body_of(const std::string& response, std::string& body, std::string& error)
		{
			const size_t end = response.find("\r\n\r\n");
			if (end == std::string::npos)
			{
				error = "no answer";
				return false;
			}
			const std::string head = response.substr(0, end);
			if (head.compare(0, 12, "HTTP/1.1 200") != 0)
			{
				error = head.substr(0, head.find('\r'));
				return false;
			}
			const std::string rest = response.substr(end + 4);
			// whole, or chunked; a body that is JSON already is taken as it is (a local proxy, some antivirus web shields,
			// may rewrite the headers of loopback HTTP without the body)
			const size_t first = rest.find_first_not_of(" \r\n\t");
			if (head.find("chunked") == std::string::npos || (first != std::string::npos && rest[first] == '{'))
			{
				body = rest;
				return true;
			}
			body.clear();
			size_t at = 0;
			while (at < rest.size())
			{
				const size_t line = rest.find("\r\n", at);
				if (line == std::string::npos) break;
				const size_t size = std::strtoul(rest.substr(at, line - at).c_str(), nullptr, 16);
				if (size == 0) break;
				body += rest.substr(line + 2, size);
				at = line + 2 + size + 2;
			}
			return true;
		}
	}

	bool McpClient::rpc(const std::string& method, const json& params, json& result, std::string& error)
	{
#if defined(_WIN32)
		static Winsock winsock;
#endif
		const json request = { { "jsonrpc", "2.0" }, { "id", m_nextId++ }, { "method", method }, { "params", params } };
		const std::string body = request.dump();
		std::string message = "POST /mcp HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\n";
		if (!m_token.empty()) message += "Authorization: Bearer " + m_token + "\r\n";
		message += "Connection: close\r\n\r\n" + body;

		const Socket s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (s == kNone)
		{
			error = "no socket";
			return false;
		}
#if defined(_WIN32)
		const DWORD wait = static_cast<DWORD>(m_timeout * 1000.0);
		setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&wait), sizeof(wait));
		setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&wait), sizeof(wait));
#else
		timeval wait{ static_cast<long>(m_timeout), static_cast<long>((m_timeout - long(m_timeout)) * 1e6) };
		setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &wait, sizeof(wait));
		setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &wait, sizeof(wait));
#endif
		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_port = htons(m_port);
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		if (connect(s, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
		{
			close_socket(s);
			error = "nothing answers on 127.0.0.1:" + std::to_string(m_port);
			return false;
		}
		size_t sent = 0;
		while (sent < message.size())
		{
			const int n = send(s, message.data() + sent, static_cast<int>(message.size() - sent), kSendFlags);
			if (n <= 0) break;
			sent += static_cast<size_t>(n);
		}
		std::string response;
		char buffer[8192];
		for (;;)
		{
			const int n = recv(s, buffer, sizeof(buffer), 0);
			if (n <= 0) break;
			response.append(buffer, static_cast<size_t>(n));
		}
		close_socket(s);
		std::string text;
		if (!body_of(response, text, error)) return false;
		const json answer = json::parse(text, nullptr, false);
		if (!answer.is_object())
		{
			error = "not JSON";
			return false;
		}
		if (answer.contains("error"))
		{
			error = answer["error"].value("message", answer["error"].dump());
			return false;
		}
		result = answer.value("result", json());
		return true;
	}

	bool McpClient::list_tools(json& tools, std::string& error)
	{
		json result;
		if (!rpc("tools/list", json::object(), result, error)) return false;
		tools = result.value("tools", json::array());
		return true;
	}

	bool McpClient::call(const std::string& tool, const json& arguments, json& out, std::string& error)
	{
		json result;
		if (!rpc("tools/call", { { "name", tool }, { "arguments", arguments } }, result, error)) return false;
		const json content = result.value("content", json::array());
		const std::string text = !content.empty() ? content[0].value("text", std::string()) : std::string();
		out = json::parse(text, nullptr, false);
		if (out.is_discarded()) out = json();
		if (result.value("isError", false))
		{
			error = out.is_object() ? out.value("error", text) : text;
			return false;
		}
		return true;
	}
}
