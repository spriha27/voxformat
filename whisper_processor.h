#ifndef WHISPER_PROCESSOR_H
#define WHISPER_PROCESSOR_H

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include "whisper.h"
#include "document_formatter.h"

// Define constants used by this class and potentially by main.cpp for printing
// These are now preprocessor macros for easier use in calculating other constants within this header.
#define WP_PROCESSING_WINDOW_SECONDS_VAL 4.0
#define WP_INPUT_SAMPLE_RATE 44100
#define WP_WHISPER_SAMPLE_RATE 16000
#define WP_WINDOW_SLIDE_SECONDS_VAL 2.0
#define WP_MIN_CHUNK_PROCESS_SECONDS_VAL 1.0 // For final chunk

// Calculated constants
const size_t WP_CHUNK_PROCESSING_SAMPLES_RAW = static_cast<size_t>(WP_INPUT_SAMPLE_RATE * WP_PROCESSING_WINDOW_SECONDS_VAL);
const size_t WP_WINDOW_SLIDE_SAMPLES_RAW = static_cast<size_t>(WP_INPUT_SAMPLE_RATE * WP_WINDOW_SLIDE_SECONDS_VAL);
const size_t WP_MIN_SAMPLES_FOR_FINAL_CHUNK_RAW = static_cast<size_t>(WP_INPUT_SAMPLE_RATE * WP_MIN_CHUNK_PROCESS_SECONDS_VAL);


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
    std::chrono::steady_clock::time_point get_last_activity_time() const; // Declaration added

private:
    void processing_loop();
    std::vector<float> resample_audio(const std::vector<float>& input_audio);

    std::string m_model_path;
    whisper_context* m_whisper_ctx;
    std::thread m_worker_thread;

    std::vector<float>& m_shared_audio_buffer_ref;
    std::mutex& m_buffer_mutex_ref;
    std::condition_variable& m_buffer_cv_ref;
    std::atomic<bool>& m_stop_flag_ref;
    DocumentFormatter& m_formatter_ref;

    bool m_first_transcription_run;
    std::string m_previous_chunk_full_text_for_dedup;
    std::atomic<std::chrono::steady_clock::time_point> m_last_activity_time;
};

#endif // WHISPER_PROCESSOR_H