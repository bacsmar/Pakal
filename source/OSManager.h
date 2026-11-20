///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 The PakalEngine Open Source Project
// File: ComponentSystem.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 13 jun 2015
// Purpose: Class to control creation IComponents using factories.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Config.h"
#include "PakalMath.h"
#include "Event.h"
#include "EventArgs.h"
#include "TaskCompletionSource.h"

#include "HttpPost.h"

#include <atomic>
#include <functional>
#include <string>
#include <vector>


namespace Pakal
{	
	class IInputManager;

	class OSManager
	{
		template <class OSManager> friend class SingletonHolder;

	public:
		struct IWindowManager
		{
			virtual WindowArgs setup_window(unsigned windowId, const tmath::vector2di& dimensions, bool fullscreen, unsigned int bitsPerPixel) = 0;
			virtual void close_window() = 0;
			virtual void process_window_events() = 0;
			virtual void flush() {};
			virtual void set_title(const std::string& title) = 0;
			virtual tmath::vector2di get_position() = 0;
			virtual tmath::vector2di get_size() = 0;
			virtual ~IWindowManager() {}
		};
		struct Settings
		{
			std::function<IInputManager*(OSManager*)> input_manager_allocator;
			std::function<IWindowManager*(OSManager*)> window_manager_allocator;
		};
	private:

		IWindowManager*	m_window_impl;
		IInputManager*  m_input_manager;
		TaskCompletionSource<WindowArgs> m_windows_setup_task;
		std::string m_dataFolder = "./";
		std::string m_language;
		std::function<bool(const std::string&, const std::string&)> m_share;
		std::function<bool(const std::string&)> m_openUrl;
		std::function<bool(const std::string&, const std::string&)> m_shareText;
		std::function<std::string()> m_takeLink;
		std::string m_link;
	public:
		using HttpHeaders = Pakal::HttpHeaders;
	private:
		std::function<int(const std::string&, const HttpHeaders&, const std::string&, int)> m_post;
		std::function<int(const std::string&, const std::string&, const HttpHeaders&, const std::string&, int, std::string*)> m_request;
		std::vector<std::string> m_appSigners;
		std::atomic<bool> m_closeRequested{ false };

	protected:
		virtual ~OSManager() {}
		OSManager() = default;
	public:
		static OSManager& instance();

		Event<WindowArgs>	event_window_created;
		Event<WindowArgs>	event_window_destroyed;
		Event<WindowArgs>	event_window_redraw_needed;
		Event<WindowArgs>	event_window_resized;
		Event<bool>		event_window_focused;
		Event<void>		event_app_finished;
		Event<void>		event_app_paused;
		Event<void>		event_app_resumed;
		Event<void>		event_app_started;
		Event<void>		event_app_stoped;

		inline IInputManager* get_input_manager() const { return m_input_manager; };
		inline IWindowManager* get_window_manager() const { return m_window_impl;  };
		inline virtual std::string get_app_path() { return "./";  };

		// Where the program may write files for the person to take away (a profiling session): the working folder, or
		// what the platform sets (Android: the app's external files folder). With a separator at the end.
		std::string data_folder() const { return m_dataFolder; }
		void set_data_folder(const std::string& folder) { m_dataFolder = folder; }
		// The language the device is set to, as two lowercase letters ("en", "es"), set by the platform (Android: the activity's
		// configuration; Windows: the user's locale); empty when it is not known. A game chooses its texts with it.
		const std::string& language() const { return m_language; }
		void set_language(const std::string& language) { m_language = language; }
		// Hands a file to the person: the platform's "share" (Android: the share sheet, e-mail, chat), set by the platform
		// with set_share_handler; elsewhere it is only logged (the file is where it was written). False when nothing
		// took it
		bool share_file(const std::string& path, const std::string& title);
		void set_share_handler(std::function<bool(const std::string& path, const std::string& title)> handler) { m_share = std::move(handler); }
		// Opens a web address in the person's browser (Android: the activity's, set_open_url_handler; Windows: the shell's). False when
		// nothing could open it
		bool open_url(const std::string& url);
		void set_open_url_handler(std::function<bool(const std::string& url)> handler) { m_openUrl = std::move(handler); }
		// Hands a text to the person's apps (a message with a link: Android's share sheet, set_share_text_handler); elsewhere it is
		// only logged. False when nothing took it
		bool share_text(const std::string& text, const std::string& title);
		void set_share_text_handler(std::function<bool(const std::string& text, const std::string& title)> handler) { m_shareText = std::move(handler); }
		// The link that opened the program (Android: a VIEW intent of the game's android.json "links", or one that reached it while it
		// ran; set_link_handler), once: empty until another comes. offer_link puts one by hand (a desktop's command line, a test)
		std::string take_link();
		void offer_link(const std::string& link) { m_link = link; }
		void set_link_handler(std::function<std::string()> handler) { m_takeLink = std::move(handler); }
		// Sends 'body' to 'url' (http or https) with a POST and these headers (Content-Type among them) and waits for the answer: its
		// HTTP status (200, 201, 302...), or -1 when it could not be sent (no network, no way on this platform). It blocks: call it from
		// a thread of its own. Android: the activity's (PakalActivity.postBytes, set by the platform with set_http_post_handler);
		// otherwise the platform's own (win32/HttpPostImpl.cpp: WinHTTP; unix/HttpPostImpl.cpp: none, -1).
		int http_post(const std::string& url, const HttpHeaders& headers, const std::string& body, int timeoutMs = 15000);
		void set_http_post_handler(std::function<int(const std::string& url, const HttpHeaders& headers, const std::string& body, int timeoutMs)> handler) { m_post = std::move(handler); }
		// Any request (GET, POST...) to 'url' with these headers and body, waiting for the answer: its HTTP status (or -1), and the
		// answer's body in 'response' (a JSON, a file to save). It blocks: call it from a thread of its own. Android: the activity's
		// (PakalActivity.requestBytes, set_http_request_handler); otherwise the platform's (win32: WinHTTP; unix: none, -1)
		int http_request(const std::string& method, const std::string& url, const HttpHeaders& headers, const std::string& body, std::string* response, int timeoutMs = 20000);
		using HttpRequestHandler = std::function<int(const std::string& method, const std::string& url, const HttpHeaders& headers, const std::string& body, int timeoutMs, std::string* response)>;
		void set_http_request_handler(HttpRequestHandler handler) { m_request = std::move(handler); }
		// The certificates the installed app is signed with, as SHA-256 in lowercase hex (Android: the package's signers, set by
		// the platform at start); empty where the platform has none (the desktop). A game can tell its own copy from one
		// signed again by someone else.
		const std::vector<std::string>& app_signers() const { return m_appSigners; }
		void set_app_signers(std::vector<std::string> signers) { m_appSigners = std::move(signers); }
		
		void initialize(const Settings& settings);
		void terminate();

		TaskPtr<WindowArgs> setup_window(unsigned windowId, const tmath::vector2di& dimensions, bool fullscreen, unsigned int bitsPerPixel);
		void flush_window();
		void close_window();

		// The window's events (and the synthetic input that is due, IInputManager::deliver_synthetic), on the thread that owns them
		void  process_window_events();
		// From any thread (the QA tools' "quit"): the game ends as when the person closes it, at the next process_window_events
		// (event_app_finished, on the thread that handles the window's events)
		void request_close() { m_closeRequested.store(true); }

		virtual void on_window_created(const WindowArgs& arg);
		virtual void on_window_destroyed(const WindowArgs& arg);
		
	};
}

#define OSMgr Pakal::OSManager::instance()