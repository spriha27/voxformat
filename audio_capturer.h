#ifndef AUDIO_CAPTURER_H
#define AUDIO_CAPTURER_H

#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <portaudio.h>

#define AC_INPUT_SAMPLE_RATE 44100
#define AC_FRAMES_PER_CALLBACK 256

class AudioCapturer {
public:
    AudioCapturer(std::vector<float>& shared_buffer, std::mutex& buffer_mutex,
                  std::condition_variable& buffer_cv, std::atomic<bool>& stop_flag);
    ~AudioCapturer();
    bool initialize();
    bool start_stream();
    void stop_stream();
    bool is_stream_active() const;

private:
    PaStream* m_stream;
    PaStreamParameters m_input_parameters;
    PaError m_pa_err;
    bool m_pa_initialized_by_this_instance;

    std::vector<float>& m_shared_audio_buffer_ref;
    std::mutex& m_buffer_mutex_ref;
    std::condition_variable& m_buffer_cv_ref;
    std::atomic<bool>& m_stop_flag_ref;

    static int pa_capture_callback(const void *inputBuffer, void *outputBuffer,
                                   unsigned long framesPerBuffer,
                                   const PaStreamCallbackTimeInfo* timeInfo,
                                   PaStreamCallbackFlags statusFlags,
                                   void *userData);
};
#endif // AUDIO_CAPTURER_H