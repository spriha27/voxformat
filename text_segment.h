#pragma once
#include <string>
#include <ostream>

struct TextSegment {
    std::string text;
    bool is_bold = false;
    bool is_italic = false;
    bool is_underlined = false;
};

inline std::ostream& operator<<(std::ostream& os, const TextSegment& segment)
{
    os << "Text: \"" << segment.text << "\" [";
    if (segment.is_bold) os << "Bold, ";
    if (segment.is_italic) os << "Italic, ";
    if (segment.is_underlined) os << "Underlined";
    os << "]";
    return os;
}