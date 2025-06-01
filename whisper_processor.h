#ifndef WHISPER_PROCESSOR_H
#define WHISPER_PROCESSOR_H

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "whisper.h"
#include "document_formatter.h"

#define WP_INPUT_SAMPLE_RATE 44100
#define WP_WHISPER_SAMPLE_RATE 16000
#define CFG_PROCESSING_WINDOW_SECONDS 4.0;

// Use the fixed-chunk processing constants again
const double WP_CHUNK_PROCESSING_SECONDS = 4.0;
const size_t WP_CHUNK_PROCESSING_SAMPLES_RAW = static_cast<size_t>(WP_INPUT_SAMPLE_RATE * WP_CHUNK_PROCESSING_SECONDS);
// Min samples needed in buffer to trigger processing loop iteration (especially for final chunk)
const size_t WP_MIN_SAMPLES_IN_BUFFER_FOR_LOOP = static_cast<size_t>(WP_INPUT_SAMPLE_RATE * 0.5); // 0.5s

class WhisperProcessor {
public:
    WhisperProcessor(const std::string& model_path,
                       std::vector<float>& shared_audio_buffer,
                       std::mutex& buffer_mutex,
                       std::condition_variable& buffer_cv,
                       std::atomic<bool>& stop_flag,
                       DocumentFormatter& formatter);
    ~WhisperProcessor();

    bool initialize_whisper();
    void start_processing_thread();
    void join_thread();
    bool is_thread_joinable() const;

private:
    void processing_loop();
    std::vector<float> resample_audio(const std::vector<float>& input_audio);

    std::string m_model_path;
    whisper_context* m_whisper_ctx;
    // whisper_state*   m_whisper_state; // Not used in this simpler model
    std::thread m_worker_thread;

    std::vector<float>& m_shared_audio_buffer_ref;
    std::mutex& m_buffer_mutex_ref;
    std::condition_variable& m_buffer_cv_ref;
    std::atomic<bool>& m_stop_flag_ref;
    DocumentFormatter& m_formatter_ref;

    bool m_first_transcription_run;
};

#endif // WHISPER_PROCESSOR_H