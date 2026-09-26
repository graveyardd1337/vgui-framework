#pragma once
#include "render.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <unordered_map>

namespace vgui {
namespace detail {
inline std::string_view caption(std::string_view text) {
    return text.substr(0, text.find("##"));
}
inline bool contains(Rect bounds, float x, float y) {
    return x >= bounds.x && y >= bounds.y && x < bounds.x + bounds.w && y < bounds.y + bounds.h;
}
}
using detail::caption;
using detail::contains;
// UI state lives here; this header stays private.
struct Context::Impl {
    HWND hwnd;
    bool borderless = false;
    LONG_PTR savedWindowStyle = 0;
    RECT savedWindowRect{};
    detail::Renderer render;
    float mouseX = 0, mouseY = 0, dragOffsetX = 0, dragOffsetY = 0, cursorY = 0;
    bool mouseDown = false, mousePressed = false, mouseReleased = false, tabPressed = false,
         activatePressed = false;
    int arrowX = 0;
    int arrowY = 0, wheel = 0;
    bool homePressed = false, endPressed = false, deletePressed = false, escapePressed = false;
    std::string textInput, openPopup;
    Rect popupBounds{}, popupAnchor{};
    bool inPopup = false, popupDrawn = false;
    std::unordered_map<std::string, int> scrolls;
    std::unordered_map<std::string, size_t> carets;
    struct Layout {
        Rect panel, clip;
        float cursorY;
        std::string id;
        Rect lastBounds;
        bool hasItem;
        size_t idDepth;
    };
    std::vector<Layout> layouts;
    std::string activeId, focusedId, panelId;
    std::vector<std::string> prevTabOrder, tabOrder;
    Rect panel{};
    bool inPanel = false;
    bool inFrame = false;
    bool hasItem = false;
    bool inlineNext = false;
    float nextWidth = 0, lineSpacing = 0;
    Rect lastBounds{};
    ItemState lastItem{};
    FrameStats stats{};
    std::vector<std::string> idStack;
    size_t panelIdDepth = 0;
    Theme* theme;
    Style* style;

    Impl(HWND window, Theme* colors, Style* metrics)
        : hwnd(window), render(window, colors), theme(colors), style(metrics) {}
    void reset_input();
    void require_frame() const;
    void record_item(const std::string& key, Rect bounds);
    Rect row(float height);
    std::string id(std::string_view label);
    bool hit(Rect bounds);
    bool interact(const std::string& key, Rect bounds);
    Rect intersect(Rect a, Rect b);
    bool scrollbar(const std::string& key, Rect bounds, int& position, int total, int visible);
    bool list(const std::string& key, Rect bounds, int* selected, std::vector<bool>* multiple,
              const std::vector<std::string>& items, int count);
};
}
