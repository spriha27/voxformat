#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <algorithm>
#include <cctype>
#include <regex>

std::string to_lower_util(std::string s);
std::string trim_string_util(const std::string& str);
std::string cleanup_stt_artifacts_util(std::string text);

#endif // UTILS_H