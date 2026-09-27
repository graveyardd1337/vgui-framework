#pragma once
#include "render.hpp"
#include "text.hpp"
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
    bool resizable = true;
    float minWidth = 180, minHeight = 80, requestedScale = 0;
    float pointerX = 0, pointerY = 0;
    bool ctrl = false, shift = false, selectAll = false, copy = false, paste = false, cut = false;
    wchar_t highSurrogate = 0;
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
    std::unordered_map<std::string, size_t> anchors;
    struct ScrollPanel {
        float offset = 0, contentHeight = 0;
        Rect viewport{};
        size_t depth = 0;
        std::string root;
        bool seen = false;
    };
    std::unordered_map<std::string, ScrollPanel> scrollPanels;
    bool scrolling = false;
    Rect scrollBounds{};
    float scrollStart = 0;
    std::string wheelPanel;
    struct Root {
        Rect bounds;
        bool seen = false;
    };
    std::unordered_map<std::string, Root> roots;
    std::vector<std::string> zOrder;
    std::string rootId, pointerRoot, focusRoot, lastKey, hoverKey;
    ULONGLONG hoverSince = 0;
    bool hoverSeen = false;
    struct Layout {
        Rect panel, clip;
        float cursorY;
        std::string id;
        Rect lastBounds;
        bool hasItem;
        size_t idDepth;
        bool scrolling;
        Rect scrollBounds;
        float scrollStart;
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

    Impl(HWND window, Theme* colors, Style* metrics, ID3D11Device* device = nullptr,
         ID3D11DeviceContext* context = nullptr)
        : hwnd(window), render(window, colors, device, context), theme(colors), style(metrics) {}
    bool start_frame(unsigned width, unsigned height);
    void finish_frame(ID3D11RenderTargetView* target);
    void open_root(const std::string& id, Rect bounds);
    bool keyboard() const {
        return rootId == focusRoot && (openPopup.empty() || inPopup);
    }
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
