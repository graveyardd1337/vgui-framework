#include "internal.hpp"
#include <cstring>

namespace vgui {
namespace {
struct Clipboard {
    bool opened;
    explicit Clipboard(HWND window) : opened(OpenClipboard(window) != FALSE) {}
    ~Clipboard() {
        if (opened)
            CloseClipboard();
    }
};
bool copy_text(HWND window, std::string_view text) {
    auto wide = detail::utf16(text);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (wide.size() + 1) * sizeof(wchar_t));
    if (!memory)
        return false;
    void* data = GlobalLock(memory);
    if (!data) {
        GlobalFree(memory);
        return false;
    }
    memcpy(data, wide.c_str(), (wide.size() + 1) * sizeof(wchar_t));
    GlobalUnlock(memory);
    Clipboard clipboard(window);
    if (!clipboard.opened || !EmptyClipboard() || !SetClipboardData(CF_UNICODETEXT, memory)) {
        GlobalFree(memory);
        return false;
    }
    return true;
}
std::string paste_text(HWND window) {
    Clipboard clipboard(window);
    if (!clipboard.opened)
        return {};
    HANDLE memory = GetClipboardData(CF_UNICODETEXT);
    if (!memory)
        return {};
    auto* data = static_cast<const wchar_t*>(GlobalLock(memory));
    if (!data)
        return {};
    size_t count = GlobalSize(memory) / sizeof(wchar_t), length = 0;
    while (length < count && data[length])
        ++length;
    std::string result = detail::utf8(std::wstring_view(data, length));
    GlobalUnlock(memory);
    return detail::single_line(result);
}
}
bool Context::text_entry(std::string_view label, std::string& value, size_t maxLength) {
    auto& s = *impl;
    auto bounds = s.row(46);
    auto key = s.id(label);
    s.render.label(bounds.x, bounds.y, caption(label), theme.text);
    Rect field{bounds.x, bounds.y + 20, bounds.w, 26};
    s.interact(key, field);
    size_t& caret = s.carets[key];
    size_t& anchor = s.anchors[key];
    caret = detail::boundary(value, std::min(caret, value.size()));
    anchor = detail::boundary(value, std::min(anchor, value.size()));
    bool editing = s.focusedId == key && s.keyboard();
    const std::string old = value;
    const float available = std::max(1.f, field.w - 12);
    size_t start = 0;
    auto update_start = [&] {
        start = 0;
        if (editing)
            while (start < caret && s.render.measure(std::string_view(value).substr(
                                        start, caret - start)) > available)
                start = detail::decode(value, start).next;
    };
    update_start();
    if ((s.mousePressed && s.hit(field)) || (s.activeId == key && s.mouseDown)) {
        size_t clicked = start;
        float x = field.x + 6;
        while (clicked < value.size()) {
            size_t next = detail::decode(value, clicked).next;
            float width = s.render.measure(std::string_view(value).substr(clicked, next - clicked));
            if (s.mouseX < x + width / 2)
                break;
            x += width;
            clicked = next;
        }
        if (s.mouseX < field.x + 6 && start)
            clicked = detail::previous(value, start);
        caret = clicked;
        if (s.mousePressed && !s.shift)
            anchor = caret;
    }
    auto erase_selection = [&] {
        size_t from = std::min(caret, anchor), to = std::max(caret, anchor);
        if (from == to)
            return false;
        value.erase(from, to - from);
        caret = anchor = from;
        return true;
    };
    auto insert = [&](std::string_view text) {
        size_t from = std::min(caret, anchor), to = std::max(caret, anchor);
        size_t remaining = value.size() - (to - from);
        size_t room = maxLength > remaining ? maxLength - remaining : 0;
        size_t count = detail::boundary(text, std::min(room, text.size()));
        if (!count)
            return;
        erase_selection();
        value.insert(caret, text.substr(0, count));
        caret += count;
        anchor = caret;
    };
    if (editing) {
        if (s.selectAll) {
            anchor = 0;
            caret = value.size();
        }
        if (s.arrowX || s.homePressed || s.endPressed) {
            if (s.homePressed)
                caret = 0;
            else if (s.endPressed)
                caret = value.size();
            else if (!s.shift && caret != anchor)
                caret = s.arrowX < 0 ? std::min(caret, anchor) : std::max(caret, anchor);
            else
                caret = s.arrowX < 0 ? detail::previous(value, caret)
                                     : detail::decode(value, caret).next;
            if (!s.shift)
                anchor = caret;
        }
        if ((s.copy || s.cut) && caret != anchor) {
            size_t from = std::min(caret, anchor), to = std::max(caret, anchor);
            if (copy_text(s.hwnd, std::string_view(value).substr(from, to - from)) && s.cut)
                erase_selection();
        }
        if (s.paste)
            insert(paste_text(s.hwnd));
        if (s.deletePressed && !erase_selection() && caret < value.size())
            value.erase(caret, detail::decode(value, caret).next - caret);
        for (size_t i = 0; i < s.textInput.size();) {
            auto c = detail::decode(s.textInput, i);
            i = c.next;
            if (c.value == 8) {
                if (!erase_selection() && caret) {
                    size_t prev = detail::previous(value, caret);
                    value.erase(prev, caret - prev);
                    caret = anchor = prev;
                }
            } else if (c.value >= 32 && c.value != 127)
                insert(detail::encode(c.value));
        }
        if (s.escapePressed) {
            s.focusedId.clear();
            editing = false;
        }
    }
    s.render.quad(field, theme.background);
    s.render.bevel(field, true);
    Rect clip = s.render.clip;
    s.render.clip = s.intersect(clip, {field.x + 5, field.y + 3, field.w - 10, field.h - 6});
    update_start();
    if (editing && caret != anchor) {
        size_t from = std::max(start, std::min(caret, anchor)), to = std::max(caret, anchor);
        if (to > from) {
            float x =
                field.x + 6 + s.render.measure(std::string_view(value).substr(start, from - start));
            s.render.quad({x, field.y + 4,
                           s.render.measure(std::string_view(value).substr(from, to - from)), 18},
                          theme.selected);
        }
    }
    s.render.label(field.x + 6, field.y + 6, std::string_view(value).substr(start), theme.text);
    if (editing && (GetTickCount64() / 500) % 2 == 0)
        s.render.quad(
            {field.x + 6 + s.render.measure(std::string_view(value).substr(start, caret - start)),
             field.y + 5, 1, 15},
            theme.text);
    s.render.clip = clip;
    return value != old;
}
}
