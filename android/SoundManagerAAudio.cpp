#include "SoundManagerAAudio.h"

#include "LogMgr.h"
#include "ResourceManager.h"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace Pakal
{
	namespace
	{
		const size_t kMaxVoices = 24;

		uint16_t read16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
		uint32_t read32(const uint8_t* p) { return static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24)); }
	}

	SoundManagerAAudio::~SoundManagerAAudio()
	{
		terminate();
	}

	void SoundManagerAAudio::initialize()
	{
		AAudioStreamBuilder* builder = nullptr;
		if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK)
		{
			LOG_ERROR("[SoundManagerAAudio] no stream builder: the game plays without sound");
			return;
		}
		AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
		AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
		AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
		AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
		AAudioStreamBuilder_setChannelCount(builder, 1);
		AAudioStreamBuilder_setDataCallback(builder, &SoundManagerAAudio::on_data, this);
		const aaudio_result_t result = AAudioStreamBuilder_openStream(builder, &m_stream);
		AAudioStreamBuilder_delete(builder);
		if (result != AAUDIO_OK || !m_stream)
		{
			LOG_ERROR("[SoundManagerAAudio] the stream did not open (%s): the game plays without sound", AAudio_convertResultToText(result));
			m_stream = nullptr;
			return;
		}
		m_sampleRate = AAudioStream_getSampleRate(m_stream);
		AAudioStream_requestStart(m_stream);
		LOG_INFO("[SoundManagerAAudio] output at %d Hz", m_sampleRate);
	}

	void SoundManagerAAudio::terminate()
	{
		if (m_stream)
		{
			// stopped before it is closed: Android 8's AAudio closed while still stopping let go of a binder
			// object twice, and the app crashed as it quit
			AAudioStream_requestStop(m_stream);
			aaudio_stream_state_t state = AAUDIO_STREAM_STATE_STOPPING;
			for (int i = 0; i < 4 && state != AAUDIO_STREAM_STATE_STOPPED && state != AAUDIO_STREAM_STATE_DISCONNECTED; ++i)
			{
				if (AAudioStream_waitForStateChange(m_stream, state, &state, 100 * 1000000LL) != AAUDIO_OK) break;
			}
			AAudioStream_close(m_stream);
			m_stream = nullptr;
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

	aaudio_data_callback_result_t SoundManagerAAudio::on_data(AAudioStream*, void* user, void* audio, int32_t frames)
	{
		static_cast<SoundManagerAAudio*>(user)->mix(static_cast<int16_t*>(audio), frames);
		return AAUDIO_CALLBACK_RESULT_CONTINUE;
	}

	void SoundManagerAAudio::mix(int16_t* out, int32_t frames)
	{
		std::vector<float> accumulator(static_cast<size_t>(frames), 0.0f);
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
		for (int32_t i = 0; i < frames; ++i)
		{
			const float value = std::max(-1.0f, std::min(1.0f, accumulator[static_cast<size_t>(i)]));
			out[i] = static_cast<int16_t>(value * 32767.0f);
		}
	}
}
