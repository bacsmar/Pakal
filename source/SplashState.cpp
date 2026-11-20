#include "SplashState.h"
#include "Engine.h"
#include "IInputManager.h"
#include "IUIManager.h"
#include "LogMgr.h"

#include <algorithm>
#include <cstdlib>
#include <cstdio>

namespace Pakal
{
	SplashState::SplashState(Settings settings) : BaseGameState("SplashState"), m_settings(std::move(settings))
	{
		m_settings.fixed_seconds = std::clamp(m_settings.fixed_seconds, 1.0f, 2.0f);   // (a): 1 to 2 seconds
		m_settings.min_after_ready = std::max(m_settings.min_after_ready, 1.0f);
		m_settings.fade_seconds = std::max(m_settings.fade_seconds, 0.0f);
		if (m_settings.image.empty())
		{
			m_settings.image = kEngineLogo;
			if (m_settings.background.empty()) m_settings.background = kEngineBackground;
		}
		if (m_settings.background.empty()) m_settings.background = "#000000";
	}

	bool SplashState::skipped_by_environment()
	{
		const char* value = std::getenv("PAKAL_NO_SPLASH");
		return value && value[0] == '1';
	}

	void SplashState::on_initialize(Engine* engine)
	{
		m_engine = engine;
		if (skipped_by_environment())
		{
			LOG_INFO("[Splash] left out (PAKAL_NO_SPLASH)");
			m_fade = m_settings.fade_seconds;   // straight to the end, on the first update
			return;
		}
		IUIManager* ui = engine->get_ui_manager();
		std::string image;   // escaped for the markup (a path: no quotes expected)
		for (const char c : m_settings.image) if (c != '"' && c != '<' && c != '>' && c != ')') image += c;
		const std::string markup = "<rml><head><style>"
			"body { position: absolute; left: 0; top: 0; right: 0; bottom: 0; margin: 0; background-color: " + m_settings.background + "; }"
			"#splash { position: absolute; left: 0; top: 0; right: 0; bottom: 0; decorator: image(" + image + " contain); }"
			"</style></head><body id=\"body\"><div id=\"splash\"></div></body></rml>";
		m_loaded = ui && ui->load_document_from_text(m_settings.document_id, markup);
		if (m_loaded)
		{
			ui->show_document(m_settings.document_id);
			ui->bring_to_front(m_settings.document_id);
		}
		else
			LOG_WARNING("[Splash] no interface to show the logo on: it is left out");
		listen(engine);
		LOG_INFO("[Splash] %s (%s)", m_settings.image.c_str(), m_settings.ready ? "covers the load, skippable when ready" : "fixed, then the loading screen");
	}

	void SplashState::on_terminate(Engine* engine)
	{
		stop_listening();
		if (m_loaded)
		{
			if (IUIManager* ui = engine->get_ui_manager()) ui->unload_document(m_settings.document_id);
			m_loaded = false;
		}
	}

	void SplashState::listen(Engine* engine)
	{
		IInputManager* input = engine->input_manager();
		if (!input) return;
		m_key = input->event_key_down.add_listener([this](const KeyArgs&) { m_pressed = true; });
		m_touch = input->event_touch_began.add_listener([this](const TouchArgs&) { m_pressed = true; });
		m_mouse = input->event_mouse_pressed.add_listener([this](const MouseArgs&) { m_pressed = true; });
	}

	void SplashState::stop_listening()
	{
		IInputManager* input = m_engine ? m_engine->input_manager() : nullptr;
		if (!input) return;
		if (m_key) input->event_key_down.remove_listener(m_key);
		if (m_touch) input->event_touch_began.remove_listener(m_touch);
		if (m_mouse) input->event_mouse_pressed.remove_listener(m_mouse);
		m_key = m_touch = m_mouse = 0;
	}

	void SplashState::on_update(unsigned long dtMilliseconds)
	{
		if (m_finished) return;
		const double dt = std::min(dtMilliseconds, 100ul) / 1000.0;   // (a long first frame does not eat the logo's time)
		IInputManager* input = m_engine ? m_engine->input_manager() : nullptr;
		const bool pressed = m_pressed.exchange(false) || (input && input->any_pad_pressed());
		if (m_fade < 0.0)
		{
			m_shown += dt;
			if (!m_settings.ready)
			{
				// (a): fixed, never skipped (presses are swallowed)
				if (m_shown >= m_settings.fixed_seconds) m_fade = 0.0;
			}
			else
			{
				// (b): until the game is ready and min_after_ready more; a press skips it once both hold
				if (m_ready_at < 0.0 && m_settings.ready()) m_ready_at = m_shown;
				const bool ready = m_ready_at >= 0.0;
				const bool shownEnough = m_shown >= m_settings.min_after_ready;
				if (ready && (m_shown - m_ready_at >= m_settings.min_after_ready || (pressed && shownEnough))) m_fade = 0.0;
			}
			return;
		}
		m_fade += dt;
		if (m_loaded && m_settings.fade_seconds > 0.0f)
		{
			char opacity[16];
			std::snprintf(opacity, sizeof(opacity), "%.3f", std::max(0.0, 1.0 - m_fade / m_settings.fade_seconds));
			m_engine->get_ui_manager()->set_property(m_settings.document_id, "body", "opacity", opacity);
		}
		if (m_fade >= m_settings.fade_seconds) finish();
	}

	void SplashState::finish()
	{
		m_finished = true;
		stop_listening();
		if (m_loaded)
		{
			m_engine->get_ui_manager()->unload_document(m_settings.document_id);
			m_loaded = false;
		}
		LOG_INFO("[Splash] done after %.1f s", m_shown);
		if (m_settings.done) m_settings.done();
	}
}
