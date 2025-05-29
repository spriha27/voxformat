#pragma once
#include <string>
#include <ostream>

struct TextSegment {
    std::string text;
    bool is_bold = false;
    bool is_italic = false;
    bool is_underlined = false;
};

std::ostream& operator<<(std::ostream& os, const TextSegment& text);