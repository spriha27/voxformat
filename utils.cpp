#include "utils.h"
#include <iostream>

std::string to_lower_util(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return s;
}

std::string trim_string_util(const std::string& str) {
    const std::string whitespace = " \t\n\r\f\v";
    size_t start = str.find_first_not_of(whitespace);
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, (end - start + 1));
}

std::string cleanup_stt_artifacts_util(std::string text) {
    std::string result = trim_string_util(text);
    if (result.empty()) return result;
    try {
        result = std::regex_replace(result, std::regex("\\[BLANK_AUDIO\\]", std::regex_constants::icase), "");
        result = std::regex_replace(result, std::regex("\\(sighs\\)", std::regex_constants::icase), "");
        result = std::regex_replace(result, std::regex("\\[\\s*Silence\\s*\\]", std::regex_constants::icase), "");
        result = std::regex_replace(result, std::regex("\\(silence\\)", std::regex_constants::icase), "");
        result = std::regex_replace(result, std::regex("\\(um\\)", std::regex_constants::icase), "");
        result = std::regex_replace(result, std::regex("\\(uh\\)", std::regex_constants::icase), "");
        result = std::regex_replace(result, std::regex("\\[noise\\]", std::regex_constants::icase), "");
        result = std::regex_replace(result, std::regex("\\[Laughter\\]", std::regex_constants::icase), "");
        result = std::regex_replace(result, std::regex("\\(music\\)", std::regex_constants::icase), "");
        result = std::regex_replace(result, std::regex("\\[music\\]", std::regex_constants::icase), "");
    } catch (const std::regex_error& e) {
        std::cerr << "Regex error during artifact cleaning: " << e.what() << std::endl;
    }
    result = trim_string_util(result);
    try {
        result = std::regex_replace(result, std::regex(" {2,}"), " ");
    } catch (const std::regex_error& e) {
        std::cerr << "Regex error during space consolidation: " << e.what() << std::endl;
    }
    return trim_string_util(result);
}