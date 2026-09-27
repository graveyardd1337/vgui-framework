#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace vgui::detail {
struct Codepoint {
    uint32_t value;
    size_t next;
};
Codepoint decode(std::string_view text, size_t offset);
size_t previous(std::string_view text, size_t offset);
size_t boundary(std::string_view text, size_t offset);
std::string encode(uint32_t value);
std::wstring utf16(std::string_view text);
std::string utf8(std::wstring_view text);
std::string single_line(std::string_view text);
}
