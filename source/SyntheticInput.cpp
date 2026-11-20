///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: SyntheticInput.cpp
// Purpose: IInputManager's synthetic input (IInputManager.h): the queue the engine's QA tools fill and the
//          platform delivers where its own input arrives, and the keys, pad inputs and sticks it holds.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "IInputManager.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace Pakal
{
	// Axis device 0 as a pad's sticks (axes 0-1 the left one, 2-3 the right; y up), read from the synthetic sticks of the
	// first seat that pushes one
	class IInputManager::SyntheticSticks final : public IAxisDevice
	{
	public:
		explicit SyntheticSticks(const IInputManager& owner) : m_owner(owner) {}
		void set_device_id(unsigned) override {}
		unsigned get_device_id() const override { return 0; }
		float get_axis_state(unsigned axis) override
		{
			if (axis >= 4) return 0.0f;
			for (int slot = 0; slot < kPadSlots; ++slot)
			{
				for (unsigned a = 0; a < 4; ++a)
				{
					if (m_owner.synthetic_axis(slot, a) != 0.0f) return m_owner.synthetic_axis(slot, axis);
				}
			}
			return 0.0f;
		}
		void map_axis(unsigned, unsigned) override {}

	private:
		const IInputManager& m_owner;
	};

	IInputManager::~IInputManager()
	{
		delete m_synthetic_sticks;
	}

	long long IInputManager::synthetic_now_ms()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	unsigned long long IInputManager::queue_synthetic(const SyntheticEvent& event)
	{
		std::lock_guard<std::mutex> lock(m_synthetic_mutex);
		m_synthetic_queue.push_back(event);
		m_synthetic_numbers.push_back(++m_synthetic_queued);
		return m_synthetic_queued;
	}

	void IInputManager::deliver_synthetic(const tmath::vector2di& area)
	{
		// In the order they were queued: one that is not due yet holds back the ones after it (a tap's release, then what follows)
		std::vector<SyntheticEvent> due;
		unsigned long long last = 0;
		{
			std::lock_guard<std::mutex> lock(m_synthetic_mutex);
			if (m_synthetic_queue.empty()) return;
			const long long now = synthetic_now_ms();
			size_t count = 0;
			while (count < m_synthetic_queue.size() && m_synthetic_queue[count].due_ms <= now) ++count;
			if (count == 0) return;
			due.assign(m_synthetic_queue.begin(), m_synthetic_queue.begin() + static_cast<std::ptrdiff_t>(count));
			last = m_synthetic_numbers[count - 1];
			m_synthetic_queue.erase(m_synthetic_queue.begin(), m_synthetic_queue.begin() + static_cast<std::ptrdiff_t>(count));
			m_synthetic_numbers.erase(m_synthetic_numbers.begin(), m_synthetic_numbers.begin() + static_cast<std::ptrdiff_t>(count));
		}
		for (SyntheticEvent& event : due)
		{
			if (event.normalized)
			{
				event.x *= static_cast<float>(area.x);
				event.y *= static_cast<float>(area.y);
				event.normalized = false;
			}
			apply_synthetic_state(event);
			on_synthetic(event);
		}
		m_synthetic_delivered.store(last);
	}

	void IInputManager::apply_synthetic_state(const SyntheticEvent& event)
	{
		const int slot = std::clamp(event.slot, 0, kPadSlots - 1);
		const int key = static_cast<int>(event.key);
		switch (event.kind)
		{
		case SyntheticEvent::Kind::KeyDown:
		case SyntheticEvent::Kind::KeyUp:
			if (key >= 0 && key < static_cast<int>(Key::KeyCount)) m_synthetic_keys[key].store(event.kind == SyntheticEvent::Kind::KeyDown);
			break;
		case SyntheticEvent::Kind::PadDown:
			m_synthetic_pad[slot].fetch_or(1u << static_cast<unsigned>(event.pad));
			break;
		case SyntheticEvent::Kind::PadUp:
			m_synthetic_pad[slot].fetch_and(~(1u << static_cast<unsigned>(event.pad)));
			break;
		case SyntheticEvent::Kind::PadStick:
		{
			const unsigned base = event.stick == 0 ? 0u : 2u;
			m_synthetic_stick[slot][base].store(std::clamp(event.x, -1.0f, 1.0f));
			m_synthetic_stick[slot][base + 1].store(std::clamp(event.y, -1.0f, 1.0f));
			break;
		}
		default:
			break;
		}
	}

	void IInputManager::on_synthetic(const SyntheticEvent& event)
	{
		auto mouse = [&event](MouseButton button)
		{
			MouseArgs args{};
			args.button_id = button;
			args.x = static_cast<unsigned>(std::max(0.0f, event.x));
			args.y = static_cast<unsigned>(std::max(0.0f, event.y));
			args.delta = event.delta;
			return args;
		};
		switch (event.kind)
		{
		case SyntheticEvent::Kind::KeyDown:
		case SyntheticEvent::Kind::KeyUp:
		{
			KeyArgs args{};
			args.key = event.key;
			args.key_modifier = 0;
			if (event.kind == SyntheticEvent::Kind::KeyDown) event_key_down(args);
			else event_key_up(args);
			break;
		}
		case SyntheticEvent::Kind::MouseMove:
			event_mouse_moved(mouse(static_cast<MouseButton>(-1)));
			break;
		case SyntheticEvent::Kind::MouseDown:
			// a real click comes where the pointer already is: it moves there first
			event_mouse_moved(mouse(static_cast<MouseButton>(-1)));
			event_mouse_pressed(mouse(event.button));
			break;
		case SyntheticEvent::Kind::MouseUp:
			event_mouse_released(mouse(event.button));
			break;
		case SyntheticEvent::Kind::MouseWheel:
			event_mouse_wheel(mouse(static_cast<MouseButton>(-1)));
			break;
		// a touch, as SFML gives one on the desktop: the mouse, its button the finger
		case SyntheticEvent::Kind::TouchDown:
			event_mouse_moved(mouse(static_cast<MouseButton>(-1)));
			event_mouse_pressed(mouse(static_cast<MouseButton>(event.finger)));
			break;
		case SyntheticEvent::Kind::TouchMove:
			event_mouse_moved(mouse(static_cast<MouseButton>(event.finger)));
			break;
		case SyntheticEvent::Kind::TouchUp:
			event_mouse_released(mouse(static_cast<MouseButton>(event.finger)));
			break;
		default:
			break;
		}
	}

	bool IInputManager::synthetic_key_held(Key key) const
	{
		const int index = static_cast<int>(key);
		return index >= 0 && index < static_cast<int>(Key::KeyCount) && m_synthetic_keys[index].load();
	}

	unsigned IInputManager::synthetic_pad_held(int slot) const
	{
		if (slot < 0 || slot >= kPadSlots) return 0;
		unsigned held = m_synthetic_pad[slot].load();
		// the left stick past half its way is a direction, as a real pad's is
		const float x = m_synthetic_stick[slot][0].load(), y = m_synthetic_stick[slot][1].load();
		if (x < -0.5f) held |= 1u << static_cast<unsigned>(PadInput::Left);
		if (x > 0.5f) held |= 1u << static_cast<unsigned>(PadInput::Right);
		if (y > 0.5f) held |= 1u << static_cast<unsigned>(PadInput::Up);
		if (y < -0.5f) held |= 1u << static_cast<unsigned>(PadInput::Down);
		return held;
	}

	float IInputManager::synthetic_axis(int slot, unsigned axis) const
	{
		if (slot < 0 || slot >= kPadSlots || axis >= 4) return 0.0f;
		return m_synthetic_stick[slot][axis].load();
	}

	bool IInputManager::synthetic_pressed(Key key, const std::vector<PadBinding>& bindings) const
	{
		if (synthetic_key_held(key)) return true;
		unsigned slots[kPadSlots], any = 0;
		for (int i = 0; i < kPadSlots; ++i) any |= slots[i] = synthetic_pad_held(i);
		if (!any) return false;
		for (const PadBinding& binding : bindings)
		{
			const unsigned held = binding.pad < 0 ? any : (binding.pad < kPadSlots ? slots[binding.pad] : 0u);
			if (binding.key == key && (held & (1u << static_cast<unsigned>(binding.input)))) return true;
		}
		return false;
	}

	IAxisDevice* IInputManager::synthetic_sticks() const
	{
		bool pushed = false;
		for (int slot = 0; slot < kPadSlots && !pushed; ++slot)
		{
			for (unsigned a = 0; a < 4; ++a) pushed = pushed || m_synthetic_stick[slot][a].load() != 0.0f;
		}
		if (!pushed) return nullptr;
		std::lock_guard<std::mutex> lock(m_synthetic_mutex);
		if (!m_synthetic_sticks) m_synthetic_sticks = new SyntheticSticks(*this);
		return m_synthetic_sticks;
	}
}
