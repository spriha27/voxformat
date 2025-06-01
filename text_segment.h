#pragma once
#include <string>
#include <vector>
#include <ostream>

struct TextSegment {
    std::string text;
    bool is_bold = false;
    bool is_italic = false;
    // bool is_underlined = false;

    TextSegment(std::string t = "", bool b = false, bool i = false) : text(std::move(t)), is_bold(b), is_italic(i) {}
};

inline std::ostream& operator<<(std::ostream& os, const TextSegment& segment)
{
    os << "Text: \"" << segment.text << "\" [";
    if (segment.is_bold) os << "Bold, ";
    if (segment.is_italic) os << "Italic, ";
    // if (segment.is_underlined) os << "Underlined";
    os << "]";
    return os;
}