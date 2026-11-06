#include "SoundManagerAAudio.h"

#include "LogMgr.h"
#include "ResourceManager.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <dlfcn.h>
#include <fstream>

namespace Pakal
{
	namespace
	{
		const size_t kMaxVoices = 24;
		const size_t kRingFrames = 8192;   // ~0.17 s at 48 kHz: how far the music is decoded ahead
		const int kFeedFrames = 1024;

		uint16_t read16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
		uint32_t read32(const uint8_t* p) { return static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24)); }
	}

	SoundManagerAAudio::~SoundManagerAAudio()
	{
		terminate();
	}

	void SoundManagerAAudio::initialize()
	{
		if (!open_stream())
		{
			LOG_ERROR("[SoundManagerAAudio] the game plays without sound");
			return;
		}
		m_ring.assign(kRingFrames * 2, 0.0f);
		m_ringRead = 0;
		m_ringWrite = 0;
		m_feeding = true;
		m_feeder = std::thread([this] { feed_music(); });
		AAudioStream_requestStart(m_stream);
		LOG_INFO("[SoundManagerAAudio] output at %d Hz", m_sampleRate.load());
	}

	bool SoundManagerAAudio::open_stream()
	{
		AAudioStreamBuilder* builder = nullptr;
		if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK)
		{
			LOG_ERROR("[SoundManagerAAudio] no stream builder");
			return false;
		}
		AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
		AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
		AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
		AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
		AAudioStreamBuilder_setChannelCount(builder, 2);
		AAudioStreamBuilder_setDataCallback(builder, &SoundManagerAAudio::on_data, this);
		AAudioStreamBuilder_setErrorCallback(builder, &SoundManagerAAudio::on_error, this);
		// A session of its own keeps the stream off MMAP (the device written to directly, past the system's mixer): a
		// stereo low-latency stream got one, and screen recorders cannot capture it (the videos had no sound; the
		// recorder starting disconnected it). Android 9 and later; Android 8 has no MMAP
		using SetSessionId = void (*)(AAudioStreamBuilder*, aaudio_session_id_t);
		static const auto setSessionId = reinterpret_cast<SetSessionId>(dlsym(RTLD_DEFAULT, "AAudioStreamBuilder_setSessionId"));
		if (setSessionId) setSessionId(builder, AAUDIO_SESSION_ID_ALLOCATE);
		AAudioStream* stream = nullptr;
		const aaudio_result_t result = AAudioStreamBuilder_openStream(builder, &stream);
		AAudioStreamBuilder_delete(builder);
		if (result != AAUDIO_OK || !stream)
		{
			LOG_ERROR("[SoundManagerAAudio] the stream did not open (%s)", AAudio_convertResultToText(result));
			return false;
		}
		m_sampleRate = AAudioStream_getSampleRate(stream);
		m_stream = stream;
		return true;
	}

	void SoundManagerAAudio::close_stream()
	{
		AAudioStream* stream = m_stream.exchange(nullptr);
		if (!stream) return;
		// stopped before it is closed: Android 8's AAudio closed while still stopping let go of a binder object twice,
		// and the app crashed as it quit
		AAudioStream_requestStop(stream);
		aaudio_stream_state_t state = AAUDIO_STREAM_STATE_STOPPING;
		for (int i = 0; i < 4 && state != AAUDIO_STREAM_STATE_STOPPED && state != AAUDIO_STREAM_STATE_DISCONNECTED; ++i)
		{
			if (AAudioStream_waitForStateChange(stream, state, &state, 100 * 1000000LL) != AAUDIO_OK) break;
		}
		AAudioStream_close(stream);
	}

	void SoundManagerAAudio::reopen_stream()
	{
		std::lock_guard<std::mutex> lock(m_streamMutex);
		m_lost = false;
		close_stream();
		if (!open_stream()) return;   // tried again on the next resume
		if (!m_paused) AAudioStream_requestStart(m_stream);
		LOG_INFO("[SoundManagerAAudio] the output opened again at %d Hz", m_sampleRate.load());
	}

	void SoundManagerAAudio::terminate()
	{
		m_feeding = false;
		if (m_feeder.joinable()) m_feeder.join();
		{
			std::lock_guard<std::mutex> lock(m_streamMutex);
			close_stream();
		}
		std::lock_guard<std::mutex> lock(m_mutex);
		m_voices.clear();
	}

	std::shared_ptr<PcmSound> SoundManagerAAudio::load_wav(const std::string& path)
	{
		// from wherever the resource manager finds it (the app's storage, the APK)
		const auto shared = ResourceMgr.read_bytes(path);
		if (!shared) return nullptr;
		const std::vector<uint8_t>& bytes = *shared;
		if (bytes.size() < 44 || std::memcmp(bytes.data(), "RIFF", 4) != 0 || std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) return nullptr;

		int channels = 1, bits = 16, rate = 44100;
		bool pcm = false;
		const uint8_t* data = nullptr;
		size_t dataSize = 0;
		for (size_t at = 12; at + 8 <= bytes.size();)
		{
			const uint32_t size = read32(bytes.data() + at + 4);
			const uint8_t* body = bytes.data() + at + 8;
			if (std::memcmp(bytes.data() + at, "fmt ", 4) == 0 && size >= 16 && at + 8 + 16 <= bytes.size())
			{
				pcm = read16(body) == 1;
				channels = read16(body + 2);
				rate = static_cast<int>(read32(body + 4));
				bits = read16(body + 14);
			}
			else if (std::memcmp(bytes.data() + at, "data", 4) == 0)
			{
				data = body;
				dataSize = std::min<size_t>(size, bytes.size() - (at + 8));
				break;
			}
			at += 8 + size + (size & 1);
		}
		if (!pcm || !data || channels < 1 || (bits != 16 && bits != 8)) return nullptr;

		auto sound = std::make_shared<PcmSound>();
		sound->sampleRate = rate;
		const size_t frameBytes = static_cast<size_t>(channels) * (bits / 8);
		const size_t frames = dataSize / frameBytes;
		sound->samples.resize(frames);
		for (size_t i = 0; i < frames; ++i)
		{
			int sum = 0;
			for (int c = 0; c < channels; ++c)
			{
				const uint8_t* p = data + i * frameBytes + static_cast<size_t>(c) * (bits / 8);
				sum += bits == 16 ? static_cast<int16_t>(read16(p)) : (static_cast<int>(*p) - 128) * 256;
			}
			sound->samples[i] = static_cast<int16_t>(sum / channels);
		}
		return sound;
	}

	void SoundManagerAAudio::play(const std::shared_ptr<PcmSound>& sound, float gain)
	{
		if (!m_stream || !sound || sound->samples.empty() || gain <= 0.0f) return;
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_voices.size() >= kMaxVoices)
		{
			m_voices.erase(m_voices.begin()); // the oldest gives way
		}
		m_voices.push_back({ sound, 0.0, gain });
	}

	void SoundManagerAAudio::set_paused(bool paused)
	{
		{
			std::lock_guard<std::mutex> lock(m_streamMutex);
			m_paused = paused;
			if (m_stream && !m_lost)
			{
				const aaudio_result_t result = paused ? AAudioStream_requestPause(m_stream) : AAudioStream_requestStart(m_stream);
				if (result == AAUDIO_OK || paused) return;
				LOG_WARNING("[SoundManagerAAudio] start: %s", AAudio_convertResultToText(result));
			}
			if (paused) return;
		}
		// lost while in the background (another app took the output, the screen recorder changed its route), or it
		// never opened: a new one
		reopen_stream();
	}

	void SoundManagerAAudio::on_error(AAudioStream*, void* user, aaudio_result_t error)
	{
		// the stream is dead (its device changed or went away: headphones, a screen recording, a call). It cannot be
		// closed from this callback; the feeder opens a new one
		LOG_WARNING("[SoundManagerAAudio] the output was lost: %s", AAudio_convertResultToText(error));
		static_cast<SoundManagerAAudio*>(user)->m_lost = true;
	}

	bool SoundManagerAAudio::play_music(const std::string& path, const MusicSettings& settings)
	{
		const auto bytes = ResourceMgr.read_bytes(path);
		if (!m_stream || !bytes || !m_music.play(bytes, settings)) return false;
		m_musicOn = true;
		return true;
	}

	void SoundManagerAAudio::feed_music()
	{
		std::vector<float> block(static_cast<size_t>(kFeedFrames) * 2);
		while (m_feeding)
		{
			if (m_lost && !m_paused) reopen_stream();
			const size_t read = m_ringRead.load(std::memory_order_acquire);
			size_t write = m_ringWrite.load(std::memory_order_relaxed);
			if (!m_musicOn || write - read + kFeedFrames > kRingFrames)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
				continue;
			}
			std::fill(block.begin(), block.end(), 0.0f);
			m_music.render(block.data(), kFeedFrames, m_sampleRate);
			for (int i = 0; i < kFeedFrames; ++i, ++write)
			{
				const size_t at = (write % kRingFrames) * 2;
				m_ring[at] = block[static_cast<size_t>(i) * 2];
				m_ring[at + 1] = block[static_cast<size_t>(i) * 2 + 1];
			}
			m_ringWrite.store(write, std::memory_order_release);
		}
	}

	aaudio_data_callback_result_t SoundManagerAAudio::on_data(AAudioStream*, void* user, void* audio, int32_t frames)
	{
		static_cast<SoundManagerAAudio*>(user)->mix(static_cast<int16_t*>(audio), frames);   // stereo frames
		return AAUDIO_CALLBACK_RESULT_CONTINUE;
	}

	void SoundManagerAAudio::mix(int16_t* out, int32_t frames)
	{
		std::vector<float> accumulator(static_cast<size_t>(frames), 0.0f);   // the effects, mono
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			for (auto& voice : m_voices)
			{
				const std::vector<int16_t>& samples = voice.sound->samples;
				const double step = static_cast<double>(voice.sound->sampleRate) / static_cast<double>(m_sampleRate);
				const float gain = voice.gain * m_volume * (1.0f / 32768.0f);
				for (int32_t i = 0; i < frames; ++i)
				{
					const size_t index = static_cast<size_t>(voice.position);
					if (index + 1 >= samples.size()) { voice.position = static_cast<double>(samples.size()); break; }
					const float fraction = static_cast<float>(voice.position - static_cast<double>(index));
					accumulator[static_cast<size_t>(i)] += (samples[index] * (1.0f - fraction) + samples[index + 1] * fraction) * gain;
					voice.position += step;
				}
			}
			m_voices.erase(std::remove_if(m_voices.begin(), m_voices.end(),
				[](const Voice& v) { return static_cast<size_t>(v.position) + 1 >= v.sound->samples.size(); }), m_voices.end());
		}
		// the music the feeder decoded ahead (silence when it fell behind)
		size_t read = m_ringRead.load(std::memory_order_relaxed);
		const size_t available = m_ringWrite.load(std::memory_order_acquire) - read;
		for (int32_t i = 0; i < frames; ++i)
		{
			float left = accumulator[static_cast<size_t>(i)], right = left;
			if (static_cast<size_t>(i) < available)
			{
				const size_t at = (read % kRingFrames) * 2;
				left += m_ring[at] * m_volume;
				right += m_ring[at + 1] * m_volume;
				++read;
			}
			out[2 * i] = static_cast<int16_t>(std::max(-1.0f, std::min(1.0f, left)) * 32767.0f);
			out[2 * i + 1] = static_cast<int16_t>(std::max(-1.0f, std::min(1.0f, right)) * 32767.0f);
		}
		m_ringRead.store(read, std::memory_order_release);
	}
}
