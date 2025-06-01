#include "whisper_processor.h"
#include "utils.h"
#include <iostream>
#include <iomanip>
#include <samplerate.h>
#include <cmath>
#include <chrono>
#include <algorithm>

WhisperProcessor::WhisperProcessor(const std::string& model_path,
                                   std::vector<float>& shared_audio_buffer,
                                   std::mutex& buffer_mutex,
                                   std::condition_variable& buffer_cv,
                                   std::atomic<bool>& stop_flag,
                                   DocumentFormatter& formatter)
    : m_model_path(model_path), m_whisper_ctx(nullptr), // m_whisper_state removed
      m_shared_audio_buffer_ref(shared_audio_buffer),
      m_buffer_mutex_ref(buffer_mutex),
      m_buffer_cv_ref(buffer_cv),
      m_stop_flag_ref(stop_flag),
      m_formatter_ref(formatter),
      m_first_transcription_run(true) {}

WhisperProcessor::~WhisperProcessor() {
    if (m_worker_thread.joinable()) {
        m_stop_flag_ref.store(true);
        m_buffer_cv_ref.notify_all();
        m_worker_thread.join();
    }
    // if (m_whisper_state) { whisper_free_state(m_whisper_state); m_whisper_state = nullptr; } // Removed
    if (m_whisper_ctx) { whisper_free(m_whisper_ctx); m_whisper_ctx = nullptr; }
}

bool WhisperProcessor::is_thread_joinable() const { return m_worker_thread.joinable(); }

bool WhisperProcessor::initialize_whisper() {
    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = true;
#if defined(__APPLE__)
#else
    cparams.use_gpu = false;
#endif
    m_whisper_ctx = whisper_init_from_file_with_params(m_model_path.c_str(), cparams);
    if (!m_whisper_ctx) {
        std::cerr << "WhisperProcessor: Failed to load model from " << m_model_path << std::endl;
        return false;
    }
    // m_whisper_state = whisper_init_state(m_whisper_ctx); // Removed
    // if (!m_whisper_state) { /* ... error handling ... */ } // Removed
    return true;
}

void WhisperProcessor::start_processing_thread() {
    if (!m_whisper_ctx) { // Removed m_whisper_state check
        std::cerr << "WhisperProcessor: Whisper context not initialized. Cannot start thread." << std::endl;
        return;
    }
    m_worker_thread = std::thread(&WhisperProcessor::processing_loop, this);
}

void WhisperProcessor::join_thread() { if (m_worker_thread.joinable()) { m_worker_thread.join(); } }

std::vector<float> WhisperProcessor::resample_audio(const std::vector<float>& input_audio) {
    if (input_audio.empty()) return {};
    double ratio = static_cast<double>(WP_WHISPER_SAMPLE_RATE) / static_cast<double>(WP_INPUT_SAMPLE_RATE);
    int est_frames = static_cast<int>(static_cast<double>(input_audio.size()) * ratio) + 1;
    std::vector<float> output(est_frames);
    SRC_DATA src_data;
    src_data.data_in = input_audio.data();
    src_data.input_frames = static_cast<long>(input_audio.size());
    src_data.data_out = output.data();
    src_data.output_frames = static_cast<long>(output.size());
    src_data.src_ratio = ratio;
    src_data.end_of_input = 1;
    if (src_simple(&src_data, SRC_SINC_FASTEST, 1) != 0) return {};
    output.resize(src_data.output_frames_gen);
    return output;
}

void WhisperProcessor::processing_loop() {
    std::vector<float> chunk_to_process_raw;
    std::string previous_chunk_full_text = ""; // Simple de-duplication for full chunk repeats

    while (!m_stop_flag_ref.load(std::memory_order_relaxed)) {
        chunk_to_process_raw.clear();
        {
            std::unique_lock<std::mutex> lock(m_buffer_mutex_ref);
            m_buffer_cv_ref.wait_for(lock, std::chrono::milliseconds(200), [&]{ // Timeout to check stop_flag
                return (m_shared_audio_buffer_ref.size() >= WP_CHUNK_PROCESSING_SAMPLES_RAW) ||
                       m_stop_flag_ref.load(std::memory_order_relaxed);
            });

            if (m_stop_flag_ref.load(std::memory_order_relaxed)) {
                if (m_shared_audio_buffer_ref.size() >= WP_MIN_SAMPLES_IN_BUFFER_FOR_LOOP) { // Process final chunk if decent size
                    chunk_to_process_raw.assign(m_shared_audio_buffer_ref.begin(), m_shared_audio_buffer_ref.end());
                    m_shared_audio_buffer_ref.clear();
                } else {
                    break; // Stop and buffer is too small or empty
                }
            } else if (m_shared_audio_buffer_ref.size() >= WP_CHUNK_PROCESSING_SAMPLES_RAW) {
                chunk_to_process_raw.assign(m_shared_audio_buffer_ref.begin(), m_shared_audio_buffer_ref.begin() + WP_CHUNK_PROCESSING_SAMPLES_RAW);
                m_shared_audio_buffer_ref.erase(m_shared_audio_buffer_ref.begin(), m_shared_audio_buffer_ref.begin() + WP_CHUNK_PROCESSING_SAMPLES_RAW); // Non-overlapping chunks
            } else {
                continue;
            }
        }

        if (chunk_to_process_raw.empty()) {
             if (m_stop_flag_ref.load(std::memory_order_relaxed)) break;
            continue;
        }

        std::vector<float> resampled_chunk = resample_audio(chunk_to_process_raw);
        if (resampled_chunk.empty()) continue;

        whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
        params.language = "en";
        params.suppress_blank = true;
        params.print_realtime = false;
        params.print_progress = false;
        params.no_timestamps = true; // Can be true if not using timestamps for de-dupe

        int stt_result = whisper_full(m_whisper_ctx, params, resampled_chunk.data(), resampled_chunk.size()); // Using whisper_full

        if (stt_result == 0) {
            int n_segments = whisper_full_n_segments(m_whisper_ctx);
            std::string current_chunk_text_combined = "";
            bool new_text_this_run = false;

            if (n_segments > 0) {
                for (int i = 0; i < n_segments; ++i) {
                    const char* text_cstr = whisper_full_get_segment_text(m_whisper_ctx, i);
                    if (text_cstr) {
                        std::string segment = std::string(text_cstr);
                        if(!current_chunk_text_combined.empty() && !segment.empty() && segment.front() != ' ') current_chunk_text_combined += " ";
                        current_chunk_text_combined += segment;
                    }
                }
            }
            current_chunk_text_combined = cleanup_stt_artifacts_util(current_chunk_text_combined);

            // Very simple de-duplication: if this whole chunk is same as previous, skip.
            if (m_first_transcription_run || (previous_chunk_full_text != current_chunk_text_combined && !current_chunk_text_combined.empty())) {
                if (m_first_transcription_run && !current_chunk_text_combined.empty()) {
                     m_formatter_ref.clear_document();
                     m_first_transcription_run = false;
                }
                if (!current_chunk_text_combined.empty()) {
                    m_formatter_ref.process_transcribed_text(current_chunk_text_combined);
                    new_text_this_run = true;
                }
            }
            previous_chunk_full_text = current_chunk_text_combined;

            if (new_text_this_run) {
                m_formatter_ref.print_current_document_preview();
            }
        }
        if (m_stop_flag_ref.load(std::memory_order_relaxed) && m_shared_audio_buffer_ref.empty() ) {
            break;
        }
    }
}