///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: MusicMixer.cpp
// Purpose: Music: OGG Vorbis tracks decoded as they play - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "MusicMixer.h"

#include <algorithm>
#include <cmath>

// stb_vorbis (from vcpkg's stb, on the desktop and on Android); without it the music is silent
#if __has_include(<stb_vorbis.c>)
#define PAKAL_MUSIC_VORBIS 1
#include <stb_vorbis.c>
#else
#define PAKAL_MUSIC_VORBIS 0
#endif

namespace Pakal
{
	namespace
	{
		const size_t kMaxDecks = 4;
		const int kChunkFrames = 1024;
	}

	struct MusicMixer::Deck
	{
		std::shared_ptr<const std::vector<uint8_t>> bytes;
		MusicSettings settings;
#if PAKAL_MUSIC_VORBIS
		stb_vorbis* vorbis = nullptr;
#endif
		int channels = 2, rate = 48000;
		int64_t total = 0;     // the track's frames
		int64_t decoded = 0;   // the frames taken from the decoder
		float gain = 0.0f, target = 0.0f, slope = 0.0f;   // slope: gain per second toward the target
		bool spawned = false;  // its loop's next pass started
		bool ended = false;
		std::vector<float> chunk;
		int chunkFrames = 0, chunkAt = 0;
		float prev[2] = {}, next[2] = {};
		double fraction = 0.0;
		bool primed = false;

		~Deck()
		{
#if PAKAL_MUSIC_VORBIS
			if (vorbis) stb_vorbis_close(vorbis);
#endif
		}

		// The next frame of the track (a mono one on both sides); false past its end
		bool pull(float* frame)
		{
#if PAKAL_MUSIC_VORBIS
			if (chunkAt >= chunkFrames)
			{
				if (ended) return false;
				const int wanted = std::min(channels, 2);
				chunk.resize(static_cast<size_t>(wanted * kChunkFrames));
				int got = stb_vorbis_get_samples_float_interleaved(vorbis, wanted, chunk.data(), wanted * kChunkFrames);
				if (got <= 0 && settings.loop && settings.loopFadeSeconds <= 0.0f)
				{
					stb_vorbis_seek_start(vorbis);   // a loop without a crossfade jumps back to the start
					decoded = 0;
					got = stb_vorbis_get_samples_float_interleaved(vorbis, wanted, chunk.data(), wanted * kChunkFrames);
				}
				if (got <= 0)
				{
					ended = true;
					return false;
				}
				chunkFrames = got;
				chunkAt = 0;
			}
			const size_t at = static_cast<size_t>(chunkAt) * (channels >= 2 ? 2 : 1);
			frame[0] = chunk[at];
			frame[1] = channels >= 2 ? chunk[at + 1] : chunk[at];
			++chunkAt;
			++decoded;
			return true;
#else
			(void)frame;
			return false;
#endif
		}

		void fade_to(float to, float seconds)
		{
			target = to;
			slope = seconds > 0.0f ? std::max(std::abs(to - gain), 0.001f) / seconds : 1.0e9f;
		}
	};

	MusicMixer::MusicMixer() = default;
	MusicMixer::~MusicMixer() = default;

	std::unique_ptr<MusicMixer::Deck> MusicMixer::open(const std::shared_ptr<const std::vector<uint8_t>>& ogg, const MusicSettings& settings, float gain)
	{
#if PAKAL_MUSIC_VORBIS
		if (!ogg || ogg->empty()) return nullptr;
		int error = 0;
		stb_vorbis* vorbis = stb_vorbis_open_memory(ogg->data(), static_cast<int>(ogg->size()), &error, nullptr);
		if (!vorbis) return nullptr;
		auto deck = std::make_unique<Deck>();
		deck->bytes = ogg;
		deck->settings = settings;
		deck->vorbis = vorbis;
		const stb_vorbis_info info = stb_vorbis_get_info(vorbis);
		deck->channels = info.channels;
		deck->rate = static_cast<int>(info.sample_rate);
		deck->total = static_cast<int64_t>(stb_vorbis_stream_length_in_samples(vorbis));
		deck->gain = gain;
		return deck;
#else
		(void)ogg;
		(void)settings;
		(void)gain;
		return nullptr;
#endif
	}

	bool MusicMixer::play(std::shared_ptr<const std::vector<uint8_t>> ogg, const MusicSettings& settings)
	{
		auto deck = open(ogg, settings, settings.fadeSeconds > 0.0f ? 0.0f : settings.gain);
		if (!deck) return false;
		deck->fade_to(settings.gain, settings.fadeSeconds);
		std::lock_guard<std::mutex> lock(m_mutex);
		for (auto& playing : m_decks)
		{
			playing->settings.loop = false;   // it ends, and it does not start over
			playing->spawned = true;
			playing->fade_to(0.0f, settings.fadeSeconds);
		}
		if (m_decks.size() >= kMaxDecks) m_decks.erase(m_decks.begin());   // the oldest gives way
		m_decks.push_back(std::move(deck));
		return true;
	}

	void MusicMixer::stop(float fadeSeconds)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		for (auto& deck : m_decks)
		{
			deck->settings.loop = false;
			deck->spawned = true;
			deck->fade_to(0.0f, fadeSeconds);
		}
	}

	void MusicMixer::set_volume(float volume)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_volume = std::clamp(volume, 0.0f, 1.0f);
	}

	void MusicMixer::render(float* out, int frames, int rate)
	{
		if (rate <= 0) return;
		std::lock_guard<std::mutex> lock(m_mutex);
		std::vector<std::unique_ptr<Deck>> passes;   // loops starting over during this block
		for (auto& deck : m_decks)
		{
			Deck& d = *deck;
			if (!d.primed)
			{
				d.primed = true;
				if (!d.pull(d.prev) || !d.pull(d.next)) continue;
			}
			const double step = static_cast<double>(d.rate) / static_cast<double>(rate);
			const int64_t crossfadeFrames = static_cast<int64_t>(d.settings.loopFadeSeconds * static_cast<float>(d.rate));
			for (int i = 0; i < frames; ++i)
			{
				// a looping track near its end: the next pass fades in from the start while this one fades out
				if (d.settings.loop && !d.spawned && crossfadeFrames > 0 && d.total > crossfadeFrames * 2 && d.decoded >= d.total - crossfadeFrames)
				{
					d.spawned = true;
					if (auto pass = open(d.bytes, d.settings, 0.0f))
					{
						pass->fade_to(d.target, d.settings.loopFadeSeconds);
						passes.push_back(std::move(pass));
					}
					d.settings.loop = false;
					d.fade_to(0.0f, d.settings.loopFadeSeconds);
				}
				const float delta = d.slope / static_cast<float>(rate);
				d.gain = d.gain < d.target ? std::min(d.gain + delta, d.target) : std::max(d.gain - delta, d.target);
				const float gain = d.gain * m_volume;
				const float t = static_cast<float>(d.fraction);
				out[2 * i] += (d.prev[0] + (d.next[0] - d.prev[0]) * t) * gain;
				out[2 * i + 1] += (d.prev[1] + (d.next[1] - d.prev[1]) * t) * gain;
				d.fraction += step;
				while (d.fraction >= 1.0)
				{
					d.fraction -= 1.0;
					d.prev[0] = d.next[0];
					d.prev[1] = d.next[1];
					if (!d.pull(d.next))
					{
						d.next[0] = d.next[1] = 0.0f;
						d.ended = true;
						break;
					}
				}
			}
		}
		m_decks.erase(std::remove_if(m_decks.begin(), m_decks.end(),
			[](const std::unique_ptr<Deck>& d) { return d->ended || (d->target <= 0.0f && d->gain <= 0.0f); }), m_decks.end());
		for (auto& pass : passes)
		{
			if (m_decks.size() >= kMaxDecks) m_decks.erase(m_decks.begin());
			m_decks.push_back(std::move(pass));
		}
	}
}
