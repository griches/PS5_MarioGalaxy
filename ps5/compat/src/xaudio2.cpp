// Super Mario Galaxy for the PlayStation 5 - XAudio2 for Nebula's audio sink: one
// engine, one mastering voice and PCM source voices with their buffer queues
// and callbacks, mixed into the console's 48 kHz stereo output (ps5/src/platform.c
// calls the mixer for 256 frames at a time from its own thread). A source voice
// is resampled by linear interpolation at its rate times its frequency ratio,
// which is how the sink keeps the game's audio clock and the output together.
//
// SPDX-License-Identifier: GPL-3.0-only
#include "compat_internal.h"

#include <xaudio2.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <mutex>
#include <vector>

#include "platform.h"

namespace {

constexpr UINT32 kOutputRate = 48000;

class Engine;

class SourceVoice final : public IXAudio2SourceVoice {
public:
    SourceVoice(Engine* engine, const WAVEFORMATEX& format, IXAudio2VoiceCallback* callback, float max_ratio)
        : engine_(engine), format_(format), callback_(callback), max_ratio_(max_ratio) {
        float_samples_ = format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                         (format.wFormatTag == WAVE_FORMAT_EXTENSIBLE && format.wBitsPerSample == 32);
        channels_ = std::max<WORD>(1, format.nChannels);
        frame_bytes_ = std::max<WORD>(1, format.nBlockAlign);
    }

    void STDMETHODCALLTYPE GetVoiceDetails(XAUDIO2_VOICE_DETAILS* details) override {
        details->CreationFlags = 0;
        details->ActiveFlags = 0;
        details->InputChannels = channels_;
        details->InputSampleRate = format_.nSamplesPerSec;
    }
    HRESULT STDMETHODCALLTYPE SetVolume(float volume, UINT32) override {
        std::lock_guard lock(mutex_);
        volume_ = volume;
        return S_OK;
    }
    void STDMETHODCALLTYPE GetVolume(float* volume) override {
        std::lock_guard lock(mutex_);
        *volume = volume_;
    }
    void STDMETHODCALLTYPE DestroyVoice() override;

    HRESULT STDMETHODCALLTYPE Start(UINT32, UINT32) override {
        std::lock_guard lock(mutex_);
        playing_ = true;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Stop(UINT32, UINT32) override {
        std::lock_guard lock(mutex_);
        playing_ = false;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SubmitSourceBuffer(const XAUDIO2_BUFFER* buffer, const XAUDIO2_BUFFER_WMA*) override {
        if (buffer == nullptr || buffer->pAudioData == nullptr) return E_INVALIDARG;
        std::lock_guard lock(mutex_);
        if (queue_.size() >= XAUDIO2_MAX_QUEUED_BUFFERS) return XAUDIO2_E_INVALID_CALL;
        Queued queued{};
        queued.buffer = *buffer;
        const UINT32 frames = buffer->AudioBytes / frame_bytes_;
        queued.begin = std::min(buffer->PlayBegin, frames);
        queued.end = buffer->PlayLength != 0 ? std::min(frames, queued.begin + buffer->PlayLength) : frames;
        queue_.push_back(queued);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE FlushSourceBuffers() override {
        std::vector<void*> flushed;
        {
            std::lock_guard lock(mutex_);
            // The buffer being played stays while the voice runs, as XAudio2 keeps it.
            const std::size_t keep = playing_ && !queue_.empty() && queue_.front().started ? 1u : 0u;
            while (queue_.size() > keep) {
                flushed.push_back(queue_.back().buffer.pContext);
                queue_.pop_back();
            }
            std::reverse(flushed.begin(), flushed.end());
            if (keep == 0) position_ = 0.0;
        }
        if (callback_ != nullptr) {
            for (void* context : flushed) callback_->OnBufferEnd(context);
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Discontinuity() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE ExitLoop(UINT32) override { return S_OK; }
    void STDMETHODCALLTYPE GetState(XAUDIO2_VOICE_STATE* state, UINT32 flags) override {
        std::lock_guard lock(mutex_);
        state->pCurrentBufferContext = queue_.empty() ? nullptr : queue_.front().buffer.pContext;
        state->BuffersQueued = static_cast<UINT32>(queue_.size());
        state->SamplesPlayed = (flags & XAUDIO2_VOICE_NOSAMPLESPLAYED) != 0 ? 0 : samples_played_;
    }
    HRESULT STDMETHODCALLTYPE SetFrequencyRatio(float ratio, UINT32) override {
        std::lock_guard lock(mutex_);
        ratio_ = std::clamp(ratio, 1.0f / 1024.0f, max_ratio_);
        return S_OK;
    }
    void STDMETHODCALLTYPE GetFrequencyRatio(float* ratio) override {
        std::lock_guard lock(mutex_);
        *ratio = ratio_;
    }
    HRESULT STDMETHODCALLTYPE SetSourceSampleRate(UINT32 rate) override {
        std::lock_guard lock(mutex_);
        format_.nSamplesPerSec = rate;
        return S_OK;
    }

    // Adds `frames` output frames of this voice into `mix` (interleaved stereo float).
    void mix_into(float* mix, int frames);

private:
    struct Queued {
        XAUDIO2_BUFFER buffer;
        UINT32 begin;
        UINT32 end;
        bool started;
    };

    // One source sample of channel `channel` at frame `frame` of the buffer.
    float sample(const Queued& queued, UINT32 frame, unsigned channel) const {
        const unsigned c = std::min<unsigned>(channel, channels_ - 1u);
        const BYTE* data = queued.buffer.pAudioData + static_cast<std::size_t>(frame) * frame_bytes_;
        if (float_samples_) {
            float value;
            std::memcpy(&value, data + c * 4, 4);
            return value;
        }
        if (format_.wBitsPerSample == 8) return (static_cast<int>(data[c]) - 128) / 128.0f;
        std::int16_t value;
        std::memcpy(&value, data + c * 2, 2);
        return value / 32768.0f;
    }

    Engine* engine_;
    WAVEFORMATEX format_;
    IXAudio2VoiceCallback* callback_;
    float max_ratio_;
    bool float_samples_ = false;
    WORD channels_ = 2;
    WORD frame_bytes_ = 4;

    std::mutex mutex_;
    std::deque<Queued> queue_;
    bool playing_ = false;
    float volume_ = 1.0f;
    float ratio_ = 1.0f;
    double position_ = 0.0;  // in source frames, within the front buffer
    float previous_[2] = {0.0f, 0.0f};
    UINT64 samples_played_ = 0;
};

class MasteringVoice final : public IXAudio2MasteringVoice {
public:
    void STDMETHODCALLTYPE GetVoiceDetails(XAUDIO2_VOICE_DETAILS* details) override {
        details->CreationFlags = 0;
        details->ActiveFlags = 0;
        details->InputChannels = 2;
        details->InputSampleRate = kOutputRate;
    }
    HRESULT STDMETHODCALLTYPE SetVolume(float volume, UINT32) override {
        volume_ = volume;
        return S_OK;
    }
    void STDMETHODCALLTYPE GetVolume(float* volume) override { *volume = volume_; }
    void STDMETHODCALLTYPE DestroyVoice() override {}
    HRESULT STDMETHODCALLTYPE GetChannelMask(DWORD* mask) override {
        *mask = SPEAKER_STEREO;
        return S_OK;
    }
    float volume_ = 1.0f;
};

class Engine final : public IXAudio2 {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** object) override {
        *object = this;
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG left = --references_;
        if (left == 0) {
            StopEngine();
            delete this;
        }
        return left;
    }
    HRESULT STDMETHODCALLTYPE RegisterForCallbacks(IXAudio2EngineCallback* callback) override {
        std::lock_guard lock(mutex_);
        callbacks_.push_back(callback);
        return S_OK;
    }
    void STDMETHODCALLTYPE UnregisterForCallbacks(IXAudio2EngineCallback* callback) override {
        std::lock_guard lock(mutex_);
        callbacks_.erase(std::remove(callbacks_.begin(), callbacks_.end(), callback), callbacks_.end());
    }
    HRESULT STDMETHODCALLTYPE CreateSourceVoice(IXAudio2SourceVoice** voice, const WAVEFORMATEX* format, UINT32,
                                                float max_ratio, IXAudio2VoiceCallback* callback,
                                                const XAUDIO2_VOICE_SENDS*, const XAUDIO2_EFFECT_CHAIN*) override {
        if (voice == nullptr || format == nullptr) return E_INVALIDARG;
        if (format->wFormatTag != WAVE_FORMAT_PCM && format->wFormatTag != WAVE_FORMAT_IEEE_FLOAT &&
            format->wFormatTag != WAVE_FORMAT_EXTENSIBLE) {
            return E_INVALIDARG;
        }
        auto* source = new SourceVoice(this, *format, callback, max_ratio);
        std::lock_guard lock(mutex_);
        sources_.push_back(source);
        *voice = source;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE CreateMasteringVoice(IXAudio2MasteringVoice** voice, UINT32, UINT32, UINT32, LPCWSTR,
                                                   const XAUDIO2_EFFECT_CHAIN*, AUDIO_STREAM_CATEGORY) override {
        if (voice == nullptr) return E_INVALIDARG;
        *voice = &mastering_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE StartEngine() override {
        std::lock_guard lock(mutex_);
        if (!started_) {
            started_ = audio_start(&Engine::fill, this);
            if (!started_) return E_FAIL;
        }
        return S_OK;
    }
    void STDMETHODCALLTYPE StopEngine() override {
        bool was_started = false;
        {
            std::lock_guard lock(mutex_);
            was_started = started_;
            started_ = false;
        }
        if (was_started) audio_stop();
    }
    HRESULT STDMETHODCALLTYPE CommitChanges(UINT32) override { return S_OK; }
    void STDMETHODCALLTYPE GetPerformanceData(XAUDIO2_PERFORMANCE_DATA* data) override {
        std::memset(data, 0, sizeof(*data));
        std::lock_guard lock(mutex_);
        data->CurrentLatencyInSamples = 512;
        data->ActiveSourceVoiceCount = static_cast<UINT32>(sources_.size());
        data->TotalSourceVoiceCount = static_cast<UINT32>(sources_.size());
    }
    void STDMETHODCALLTYPE SetDebugConfiguration(const XAUDIO2_DEBUG_CONFIGURATION*, void*) override {}

    void remove(SourceVoice* voice) {
        std::lock_guard mix_guard(mix_mutex_);
        std::lock_guard lock(mutex_);
        sources_.erase(std::remove(sources_.begin(), sources_.end(), voice), sources_.end());
    }

private:
    static void fill(std::int16_t* frames, int count, void* user) {
        static_cast<Engine*>(user)->render(frames, count);
    }

    void render(std::int16_t* frames, int count) {
        std::vector<IXAudio2EngineCallback*> callbacks;
        {
            std::lock_guard lock(mutex_);
            callbacks = callbacks_;
        }
        for (auto* callback : callbacks) callback->OnProcessingPassStart();
        mix_.assign(static_cast<std::size_t>(count) * 2, 0.0f);
        {
            std::lock_guard mix_guard(mix_mutex_);
            std::vector<SourceVoice*> sources;
            {
                std::lock_guard lock(mutex_);
                sources = sources_;
            }
            for (SourceVoice* source : sources) source->mix_into(mix_.data(), count);
        }
        const float volume = mastering_.volume_;
        for (int i = 0; i < count * 2; ++i) {
            const float value = std::clamp(mix_[static_cast<std::size_t>(i)] * volume, -1.0f, 1.0f);
            frames[i] = static_cast<std::int16_t>(std::lrintf(value * 32767.0f));
        }
        for (auto* callback : callbacks) callback->OnProcessingPassEnd();
    }

    std::mutex mutex_;
    std::mutex mix_mutex_;  // held while mixing: a voice is not destroyed mid-mix
    std::vector<SourceVoice*> sources_;
    std::vector<IXAudio2EngineCallback*> callbacks_;
    MasteringVoice mastering_;
    std::vector<float> mix_;
    bool started_ = false;
    ULONG references_ = 1;
};

void SourceVoice::DestroyVoice() {
    engine_->remove(this);
    delete this;
}

void SourceVoice::mix_into(float* mix, int frames) {
    if (callback_ != nullptr) {
        UINT32 required = 0;
        {
            std::lock_guard lock(mutex_);
            required = static_cast<UINT32>(std::ceil(frames * format_.nSamplesPerSec * ratio_ / kOutputRate)) * frame_bytes_;
        }
        callback_->OnVoiceProcessingPassStart(required);
    }
    std::vector<void*> started;
    std::vector<std::pair<void*, bool>> ended;  // context, end of stream
    {
        std::lock_guard lock(mutex_);
        if (playing_) {
            const double step = static_cast<double>(format_.nSamplesPerSec) * ratio_ / kOutputRate;
            for (int i = 0; i < frames && !queue_.empty(); ++i) {
                Queued& front = queue_.front();
                if (!front.started) {
                    front.started = true;
                    started.push_back(front.buffer.pContext);
                }
                const UINT32 length = front.end - front.begin;
                const UINT32 index = static_cast<UINT32>(position_);
                const float fraction = static_cast<float>(position_ - index);
                for (unsigned c = 0; c < 2; ++c) {
                    const float current = index < length ? sample(front, front.begin + index, c) : previous_[c];
                    float next;
                    if (index + 1 < length) {
                        next = sample(front, front.begin + index + 1, c);
                    } else if (queue_.size() > 1) {
                        const Queued& following = queue_[1];
                        next = following.end > following.begin ? sample(following, following.begin, c) : current;
                    } else {
                        next = current;
                    }
                    mix[2 * i + c] += (current + (next - current) * fraction) * volume_;
                }
                position_ += step;
                while (!queue_.empty() && position_ >= static_cast<double>(queue_.front().end - queue_.front().begin)) {
                    Queued& done = queue_.front();
                    const UINT32 done_length = done.end - done.begin;
                    if (done_length > 0) {
                        for (unsigned c = 0; c < 2; ++c) previous_[c] = sample(done, done.end - 1, c);
                    }
                    position_ -= done_length;
                    samples_played_ += done_length;
                    ended.emplace_back(done.buffer.pContext, (done.buffer.Flags & XAUDIO2_END_OF_STREAM) != 0);
                    queue_.pop_front();
                    if (!queue_.empty() && !queue_.front().started) {
                        queue_.front().started = true;
                        started.push_back(queue_.front().buffer.pContext);
                    }
                }
            }
            if (queue_.empty()) position_ = 0.0;
        }
    }
    if (callback_ != nullptr) {
        // In order: each buffer's start before its end
        std::size_t s = 0;
        for (const auto& [context, end_of_stream] : ended) {
            while (s < started.size()) {
                const bool same = started[s] == context;
                callback_->OnBufferStart(started[s++]);
                if (same) break;
            }
            callback_->OnBufferEnd(context);
            if (end_of_stream) callback_->OnStreamEnd();
        }
        while (s < started.size()) callback_->OnBufferStart(started[s++]);
        callback_->OnVoiceProcessingPassEnd();
    }
}

}  // namespace

HRESULT XAudio2Create(IXAudio2** engine, UINT32, UINT32) noexcept {
    if (engine == nullptr) return E_POINTER;
    *engine = new Engine();
    return S_OK;
}
