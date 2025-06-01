#ifndef DOCUMENT_FORMATTER_H
#define DOCUMENT_FORMATTER_H

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include "text_segment.h"

class DocumentFormatter {
public:
    DocumentFormatter();
    void process_transcribed_text(const std::string& text_from_whisper_raw);
    void print_current_document_preview() const;
    std::string get_markdown_document() const;
    void save_document_to_file(const std::string& full_filename_path) const;
    void signal_stop_application();
    void clear_document();

    std::atomic<bool> m_should_stop_application{false};

private:
    bool m_is_bold_active;
    bool m_is_italic_active;
    std::vector<TextSegment> m_document_segments;
    mutable std::mutex m_doc_mutex;
};
#endif // DOCUMENT_FORMATTER_H