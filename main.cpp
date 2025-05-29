#include <iostream>
#include "text_segment.h"

std::ostream& operator<<(std::ostream& os, const TextSegment& segment)
{
    os << "Text: \"" << segment.text << "\" [";
    if (segment.is_bold) os << "Bold, ";
    if (segment.is_italic) os << "Italic, ";
    if (segment.is_underlined) os << "Underlined";
    os << "]";
    return os;
}

int main()
{
    TextSegment text{"Hello", true, true, false};
    std::cout << text << std::endl;
    return 0;
}
