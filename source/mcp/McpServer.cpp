///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: McpServer.cpp
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "McpServer.h"

#include "LogMgr.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace Pakal
{
	namespace
	{
		using json = nlohmann::json;
		const char* const kProtocolVersion = "2024-11-05";
		const SOCKET kInvalidSocket = INVALID_SOCKET;

		std::string json_error(const json& id, int code, const std::string& message)
		{
			json response = { { "jsonrpc", "2.0" }, { "id", id }, { "error", { { "code", code }, { "message", message } } } };
			return response.dump();
		}

		// Reads one HTTP request: headers, then the body given by Content-Length
		bool read_request(SOCKET socket, std::string& head, std::string& body)
		{
			std::string data;
			char buffer[4096];
			size_t headerEnd = std::string::npos;
			while (headerEnd == std::string::npos)
			{
				const int received = recv(socket, buffer, sizeof(buffer), 0);
				if (received <= 0 || data.size() > 1024 * 1024)
				{
					return false;
				}
				data.append(buffer, static_cast<size_t>(received));
				headerEnd = data.find("\r\n\r\n");
			}
			head = data.substr(0, headerEnd);
			body = data.substr(headerEnd + 4);

			size_t length = 0;
			size_t at = 0;
			while ((at = head.find("\n", at)) != std::string::npos)
			{
				++at;
				const std::string line = head.substr(at, head.find('\n', at) - at);
				if (line.size() > 15 && _strnicmp(line.c_str(), "Content-Length:", 15) == 0)
				{
					length = static_cast<size_t>(std::strtoul(line.c_str() + 15, nullptr, 10));
				}
			}
			while (body.size() < length)
			{
				const int received = recv(socket, buffer, sizeof(buffer), 0);
				if (received <= 0)
				{
					return false;
				}
				body.append(buffer, static_cast<size_t>(received));
			}
			body.resize(length);
			return true;
		}

		bool has_bearer(const std::string& head, const std::string& token)
		{
			size_t at = 0;
			while ((at = head.find('\n', at)) != std::string::npos)
			{
				++at;
				const std::string line = head.substr(at, head.find('\n', at) - at);
				if (line.size() > 14 && _strnicmp(line.c_str(), "Authorization:", 14) == 0)
				{
					const std::string expected = " Bearer " + token;
					std::string value = line.substr(14);
					while (!value.empty() && (value.back() == '\r' || value.back() == ' ')) value.pop_back();
					return value == expected;
				}
			}
			return false;
		}

		void send_all(SOCKET socket, const std::string& text)
		{
			size_t sent = 0;
			while (sent < text.size())
			{
				const int n = send(socket, text.data() + sent, static_cast<int>(text.size() - sent), 0);
				if (n <= 0)
				{
					return;
				}
				sent += static_cast<size_t>(n);
			}
		}
	}

	McpServer::~McpServer()
	{
		stop();
	}

	void McpServer::add_tool(const std::string& name, Tool tool)
	{
		m_tools[name] = std::move(tool);
	}

	bool McpServer::start(unsigned short port, const std::string& token, std::string& error)
	{
		if (m_running.load())
		{
			return true;
		}
		WSADATA wsa;
		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		{
			error = "WSAStartup failed";
			return false;
		}

		const SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (listener == kInvalidSocket)
		{
			error = "cannot create the socket";
			return false;
		}
		BOOL reuse = TRUE;
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_port = htons(port);
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // never reachable from another machine
		if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || listen(listener, 4) != 0)
		{
			closesocket(listener);
			error = "cannot listen on 127.0.0.1:" + std::to_string(port);
			return false;
		}

		m_token = token;
		m_listenSocket = static_cast<uintptr_t>(listener);
		m_running.store(true);
		m_thread = std::thread([this]() { serve(); });
		LOG_INFO("[McpServer] listening on http://127.0.0.1:%u/mcp (%zu tools)%s", static_cast<unsigned>(port),
			m_tools.size(), m_token.empty() ? "" : ", token required");
		return true;
	}

	void McpServer::stop()
	{
		if (!m_running.exchange(false))
		{
			return;
		}
		if (m_thread.joinable())
		{
			m_thread.join();
		}
		if (m_listenSocket != ~static_cast<uintptr_t>(0))
		{
			closesocket(static_cast<SOCKET>(m_listenSocket));
			m_listenSocket = ~static_cast<uintptr_t>(0);
		}
		WSACleanup();
	}

	void McpServer::serve()
	{
		const SOCKET listener = static_cast<SOCKET>(m_listenSocket);
		while (m_running.load())
		{
			fd_set readable;
			FD_ZERO(&readable);
			FD_SET(listener, &readable);
			timeval timeout{ 0, 200000 }; // 200 ms: lets stop() take effect quickly
			if (select(0, &readable, nullptr, nullptr, &timeout) <= 0)
			{
				continue;
			}
			const SOCKET client = accept(listener, nullptr, nullptr);
			if (client == kInvalidSocket)
			{
				continue;
			}
			handle_connection(static_cast<uintptr_t>(client));
			closesocket(client);
		}
	}

	void McpServer::handle_connection(uintptr_t socketValue)
	{
		const SOCKET socket = static_cast<SOCKET>(socketValue);
		std::string head, body;
		if (!read_request(socket, head, body))
		{
			return;
		}
		const bool isPost = head.rfind("POST /mcp", 0) == 0;
		if (!isPost)
		{
			send_all(socket, "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
			return;
		}
		if (!m_token.empty() && !has_bearer(head, m_token))
		{
			send_all(socket, "HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
			return;
		}

		const std::string response = handle_message(body);
		if (response.empty())
		{
			send_all(socket, "HTTP/1.1 202 Accepted\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
			return;
		}
		// Chunked transfer coding: one chunk with the whole body, then the last-chunk. Valid HTTP/1.1, and
		// it does not depend on a Content-Length the path to the client could rewrite.
		char size[32];
		std::snprintf(size, sizeof(size), "%zx", response.size());
		const std::string message = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nTransfer-Encoding: chunked\r\n"
			"Connection: close\r\n\r\n" + std::string(size) + "\r\n" + response + "\r\n0\r\n\r\n";
		send_all(socket, message);
	}

	std::string McpServer::handle_message(const std::string& text)
	{
		json request = json::parse(text, nullptr, false);
		if (request.is_discarded() || !request.is_object())
		{
			return json_error(nullptr, -32700, "parse error");
		}

		const json id = request.value("id", json());
		const std::string method = request.value("method", std::string());
		const bool isNotification = !request.contains("id");

		if (method == "notifications/initialized" || (isNotification && method.rfind("notifications/", 0) == 0))
		{
			return std::string();
		}

		try
		{
			if (method == "initialize")
			{
				json result = {
					{ "protocolVersion", kProtocolVersion },
					{ "capabilities", { { "tools", json::object() } } },
					{ "serverInfo", { { "name", "pakal" }, { "version", "0.1" } } }
				};
				return json({ { "jsonrpc", "2.0" }, { "id", id }, { "result", result } }).dump();
			}
			if (method == "tools/list")
			{
				json tools = json::array();
				for (const auto& entry : m_tools)
				{
					tools.push_back({ { "name", entry.first }, { "description", entry.second.description },
						{ "inputSchema", entry.second.inputSchema } });
				}
				return json({ { "jsonrpc", "2.0" }, { "id", id }, { "result", { { "tools", tools } } } }).dump();
			}
			if (method == "tools/call")
			{
				const json params = request.value("params", json::object());
				const std::string name = params.value("name", std::string());
				const auto found = m_tools.find(name);
				if (found == m_tools.end())
				{
					return json_error(id, -32602, "unknown tool '" + name + "'");
				}
				const json arguments = params.value("arguments", json::object());
				json output;
				bool isError = false;
				try
				{
					output = found->second.call(arguments);
				}
				catch (const std::exception& e)
				{
					output = { { "error", e.what() } };
					isError = true;
				}
				json result = {
					{ "content", json::array({ { { "type", "text" }, { "text", output.dump(2) } } }) },
					{ "isError", isError }
				};
				return json({ { "jsonrpc", "2.0" }, { "id", id }, { "result", result } }).dump();
			}
			return json_error(id, -32601, "method not found: " + method);
		}
		catch (const std::exception& e)
		{
			return json_error(id, -32603, e.what());
		}
	}
}
