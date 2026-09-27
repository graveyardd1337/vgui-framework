#include "text.hpp"
#include <algorithm>

namespace vgui::detail {
Codepoint decode(std::string_view text, size_t offset) {
    if (offset >= text.size())
        return {0, text.size()};
    auto byte = [&](size_t i) { return static_cast<unsigned char>(text[i]); };
    unsigned first = byte(offset);
    if (first < 0x80)
        return {first, offset + 1};
    int count = first >= 0xc2 && first <= 0xdf   ? 2
                : first >= 0xe0 && first <= 0xef ? 3
                : first >= 0xf0 && first <= 0xf4 ? 4
                                                 : 0;
    if (!count || offset + count > text.size())
        return {0xfffd, offset + 1};
    uint32_t value = first & ((1u << (7 - count)) - 1);
    for (int i = 1; i < count; ++i) {
        unsigned c = byte(offset + i);
        if ((c & 0xc0) != 0x80)
            return {0xfffd, offset + 1};
        value = (value << 6) | (c & 63);
    }
    if ((count == 2 && value < 0x80) || (count == 3 && value < 0x800) ||
        (count == 4 && value < 0x10000) || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
        return {0xfffd, offset + 1};
    return {value, offset + count};
}
size_t boundary(std::string_view text, size_t offset) {
    size_t i = 0;
    while (i < text.size()) {
        size_t next = decode(text, i).next;
        if (next > offset)
            break;
        i = next;
    }
    return i;
}
size_t previous(std::string_view text, size_t offset) {
    return offset ? boundary(text, offset - 1) : 0;
}
std::string encode(uint32_t c) {
    if (c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
        c = 0xfffd;
    std::string out;
    if (c < 0x80)
        out += static_cast<char>(c);
    else if (c < 0x800) {
        out += static_cast<char>(0xc0 | (c >> 6));
        out += static_cast<char>(0x80 | (c & 63));
    } else if (c < 0x10000) {
        out += static_cast<char>(0xe0 | (c >> 12));
        out += static_cast<char>(0x80 | ((c >> 6) & 63));
        out += static_cast<char>(0x80 | (c & 63));
    } else {
        out += static_cast<char>(0xf0 | (c >> 18));
        out += static_cast<char>(0x80 | ((c >> 12) & 63));
        out += static_cast<char>(0x80 | ((c >> 6) & 63));
        out += static_cast<char>(0x80 | (c & 63));
    }
    return out;
}
std::wstring utf16(std::string_view text) {
    std::wstring out;
    for (size_t i = 0; i < text.size();) {
        auto c = decode(text, i);
        i = c.next;
        if (c.value < 0x10000)
            out += static_cast<wchar_t>(c.value);
        else {
            out += static_cast<wchar_t>(0xd800 + ((c.value - 0x10000) >> 10));
            out += static_cast<wchar_t>(0xdc00 + ((c.value - 0x10000) & 1023));
        }
    }
    return out;
}
std::string utf8(std::wstring_view text) {
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        uint32_t c = text[i];
        if (c >= 0xd800 && c <= 0xdbff && i + 1 < text.size() && text[i + 1] >= 0xdc00 &&
            text[i + 1] <= 0xdfff)
            c = 0x10000 + ((c - 0xd800) << 10) + (text[++i] - 0xdc00);
        out += encode(c);
    }
    return out;
}
std::string single_line(std::string_view text) {
    std::string out;
    for (size_t i = 0; i < text.size();) {
        auto c = decode(text, i);
        i = c.next;
        if (c.value >= 32 && c.value != 127)
            out += encode(c.value);
        else if (c.value == '\n' || c.value == '\t')
            out += ' ';
    }
    return out;
}
}
