// main.cpp
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <filesystem>

#include "audio_capturer.h"
#include "whisper_processor.h" // This will bring in WP_CHUNK_PROCESSING_SECONDS (if it's a macro)
                              // or WhisperProcessor::CFG_PROCESSING_WINDOW_SECONDS (if static const)
#include "document_formatter.h"
#include "text_segment.h"

namespace fs = std::filesystem;

std::vector<float> g_main_shared_audio_buffer;
std::mutex g_main_document_and_audio_mutex;
std::condition_variable g_main_buffer_cv;
std::atomic<bool> g_main_stop_threads{false};

const int SILENCE_TIMEOUT_SECONDS = 30;
// #define APP_RECORDING_DURATION_SECONDS 30 // No longer used for fixed duration

int main() {
    g_main_stop_threads.store(false);
    DocumentFormatter doc_formatter;

    const char* model_path = "../external/whisper.cpp/models/ggml-base.en.bin";
    WhisperProcessor whisper_processor(model_path,
                                       g_main_shared_audio_buffer,
                                       g_main_document_and_audio_mutex,
                                       g_main_buffer_cv,
                                       g_main_stop_threads,
                                       doc_formatter);
    if (!whisper_processor.initialize_whisper()) {
        std::cerr << "Main: Failed to initialize Whisper. Exiting." << std::endl;
        return 1;
    }

    AudioCapturer audio_capturer(g_main_shared_audio_buffer,
                                 g_main_document_and_audio_mutex,
                                 g_main_buffer_cv,
                                 g_main_stop_threads);
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
    // Use the constant defined in whisper_processor.h directly as it's a macro now
    std::cout << "Speak your commands and text. Processing " << WP_PROCESSING_WINDOW_SECONDS_VAL /* Use the macro directly */ << "s audio chunks." << std::endl;
    std::cout << "--- Listening... (Application will stop after " << SILENCE_TIMEOUT_SECONDS << "s of silence or by 'format stop application') ---" << std::endl;

    while(true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        if (g_main_stop_threads.load(std::memory_order_acquire)) {
             if (doc_formatter.m_should_stop_application.load(std::memory_order_acquire)){
                 // Message already printed by DocumentFormatter or main loop when command detected
             } else {
                 std::cout << "\n--- Main: Stop signal received by main loop. Initiating shutdown... ---" << std::endl;
             }
            break;
        }

        if (doc_formatter.m_should_stop_application.load(std::memory_order_acquire)) {
            std::cout << "\n--- Main: 'format stop application' detected by formatter. Signaling all threads... ---" << std::endl;
            g_main_stop_threads.store(true, std::memory_order_release);
            g_main_buffer_cv.notify_all();
            break;
        }

        auto now = std::chrono::steady_clock::now();
        auto last_speech_activity = whisper_processor.get_last_activity_time();
        auto silence_duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_speech_activity);

        if (silence_duration.count() >= SILENCE_TIMEOUT_SECONDS) {
            std::cout << "\n--- Main: " << SILENCE_TIMEOUT_SECONDS << "s of silence detected. Signaling stop... ---" << std::endl;
            g_main_stop_threads.store(true, std::memory_order_release);
            g_main_buffer_cv.notify_all();
            break;
        }
    }

    audio_capturer.stop_stream();

    if (whisper_processor.is_thread_joinable()) {
        whisper_processor.join_thread();
    }

    fs::path project_run_path = fs::current_path(); // This is cmake-build-debug
    fs::path project_root_path = project_run_path.parent_path(); // Go up to project root "voxformat"
    // If you want to be absolutely sure or if build dir is nested differently:
    // fs::path source_file_dir = fs::path(__FILE__).parent_path(); // Directory of main.cpp
    // project_root_path = source_file_dir; // Assuming main.cpp is in project root

    fs::path output_dir_path = project_root_path / "outputs";
    std::string output_file_path_str = (output_dir_path / "output.txt").string();

    doc_formatter.save_document_to_file(output_file_path_str);

    std::cout << "Application finished." << std::endl;
    return 0;
}