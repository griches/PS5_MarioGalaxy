// Super Mario Galaxy for the PlayStation 5 - the XAudio2 Nebula's audio sink uses:
// one engine, one mastering voice and one PCM source voice with its callbacks,
// played through the console's audio output (ps5/compat/src/xaudio2.cpp).
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "objbase.h"

#define XAUDIO2_DEFAULT_PROCESSOR 0x00000001
#define XAUDIO2_ANY_PROCESSOR 0xffffffff
#define XAUDIO2_DEFAULT_CHANNELS 0
#define XAUDIO2_DEFAULT_SAMPLERATE 0
#define XAUDIO2_MAX_QUEUED_BUFFERS 64
#define XAUDIO2_MAX_FREQ_RATIO 1024.0f
#define XAUDIO2_DEFAULT_FREQ_RATIO 2.0f
#define XAUDIO2_COMMIT_NOW 0
#define XAUDIO2_COMMIT_ALL 0
#define XAUDIO2_END_OF_STREAM 0x0040
#define XAUDIO2_VOICE_NOPITCH 0x0002
#define XAUDIO2_VOICE_NOSRC 0x0004
#define XAUDIO2_VOICE_USEFILTER 0x0008
#define XAUDIO2_VOICE_NOSAMPLESPLAYED 0x0100
#define XAUDIO2_PLAY_TAILS 0x0020
#define XAUDIO2_NO_LOOP_REGION 0
#define XAUDIO2_LOOP_INFINITE 255
#define XAUDIO2_E_INVALID_CALL ((HRESULT)0x88960001)
#define XAUDIO2_E_XMA_DECODER_ERROR ((HRESULT)0x88960002)
#define XAUDIO2_E_XAPO_CREATION_FAILED ((HRESULT)0x88960003)
#define XAUDIO2_E_DEVICE_INVALIDATED ((HRESULT)0x88960004)
#define AUDIOCLIENT_E_DEVICE_INVALIDATED ((HRESULT)0x88890004)
#define WAVE_FORMAT_PCM 1
#define WAVE_FORMAT_IEEE_FLOAT 3
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE
#define SPEAKER_FRONT_LEFT 0x1
#define SPEAKER_FRONT_RIGHT 0x2
#define SPEAKER_STEREO (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT)

typedef struct tWAVEFORMATEX {
    WORD wFormatTag;
    WORD nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD nBlockAlign;
    WORD wBitsPerSample;
    WORD cbSize;
} WAVEFORMATEX, *PWAVEFORMATEX, *LPWAVEFORMATEX;
typedef struct {
    WAVEFORMATEX Format;
    union {
        WORD wValidBitsPerSample;
        WORD wSamplesPerBlock;
        WORD wReserved;
    } Samples;
    DWORD dwChannelMask;
    GUID SubFormat;
} WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE;

typedef enum _AUDIO_STREAM_CATEGORY {
    AudioCategory_Other = 0,
    AudioCategory_ForegroundOnlyMedia = 1,
    AudioCategory_Communications = 3,
    AudioCategory_Alerts = 4,
    AudioCategory_SoundEffects = 5,
    AudioCategory_GameEffects = 6,
    AudioCategory_GameMedia = 7,
    AudioCategory_GameChat = 8,
    AudioCategory_Speech = 9,
    AudioCategory_Movie = 10,
    AudioCategory_Media = 11
} AUDIO_STREAM_CATEGORY;

typedef struct XAUDIO2_BUFFER {
    UINT32 Flags;
    UINT32 AudioBytes;
    const BYTE* pAudioData;
    UINT32 PlayBegin;
    UINT32 PlayLength;
    UINT32 LoopBegin;
    UINT32 LoopLength;
    UINT32 LoopCount;
    void* pContext;
} XAUDIO2_BUFFER;
typedef struct XAUDIO2_BUFFER_WMA {
    const UINT32* pDecodedPacketCumulativeBytes;
    UINT32 PacketCount;
} XAUDIO2_BUFFER_WMA;
typedef struct XAUDIO2_VOICE_STATE {
    void* pCurrentBufferContext;
    UINT32 BuffersQueued;
    UINT64 SamplesPlayed;
} XAUDIO2_VOICE_STATE;
typedef struct XAUDIO2_PERFORMANCE_DATA {
    UINT64 AudioCyclesSinceLastQuery;
    UINT64 TotalCyclesSinceLastQuery;
    UINT32 MinimumCyclesPerQuantum;
    UINT32 MaximumCyclesPerQuantum;
    UINT32 MemoryUsageInBytes;
    UINT32 CurrentLatencyInSamples;
    UINT32 GlitchesSinceEngineStarted;
    UINT32 ActiveSourceVoiceCount;
    UINT32 TotalSourceVoiceCount;
    UINT32 ActiveSubmixVoiceCount;
    UINT32 ActiveResamplerCount;
    UINT32 ActiveMatrixMixCount;
    UINT32 ActiveXmaSourceVoices;
    UINT32 ActiveXmaStreams;
} XAUDIO2_PERFORMANCE_DATA;
typedef struct XAUDIO2_VOICE_DETAILS {
    UINT32 CreationFlags;
    UINT32 ActiveFlags;
    UINT32 InputChannels;
    UINT32 InputSampleRate;
} XAUDIO2_VOICE_DETAILS;
typedef struct XAUDIO2_DEBUG_CONFIGURATION {
    UINT32 TraceMask;
    UINT32 BreakMask;
    BOOL LogThreadID;
    BOOL LogFileline;
    BOOL LogFunctionName;
    BOOL LogTiming;
} XAUDIO2_DEBUG_CONFIGURATION;
struct XAUDIO2_VOICE_SENDS;
struct XAUDIO2_EFFECT_CHAIN;
struct XAUDIO2_FILTER_PARAMETERS;

struct IXAudio2VoiceCallback {
    virtual void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32 bytes_required) = 0;
    virtual void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() = 0;
    virtual void STDMETHODCALLTYPE OnStreamEnd() = 0;
    virtual void STDMETHODCALLTYPE OnBufferStart(void* context) = 0;
    virtual void STDMETHODCALLTYPE OnBufferEnd(void* context) = 0;
    virtual void STDMETHODCALLTYPE OnLoopEnd(void* context) = 0;
    virtual void STDMETHODCALLTYPE OnVoiceError(void* context, HRESULT error) = 0;
};
struct IXAudio2EngineCallback {
    virtual void STDMETHODCALLTYPE OnProcessingPassStart() = 0;
    virtual void STDMETHODCALLTYPE OnProcessingPassEnd() = 0;
    virtual void STDMETHODCALLTYPE OnCriticalError(HRESULT error) = 0;
};

struct IXAudio2Voice {
    virtual void STDMETHODCALLTYPE GetVoiceDetails(XAUDIO2_VOICE_DETAILS* details) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetVolume(float volume, UINT32 operation_set = XAUDIO2_COMMIT_NOW) = 0;
    virtual void STDMETHODCALLTYPE GetVolume(float* volume) = 0;
    virtual void STDMETHODCALLTYPE DestroyVoice() = 0;
};
struct IXAudio2SourceVoice : IXAudio2Voice {
    virtual HRESULT STDMETHODCALLTYPE Start(UINT32 flags = 0, UINT32 operation_set = XAUDIO2_COMMIT_NOW) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stop(UINT32 flags = 0, UINT32 operation_set = XAUDIO2_COMMIT_NOW) = 0;
    virtual HRESULT STDMETHODCALLTYPE SubmitSourceBuffer(const XAUDIO2_BUFFER* buffer,
                                                         const XAUDIO2_BUFFER_WMA* wma = nullptr) = 0;
    virtual HRESULT STDMETHODCALLTYPE FlushSourceBuffers() = 0;
    virtual HRESULT STDMETHODCALLTYPE Discontinuity() = 0;
    virtual HRESULT STDMETHODCALLTYPE ExitLoop(UINT32 operation_set = XAUDIO2_COMMIT_NOW) = 0;
    virtual void STDMETHODCALLTYPE GetState(XAUDIO2_VOICE_STATE* state, UINT32 flags = 0) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetFrequencyRatio(float ratio, UINT32 operation_set = XAUDIO2_COMMIT_NOW) = 0;
    virtual void STDMETHODCALLTYPE GetFrequencyRatio(float* ratio) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetSourceSampleRate(UINT32 rate) = 0;
};
struct IXAudio2MasteringVoice : IXAudio2Voice {
    virtual HRESULT STDMETHODCALLTYPE GetChannelMask(DWORD* mask) = 0;
};
struct IXAudio2SubmixVoice : IXAudio2Voice {};

struct IXAudio2 : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE RegisterForCallbacks(IXAudio2EngineCallback* callback) = 0;
    virtual void STDMETHODCALLTYPE UnregisterForCallbacks(IXAudio2EngineCallback* callback) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateSourceVoice(IXAudio2SourceVoice** voice, const WAVEFORMATEX* format,
                                                        UINT32 flags = 0, float max_ratio = XAUDIO2_DEFAULT_FREQ_RATIO,
                                                        IXAudio2VoiceCallback* callback = nullptr,
                                                        const XAUDIO2_VOICE_SENDS* sends = nullptr,
                                                        const XAUDIO2_EFFECT_CHAIN* effects = nullptr) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateMasteringVoice(IXAudio2MasteringVoice** voice,
                                                           UINT32 channels = XAUDIO2_DEFAULT_CHANNELS,
                                                           UINT32 rate = XAUDIO2_DEFAULT_SAMPLERATE, UINT32 flags = 0,
                                                           LPCWSTR device_id = nullptr,
                                                           const XAUDIO2_EFFECT_CHAIN* effects = nullptr,
                                                           AUDIO_STREAM_CATEGORY category = AudioCategory_GameEffects) = 0;
    virtual HRESULT STDMETHODCALLTYPE StartEngine() = 0;
    virtual void STDMETHODCALLTYPE StopEngine() = 0;
    virtual HRESULT STDMETHODCALLTYPE CommitChanges(UINT32 operation_set) = 0;
    virtual void STDMETHODCALLTYPE GetPerformanceData(XAUDIO2_PERFORMANCE_DATA* data) = 0;
    virtual void STDMETHODCALLTYPE SetDebugConfiguration(const XAUDIO2_DEBUG_CONFIGURATION* configuration,
                                                         void* reserved = nullptr) = 0;
};
HRESULT XAudio2Create(IXAudio2** engine, UINT32 flags = 0, UINT32 processor = XAUDIO2_DEFAULT_PROCESSOR) noexcept;
