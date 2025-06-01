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
    // std::cout << "  [DF Cleaned Segment Input] \"" << original_segment_text << "\"" << std::endl;

    std::string text_left_to_parse = original_segment_text;

    if (!m_document_segments.empty() && !text_left_to_parse.empty()) {
        const auto& last_doc_seg = m_document_segments.back();
        if (!last_doc_seg.text.empty() && last_doc_seg.text.back() != ' ') {
            if (text_left_to_parse.front() != ' ') {
                bool prev_ends_punct = !last_doc_seg.text.empty() && (std::string("([{\"").find(last_doc_seg.text.back()) != std::string::npos);
                bool curr_starts_punct = std::string(",.!?;:'\")").find(text_left_to_parse.front()) != std::string::npos;
                if (!prev_ends_punct && !curr_starts_punct) {
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
    const std::string CMD_SI_L = "start italic";   const std::string CMD_XI_L = "stop italic";
    const std::string CMD_XA_L = "stop application"; const std::string CMD_SAVE_L = "save";

    size_t current_pos_in_original = 0;

    while(current_pos_in_original < text_left_to_parse.length()) {
        std::string remaining_original_case = text_left_to_parse.substr(current_pos_in_original);
        std::string remaining_lower_case = to_lower_util(remaining_original_case);

        size_t keyword_loc_relative = remaining_lower_case.find(KW_PREFIX_L);
        size_t actual_keyword_start_in_text_left = (keyword_loc_relative == std::string::npos) ?
                                           std::string::npos :
                                           current_pos_in_original + keyword_loc_relative;

        if (actual_keyword_start_in_text_left == std::string::npos) {
            std::string text_part = trim_string_util(remaining_original_case);
            if (!text_part.empty()) {
                m_document_segments.push_back({text_part, m_is_bold_active, m_is_italic_active});
            }
            break;
        }

        if (actual_keyword_start_in_text_left > current_pos_in_original) {
            std::string text_part = trim_string_util(text_left_to_parse.substr(current_pos_in_original, actual_keyword_start_in_text_left - current_pos_in_original));
            if (!text_part.empty()) {
                m_document_segments.push_back({text_part, m_is_bold_active, m_is_italic_active});
            }
        }

        std::string action_candidate_original_after_keyword = "";
        size_t start_of_action_candidate = actual_keyword_start_in_text_left + KW_PREFIX_L.length();
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
                std::cout << "    COMMAND: BOLD ON" << std::endl;
            } else if (action_candidate_lower.rfind(CMD_XB_L, 0) == 0 && (action_candidate_lower.length() == CMD_XB_L.length() || !isalnum(action_candidate_lower[CMD_XB_L.length()]))) {
                m_is_bold_active = false; matched_action_constant_l = CMD_XB_L; cmd_recognized = true;
                std::cout << "    COMMAND: BOLD OFF" << std::endl;
            } else if (action_candidate_lower.rfind(CMD_SI_L, 0) == 0 && (action_candidate_lower.length() == CMD_SI_L.length() || !isalnum(action_candidate_lower[CMD_SI_L.length()]))) {
                m_is_italic_active = true; matched_action_constant_l = CMD_SI_L; cmd_recognized = true;
                std::cout << "    COMMAND: ITALIC ON" << std::endl;
            } else if (action_candidate_lower.rfind(CMD_XI_L, 0) == 0 && (action_candidate_lower.length() == CMD_XI_L.length() || !isalnum(action_candidate_lower[CMD_XI_L.length()]))) {
                m_is_italic_active = false; matched_action_constant_l = CMD_XI_L; cmd_recognized = true;
                std::cout << "    COMMAND: ITALIC OFF" << std::endl;
            } else if (action_candidate_lower.rfind(CMD_XA_L, 0) == 0 && (action_candidate_lower.length() == CMD_XA_L.length() || !isalnum(action_candidate_lower[CMD_XA_L.length()]))) {
                signal_stop_application(); matched_action_constant_l = CMD_XA_L; cmd_recognized = true;
                std::cout << "    COMMAND: STOP APPLICATION" << std::endl;
            } else if (action_candidate_lower.rfind(CMD_SAVE_L, 0) == 0) {
                std::string filename_param_lower = "output"; // Default
                size_t consumed_action_part_len = CMD_SAVE_L.length();

                if (action_candidate_lower.length() > CMD_SAVE_L.length() && action_candidate_lower[CMD_SAVE_L.length()] == ' ') {
                     filename_param_lower = trim_string_util(action_candidate_lower.substr(CMD_SAVE_L.length() + 1));
                     if (filename_param_lower.empty()) filename_param_lower = "output"; // If "format save  "
                     else consumed_action_part_len += (1 + filename_param_lower.length()); // "save" + " " + filename
                } else if (action_candidate_lower.length() != CMD_SAVE_L.length()) {
                    // It's "saveSOMETHING" without a space, not a valid save command for us
                    filename_param_lower = ""; // Mark as invalid to prevent save
                }
                // else: it's just "save", filename_param_lower remains "output", consumed_action_part_len is CMD_SAVE_L.length()


                if (!filename_param_lower.empty()) {
                    std::string final_filename_base = filename_param_lower;
                    if (final_filename_base.length() > 4 && final_filename_base.substr(final_filename_base.length() - 4) == ".txt") {
                        final_filename_base = final_filename_base.substr(0, final_filename_base.length() - 4);
                    }
                    if (!final_filename_base.empty()) {
                        save_document_to_file(final_filename_base); // save_document_to_file adds .txt
                        matched_action_constant_l = CMD_SAVE_L + (filename_param_lower == "output" && action_candidate_lower.length() == CMD_SAVE_L.length() ? "" : " " + filename_param_lower);
                        total_consumed_for_command_phrase = KW_PREFIX_L.length() + consumed_action_part_len; // Use actual consumed length
                        cmd_recognized = true;
                        std::cout << "    COMMAND: SAVE " << final_filename_base << ".txt" << std::endl;
                    }
                }
                 if (!cmd_recognized) { std::cerr << "  COMMAND: SAVE (format error or no filename)." << std::endl;}
            }
        }

        if (cmd_recognized) {
            // If command was recognized, total_consumed_for_command_phrase was updated specifically for save.
            // For other commands, it's keyword + matched action constant length.
            if (matched_action_constant_l.rfind(CMD_SAVE_L,0) != 0) { // If not save command
                 total_consumed_for_command_phrase = KW_PREFIX_L.length() + matched_action_constant_l.length();
            }
            // This advancement needs to be more precise by finding where matched_action_constant_l
            // *actually ends* in the original case `action_candidate_original_after_keyword`
            // For simplicity, we are advancing by length of lowercase constants.
        } else {
            std::string literal_keyword_part = trim_string_util(text_left_to_parse.substr(actual_keyword_start_in_text_left, KW_PREFIX_L.length()));
             if (!literal_keyword_part.empty()) {
                 m_document_segments.push_back({literal_keyword_part, m_is_bold_active, m_is_italic_active});
            }
            // total_consumed_for_command_phrase remains KW_PREFIX_L.length()
        }

        current_pos_in_original = actual_keyword_start_in_text_left + total_consumed_for_command_phrase;

        while (current_pos_in_original < text_left_to_parse.length() && text_left_to_parse[current_pos_in_original] == ' ') {
            current_pos_in_original++;
        }
    }
}

void DocumentFormatter::print_current_document_preview() const {
    std::lock_guard<std::mutex> lock(m_doc_mutex);
    std::cout << "\n\n--- DOCUMENT PREVIEW ---\n";
    std::string md_output_str = "";
    for (size_t i = 0; i < m_document_segments.size(); ++i) {
        const auto& seg = m_document_segments[i];
        std::string current_text = seg.text;
        if (current_text.empty() && !(seg.is_bold || seg.is_italic)) continue;
        if (!md_output_str.empty()) {
            md_output_str += " ";
        }
        std::string text_to_append = current_text;
        if (seg.is_bold && seg.is_italic) { text_to_append = "***" + text_to_append + "***"; }
        else if (seg.is_italic) { text_to_append = "*" + text_to_append + "*"; }
        else if (seg.is_bold) { text_to_append = "**" + text_to_append + "**"; }
        md_output_str += text_to_append;
    }
    try {
        md_output_str = std::regex_replace(md_output_str, std::regex(" {2,}"), " ");
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
        std::string current_text = seg.text;
        if (current_text.empty() && !(seg.is_bold || seg.is_italic)) continue;
        if (!md_output_str.empty()) {
            md_output_str += " ";
        }
        std::string text_to_append = current_text;
        if (seg.is_bold && seg.is_italic) { text_to_append = "***" + text_to_append + "***"; }
        else if (seg.is_italic) { text_to_append = "*" + text_to_append + "*"; }
        else if (seg.is_bold) { text_to_append = "**" + text_to_append + "**"; }
        md_output_str += text_to_append;
    }
    try {
        md_output_str = std::regex_replace(md_output_str, std::regex(" {2,}"), " ");
        md_output_str = trim_string_util(md_output_str);
    } catch (const std::regex_error& e) {}
    return md_output_str;
}

void DocumentFormatter::save_document_to_file(const std::string& relative_filename_no_ext) const {
    // Generate raw text for .txt file
    std::string raw_text_doc = "";
    {
        std::lock_guard<std::mutex> lock(m_doc_mutex);
        for (size_t i = 0; i < m_document_segments.size(); ++i) {
            const auto& seg = m_document_segments[i];
            if (seg.text.empty()) continue;
            if (i > 0 && !raw_text_doc.empty() && raw_text_doc.back() != ' ') {
                if (seg.text.front() != ' ' &&
                    std::string(",.!?;:'\"").find(seg.text.front()) == std::string::npos &&
                    std::string("([{\"").find(raw_text_doc.back()) == std::string::npos) {
                    raw_text_doc += " ";
                }
            }
            raw_text_doc += seg.text;
        }
        raw_text_doc = trim_string_util(raw_text_doc); // Final trim for raw text
    }

    std::string output_dir_str = "outputs";
    fs::path output_dir_path(output_dir_str);

    try {
        if (!fs::exists(output_dir_path)) {
            if (!fs::create_directory(output_dir_path)) {
                std::cerr << "--- Error: Could not create directory " << output_dir_str << " ---" << std::endl;
                return;
            }
        }
        std::string full_path_str = output_dir_str + "/" + relative_filename_no_ext + ".txt";
        fs::path full_path(full_path_str);

        std::ofstream outfile(full_path);
        if (outfile.is_open()) {
            outfile << raw_text_doc;
            outfile.close();
            std::cout << "--- Document (raw text) saved to " << full_path_str << " ---" << std::endl;
        } else {
            std::cerr << "--- Error: Could not open file " << full_path_str << " for saving. ---" << std::endl;
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "--- Filesystem Error: " << e.what() << " ---" << std::endl;
    }
}
void DocumentFormatter::signal_stop_application() {
    m_should_stop_application.store(true, std::memory_order_release);
}