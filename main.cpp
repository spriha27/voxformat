// main.cpp
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "audio_capturer.h"
#include "whisper_processor.h"
#include "document_formatter.h"
#include "text_segment.h"

std::vector<float> g_main_shared_audio_buffer;
std::mutex g_main_buffer_mutex;
std::condition_variable g_main_buffer_cv;
std::atomic<bool> g_main_stop_threads{false};

#define APP_RECORDING_DURATION_SECONDS 30
#define CFG_PROCESSING_WINDOW_SECONDS 4.0;

int main() {
    g_main_stop_threads.store(false);
    DocumentFormatter doc_formatter;
    const char* model_path = "../external/whisper.cpp/models/ggml-base.en.bin";
    WhisperProcessor whisper_processor(model_path,
                                       g_main_shared_audio_buffer, g_main_buffer_mutex,
                                       g_main_buffer_cv, g_main_stop_threads,
                                       doc_formatter);
    if (!whisper_processor.initialize_whisper()) {
        std::cerr << "Main: Failed to initialize Whisper. Exiting." << std::endl;
        return 1;
    }
    AudioCapturer audio_capturer(g_main_shared_audio_buffer, g_main_buffer_mutex,
                                 g_main_buffer_cv, g_main_stop_threads);
    if (!audio_capturer.initialize()) {
        std::cerr << "Main: Failed to initialize Audio Capturer. Exiting." << std::endl;
        return 1;
    }

    whisper_processor.start_processing_thread();
    if (!audio_capturer.start_stream()) {
        std::cerr << "Main: Failed to start audio stream. Signaling stop." << std::endl;
        g_main_stop_threads.store(true);
        g_main_buffer_cv.notify_all();
        if(whisper_processor.is_thread_joinable()) whisper_processor.join_thread();
        return 1;
    }

    std::cout << "Whisper model loaded. VoxFormat ready." << std::endl;
    // std::cout << "Speak your commands and text. Processing " << CFG_PROCESSING_WINDOW_SECONDS << "s audio chunks." << std::endl;
    std::cout << "--- Listening... (Recording for " << APP_RECORDING_DURATION_SECONDS << "s or until 'format stop application') ---" << std::endl;

    auto recording_start_time = std::chrono::steady_clock::now();
    while(!g_main_stop_threads.load(std::memory_order_acquire)) { // Check main stop flag
        // Check if formatter signaled app stop
        if (doc_formatter.m_should_stop_application.load(std::memory_order_acquire)) {
            std::cout << "\n--- Main: 'format stop application' detected. Signaling stop... ---" << std::endl;
            g_main_stop_threads.store(true, std::memory_order_release); // Set main stop flag
            g_main_buffer_cv.notify_all(); // Ensure worker thread wakes up to see it
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - recording_start_time).count() >= APP_RECORDING_DURATION_SECONDS) {
            if(!g_main_stop_threads.load()) {
                std::cout << "\n--- Main: Recording duration reached. Signaling stop... ---" << std::endl;
                g_main_stop_threads.store(true, std::memory_order_release);
                g_main_buffer_cv.notify_all();
            }
            break;
        }
    }
    // This redundant propagation is fine, ensures stop if formatter set it but main loop didn't catch it immediately.
    if (doc_formatter.m_should_stop_application.load() && !g_main_stop_threads.load()) {
        g_main_stop_threads.store(true, std::memory_order_release);
        g_main_buffer_cv.notify_all();
    }

    audio_capturer.stop_stream();

    if (whisper_processor.is_thread_joinable()) {
        whisper_processor.join_thread();
    }

    std::cout << "Application finished." << std::endl;
    return 0;
}