// document_formatter.cpp
#include "document_formatter.h"
#include "utils.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <regex>

namespace fs = std::filesystem;

DocumentFormatter::DocumentFormatter() : m_is_bold_active(false), m_is_italic_active(false) {
    m_should_stop_application.store(false);
}

void DocumentFormatter::clear_document(){
    std::lock_guard<std::mutex> lock(m_doc_mutex);
    m_document_segments.clear();
    m_is_bold_active = false;
    m_is_italic_active = false;
}

void DocumentFormatter::process_transcribed_text(const std::string& text_from_whisper_raw) {
    std::lock_guard<std::mutex> lock(m_doc_mutex);

    std::string original_segment_text = cleanup_stt_artifacts_util(text_from_whisper_raw);
    if (original_segment_text.empty()) {
        return;
    }

    std::string text_left_to_parse = original_segment_text;

    if (!m_document_segments.empty() && !text_left_to_parse.empty()) {
        const auto& last_doc_seg = m_document_segments.back();
        if (!last_doc_seg.text.empty() && last_doc_seg.text.back() != ' ') {
            if (text_left_to_parse.front() != ' ') {
                bool prev_ends_special = !last_doc_seg.text.empty() && (std::string("([{\"*").find(last_doc_seg.text.back()) != std::string::npos); // Added *
                bool curr_starts_special = std::string(",.!?;:'\")*").find(text_left_to_parse.front()) != std::string::npos; // Added *
                if (!prev_ends_special && !curr_starts_special) {
                    if (last_doc_seg.is_bold == m_is_bold_active && last_doc_seg.is_italic == m_is_italic_active) {
                        if (!last_doc_seg.text.empty()) m_document_segments.back().text += " ";
                        else m_document_segments.push_back({" ", m_is_bold_active, m_is_italic_active});
                    } else {
                        m_document_segments.push_back({" ", m_is_bold_active, m_is_italic_active});
                    }
                }
            }
        }
    }

    const std::string KW_PREFIX_L = "format ";
    const std::string CMD_SB_L = "start bold";     const std::string CMD_XB_L = "stop bold";
    const std::string CMD_SI_L = "start italics";   const std::string CMD_XI_L = "stop italics";
    const std::string CMD_XA_L = "stop application";
    // Save command removed

    size_t current_pos = 0;
    while(current_pos < text_left_to_parse.length()) {
        std::string remaining_original_case = text_left_to_parse.substr(current_pos);
        std::string remaining_lower_case = to_lower_util(remaining_original_case);

        size_t keyword_loc_relative = remaining_lower_case.find(KW_PREFIX_L);
        size_t actual_keyword_start = (keyword_loc_relative == std::string::npos) ?
                                           std::string::npos :
                                           current_pos + keyword_loc_relative;

        if (actual_keyword_start == std::string::npos) {
            std::string text_part = trim_string_util(remaining_original_case);
            if (!text_part.empty()) {
                m_document_segments.push_back({text_part, m_is_bold_active, m_is_italic_active});
            }
            break;
        }

        if (actual_keyword_start > current_pos) {
            std::string text_part = trim_string_util(text_left_to_parse.substr(current_pos, actual_keyword_start - current_pos));
            if (!text_part.empty()) {
                m_document_segments.push_back({text_part, m_is_bold_active, m_is_italic_active});
            }
        }

        std::string action_candidate_original_after_keyword = "";
        size_t start_of_action_candidate = actual_keyword_start + KW_PREFIX_L.length();
        if(start_of_action_candidate < text_left_to_parse.length()){
             action_candidate_original_after_keyword = text_left_to_parse.substr(start_of_action_candidate);
        }
        std::string action_candidate_lower = trim_string_util(to_lower_util(action_candidate_original_after_keyword));

        size_t total_consumed_for_command_phrase = KW_PREFIX_L.length();
        bool cmd_recognized = false;
        std::string matched_action_constant_l = "";


        if (!action_candidate_lower.empty()) {
            if (action_candidate_lower.rfind(CMD_SB_L, 0) == 0 && (action_candidate_lower.length() == CMD_SB_L.length() || !isalnum(action_candidate_lower[CMD_SB_L.length()]))) {
                m_is_bold_active = true; matched_action_constant_l = CMD_SB_L; cmd_recognized = true;
            } else if (action_candidate_lower.rfind(CMD_XB_L, 0) == 0 && (action_candidate_lower.length() == CMD_XB_L.length() || !isalnum(action_candidate_lower[CMD_XB_L.length()]))) {
                m_is_bold_active = false; matched_action_constant_l = CMD_XB_L; cmd_recognized = true;
            } else if (action_candidate_lower.rfind(CMD_SI_L, 0) == 0 && (action_candidate_lower.length() == CMD_SI_L.length() || !isalnum(action_candidate_lower[CMD_SI_L.length()]))) {
                m_is_italic_active = true; matched_action_constant_l = CMD_SI_L; cmd_recognized = true;
            } else if (action_candidate_lower.rfind(CMD_XI_L, 0) == 0 && (action_candidate_lower.length() == CMD_XI_L.length() || !isalnum(action_candidate_lower[CMD_XI_L.length()]))) {
                m_is_italic_active = false; matched_action_constant_l = CMD_XI_L; cmd_recognized = true;
            } else if (action_candidate_lower.rfind(CMD_XA_L, 0) == 0 && (action_candidate_lower.length() == CMD_XA_L.length() || !isalnum(action_candidate_lower[CMD_XA_L.length()]))) {
                signal_stop_application(); matched_action_constant_l = CMD_XA_L; cmd_recognized = true;
            }
        }

        if (cmd_recognized) {
            total_consumed_for_command_phrase = KW_PREFIX_L.length() + matched_action_constant_l.length();
        } else {
            std::string literal_keyword_part = trim_string_util(text_left_to_parse.substr(actual_keyword_start, KW_PREFIX_L.length()));
             if (!literal_keyword_part.empty()) {
                 m_document_segments.push_back({literal_keyword_part, m_is_bold_active, m_is_italic_active});
            }
        }
        current_pos = actual_keyword_start + total_consumed_for_command_phrase;
        while (current_pos < text_left_to_parse.length() && text_left_to_parse[current_pos] == ' ') {
            current_pos++;
        }
    }
}

void DocumentFormatter::print_current_document_preview() const {
    std::lock_guard<std::mutex> lock(m_doc_mutex);
    std::cout << "\n\n--- DOCUMENT PREVIEW ---\n";
    std::string md_output_str = "";
    for (size_t i = 0; i < m_document_segments.size(); ++i) {
        const auto& seg = m_document_segments[i];
        std::string current_segment_text = seg.text; // This should be pre-trimmed
        while (!current_segment_text.empty() &&
           std::isspace((unsigned char)current_segment_text.back())) {
            current_segment_text.pop_back();
           }

        // Skip adding this segment if it's empty and carries no formatting change
        // This check helps avoid "** **" if an empty segment was created with bold on.
        if (current_segment_text.empty() &&
            !(i > 0 && (seg.is_bold != m_document_segments[i-1].is_bold || seg.is_italic != m_document_segments[i-1].is_italic)) &&
            !(i == 0 && (seg.is_bold || seg.is_italic))) { // Don't skip if it's an initial empty formatting segment
            continue;
        }

        // Add a space separator if:
        // 1. md_output_str is not empty (i.e., this isn't the first piece of content).
        // 2. md_output_str doesn't already end with a space.
        // 3. current_segment_text doesn't start with a space (it shouldn't if trimmed).
        // 4. And no intervening punctuation that makes a space look weird.
        if (!md_output_str.empty() && !current_segment_text.empty()) {
            if (md_output_str.back() != ' ' && current_segment_text.front() != ' ') {
                char last_char_of_md = ' ';
                // Iterate backwards through md_output_str to find the last *actual* text character
                for (long k_md = md_output_str.length() - 1; k_md >= 0; --k_md) {
                    if (md_output_str[k_md] != '*' && md_output_str[k_md] != '_') { // Assuming _ for underline later
                        last_char_of_md = md_output_str[k_md];
                        break;
                    }
                    if (k_md == 0) last_char_of_md = md_output_str[k_md]; // If all were markdown chars
                }

                char first_char_of_new = current_segment_text.front();
                bool prev_ends_opening_markup = (std::string("([{\"*").find(last_char_of_md) != std::string::npos);
                bool current_starts_closing_markup = (std::string(",.!?;:'\")*").find(first_char_of_new) != std::string::npos);

                if (!prev_ends_opening_markup && !current_starts_closing_markup) {
                    md_output_str += " ";
                }
            }
        }

        std::string text_to_append = current_segment_text;
        if (seg.is_bold && seg.is_italic) {
            text_to_append = "***" + text_to_append + "***";
        } else if (seg.is_italic) {
            text_to_append = "*" + text_to_append + "*";
        } else if (seg.is_bold) {
            text_to_append = "**" + text_to_append + "**";
        }
        md_output_str += text_to_append;
    }

    // Regex replace for multiple spaces is good as a final cleanup
    try {
        md_output_str = std::regex_replace(md_output_str, std::regex(" {2,}"), " ");
        // A very specific cleanup for " ** ** " or " * * " which might result from an empty text segment
        // that only carried a formatting change. This is hard to get perfect without looking at segment formatting.
        md_output_str = std::regex_replace(md_output_str, std::regex("\\*\\* \\*\\*"), ""); // ** ** -> ""
        md_output_str = std::regex_replace(md_output_str, std::regex("\\* \\*"), "");       // * *   -> ""
        md_output_str = trim_string_util(md_output_str);
    } catch (const std::regex_error& e) {}

    std::cout << md_output_str << std::endl;
    std::cout << "------------------------\n";
    std::flush(std::cout);
}

std::string DocumentFormatter::get_markdown_document() const {
    std::lock_guard<std::mutex> lock(m_doc_mutex);
    std::string md_output_str = "";

    for (size_t i = 0; i < m_document_segments.size(); ++i) {
        const auto& seg = m_document_segments[i];

        // 1) Copy the raw text…
        std::string current_text = seg.text;

        // 2) RIGHT-TRIM any trailing whitespace, exactly as you did in print…
        while (!current_text.empty() &&
               std::isspace((unsigned char)current_text.back())) {
            current_text.pop_back();
        }

        // 3) Skip entirely if it’s empty and carries no formatting change
        if (current_text.empty() && !(seg.is_bold || seg.is_italic)) {
            continue;
        }

        // 4) Possibly insert a space before this new chunk:
        if (!md_output_str.empty() && !current_text.empty()) {
            if (md_output_str.back() != ' ') {
                if (current_text.front() != ' ') {
                    char last_char_of_md = ' ';
                    if (!md_output_str.empty()) {
                        for (long k_md = md_output_str.length() - 1; k_md >= 0; --k_md) {
                            if (md_output_str[k_md] != '*' &&
                                md_output_str[k_md] != '_') {
                                last_char_of_md = md_output_str[k_md];
                                break;
                            }
                            if (k_md == 0) last_char_of_md = md_output_str[k_md];
                        }
                    }
                    char first_char_of_new = current_text.front();
                    bool no_space_needed = (std::string(",.!?;:'\"").find(first_char_of_new) != std::string::npos)
                                            || (std::string("([{\"*").find(last_char_of_md) != std::string::npos);
                    if (!no_space_needed) {
                        md_output_str += " ";
                    }
                }
            }
        }

        // 5) Wrap in **…** / *…* / ***…*** if needed
        std::string text_to_append = current_text;
        if (seg.is_bold && seg.is_italic) {
            text_to_append = "***" + text_to_append + "***";
        } else if (seg.is_italic) {
            text_to_append = "*" + text_to_append + "*";
        } else if (seg.is_bold) {
            text_to_append = "**" + text_to_append + "**";
        }

        md_output_str += text_to_append;
    }

    // 6) Final clean‐up (collapse multiple spaces to one, trim leading/trailing)
    try {
        md_output_str = std::regex_replace(md_output_str, std::regex(" {2,}"), " ");
        md_output_str = trim_string_util(md_output_str);
    } catch (const std::regex_error&) {
        // ignore if regex fails
    }

    return md_output_str;
}

void DocumentFormatter::save_document_to_file(const std::string& full_filename_path) const {
    // std::cout << "--- SAVE COMMAND: Attempting to save to: " << full_filename_path << " ---" << std::endl;

    // Get the MARKDOWN formatted document
    std::string document_content_to_save = get_markdown_document();

    fs::path output_file_p(full_filename_path);
    fs::path output_dir_p = output_file_p.parent_path();

    try {
        if (!output_dir_p.empty() && !fs::exists(output_dir_p)) {
            if (!fs::create_directories(output_dir_p)) {
                std::cerr << "--- Error: Could not create directory " << output_dir_p.string() << " ---" << std::endl;
                return;
            }
        }
        std::ofstream outfile(output_file_p);
        if (outfile.is_open()) {
            outfile << document_content_to_save; // Save the Markdown content
            outfile.close();
            std::cout << "--- Document saved to " << fs::absolute(output_file_p).string() << " ---" << std::endl;
        } else {
            std::cerr << "--- Error: Could not open file " << output_file_p.string() << " for saving. ---" << std::endl;
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "--- Filesystem Error during save: " << e.what() << " ---" << std::endl;
    }
}

void DocumentFormatter::signal_stop_application() {
    m_should_stop_application.store(true, std::memory_order_release);
}