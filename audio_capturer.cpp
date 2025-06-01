#include "audio_capturer.h"
#include <iostream>
#include <algorithm>
#include <thread>
#include <chrono>

AudioCapturer::AudioCapturer(std::vector<float>& shared_buffer, std::mutex& buffer_mutex,
                             std::condition_variable& buffer_cv, std::atomic<bool>& stop_flag)
    : m_stream(nullptr), m_pa_err(paNoError), m_pa_initialized_by_this_instance(false),
      m_shared_audio_buffer_ref(shared_buffer),
      m_buffer_mutex_ref(buffer_mutex),
      m_buffer_cv_ref(buffer_cv),
      m_stop_flag_ref(stop_flag) {}

AudioCapturer::~AudioCapturer() {
    if (m_stream) {
        if (is_stream_active()) {
            Pa_StopStream(m_stream);
        }
        Pa_CloseStream(m_stream);
        m_stream = nullptr;
    }
    if (m_pa_initialized_by_this_instance) {
        Pa_Terminate();
        m_pa_initialized_by_this_instance = false;
    }
}

int AudioCapturer::pa_capture_callback(const void *inputBuffer, void *outputBuffer,
                                       unsigned long framesPerBuffer,
                                       const PaStreamCallbackTimeInfo* timeInfo,
                                       PaStreamCallbackFlags statusFlags,
                                       void *userData) {
    AudioCapturer* self = static_cast<AudioCapturer*>(userData);
    if (self->m_stop_flag_ref.load(std::memory_order_acquire)) {
        return paComplete;
    }
    if (!inputBuffer) return paContinue;

    const float *samples = static_cast<const float *>(inputBuffer);
    {
        std::lock_guard<std::mutex> lock(self->m_buffer_mutex_ref);
        self->m_shared_audio_buffer_ref.insert(self->m_shared_audio_buffer_ref.end(), samples, samples + framesPerBuffer);
    }
    self->m_buffer_cv_ref.notify_one();
    return paContinue;
}

bool AudioCapturer::initialize() {
    if (m_pa_initialized_by_this_instance) {
        return true;
    }
    m_pa_err = Pa_Initialize();
    if (m_pa_err != paNoError) {
        std::cerr << "AudioCapturer: PortAudio initialization error: " << Pa_GetErrorText(m_pa_err) << std::endl;
        return false;
    }
    m_pa_initialized_by_this_instance = true;

    PaDeviceIndex device_idx = Pa_GetDefaultInputDevice();
    if (device_idx == paNoDevice) {
        std::cerr << "AudioCapturer: No default input audio device found." << std::endl;
        return false;
    }
    const PaDeviceInfo* device_info = Pa_GetDeviceInfo(device_idx);

    m_input_parameters.device = device_idx;
    m_input_parameters.channelCount = 1;
    m_input_parameters.sampleFormat = paFloat32;
    m_input_parameters.suggestedLatency = device_info ? device_info->defaultLowInputLatency : 0.1;
    m_input_parameters.hostApiSpecificStreamInfo = nullptr;

    m_pa_err = Pa_OpenStream(
              &m_stream,
              &m_input_parameters,
              nullptr,
              AC_INPUT_SAMPLE_RATE,
              AC_FRAMES_PER_CALLBACK,
              paClipOff,
              AudioCapturer::pa_capture_callback,
              this
          );

    if (m_pa_err != paNoError) {
        std::cerr << "AudioCapturer: PortAudio error opening stream: " << Pa_GetErrorText(m_pa_err) << std::endl;
        m_stream = nullptr;
        return false;
    }
    return true;
}

bool AudioCapturer::start_stream() {
    if (!m_stream) {
        std::cerr << "AudioCapturer: Stream not initialized. Cannot start." << std::endl;
        return false;
    }
    if (Pa_IsStreamActive(m_stream) > 0) {
        return true;
    }
    m_pa_err = Pa_StartStream(m_stream);
    if (m_pa_err != paNoError) {
        std::cerr << "AudioCapturer: PortAudio error starting stream: " << Pa_GetErrorText(m_pa_err) << std::endl;
        Pa_CloseStream(m_stream);
        m_stream = nullptr;
        return false;
    }
    return true;
}

void AudioCapturer::stop_stream() {
    if (m_stream && is_stream_active()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        m_pa_err = Pa_StopStream(m_stream);
        if (m_pa_err != paNoError && m_pa_err != paStreamIsStopped) {
             std::cerr << "AudioCapturer Warning: Pa_StopStream reported: " << Pa_GetErrorText(m_pa_err) << std::endl;
        }
    }
    if (m_stream) {
        Pa_CloseStream(m_stream);
        m_stream = nullptr;
    }
}

bool AudioCapturer::is_stream_active() const {
    if (!m_stream) return false;
    PaError err = Pa_IsStreamActive(m_stream);
    if (err < 0) {
        return false;
    }
    return err > 0;
}