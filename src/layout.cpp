#include "internal.hpp"

namespace vgui {
Rect Context::Impl::row(float height) {
    require_frame();
    if (!inPanel)
        throw std::logic_error("Widget requires begin_window or begin_panel");
    float x = panel.x + style->padding;
    float y = cursorY;
    if (inlineNext) {
        x = lastBounds.x + lastBounds.w + lineSpacing;
        y = lastBounds.y;
    }
    float available = std::max(1.f, panel.x + panel.w - style->padding - x);
    float width = nextWidth > 0 ? std::min(nextWidth, available) : available;
    Rect bounds{x, y, width, height};
    cursorY = std::max(cursorY, y + height + style->item_spacing);
    nextWidth = 0;
    inlineNext = false;
    lastBounds = bounds;
    hasItem = true;
    lastItem = {bounds, hit(bounds), false, false};
    return bounds;
}

std::string Context::Impl::id(std::string_view label) {
    std::string result = panelId;
    for (const auto& scope : idStack)
        result += "/" + std::to_string(scope.size()) + ":" + scope;
    return result + "/" + std::to_string(label.size()) + ":" + std::string(label);
}

void Context::Impl::record_item(const std::string& key, Rect bounds) {
    auto belongs = [&](const std::string& item) {
        return item == key || item.compare(0, key.size() + 1, key + "/") == 0;
    };
    lastItem = {bounds, hit(bounds), belongs(activeId), belongs(focusedId)};
}

bool Context::Impl::hit(Rect r) {
    return (openPopup.empty() || inPopup) && contains(r, mouseX, mouseY) &&
           contains(render.clip, mouseX, mouseY);
}

bool Context::Impl::interact(const std::string& key, Rect r) {
    tabOrder.push_back(key);
    if (mousePressed && hit(r) && activeId.empty()) {
        activeId = key;
        focusedId = key;
    }
    record_item(key, r);
    return (mouseReleased && activeId == key && hit(r)) ||
           (focusedId == key && activatePressed && (openPopup.empty() || inPopup));
}

Rect Context::Impl::intersect(Rect a, Rect b) {
    float x = std::max(a.x, b.x), y = std::max(a.y, b.y);
    return {x, y, std::max(0.f, std::min(a.x + a.w, b.x + b.w) - x),
            std::max(0.f, std::min(a.y + a.h, b.y + b.h) - y)};
}

void Context::begin_window(std::string_view title, Rect& bounds, WindowMode mode) {
    auto& state = *impl;
    state.require_frame();
    if (state.inPanel)
        throw std::logic_error("Windows cannot be nested; use begin_panel");
    state.hasItem = false;
    state.inlineNext = false;
    state.nextWidth = 0;
    state.panelIdDepth = state.idStack.size();
    if (mode == WindowMode::Native)
        bounds = {0, 0, static_cast<float>(state.render.width),
                  static_cast<float>(state.render.height)};
    if (mode == WindowMode::Panel) {
        bounds.w = std::max(bounds.w, 180.f);
        bounds.h = std::max(bounds.h, 80.f);
    }
    state.panelId = std::to_string(title.size()) + ":" + std::string(title);
    auto drag = state.id("@drag");
    Rect bar{bounds.x, bounds.y, bounds.w, 27};
    if (state.mousePressed && state.hit(bar) && state.activeId.empty()) {
        if (mode == WindowMode::Native) {
            ReleaseCapture();
            POINT cursor{};
            GetCursorPos(&cursor);
            SendMessageW(state.hwnd, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(cursor.x, cursor.y));
            state.mouseDown = false;
            state.reset_input();
        } else {
            state.activeId = drag;
            state.dragOffsetX = state.mouseX - bounds.x;
            state.dragOffsetY = state.mouseY - bounds.y;
        }
    }
    if (state.activeId == drag && state.mouseDown) {
        bounds.x = state.mouseX - state.dragOffsetX;
        bounds.y = state.mouseY - state.dragOffsetY;
    }
    bounds.x = std::clamp(bounds.x, 0.f, std::max(0.f, state.render.width - bounds.w));
    bounds.y = std::clamp(bounds.y, 0.f, std::max(0.f, state.render.height - 27.f));
    state.panel = bounds;
    state.inPanel = true;
    state.render.quad({bounds.x + 4, bounds.y + 4, bounds.w, bounds.h}, {0, 0, 0, 85});
    state.render.quad(bounds, theme.panel);
    state.render.bevel(bounds);
    state.render.clip = {bounds.x + 2, bounds.y + 2, bounds.w - 4, bounds.h - 4};
    state.render.label(bounds.x + 10, bounds.y + 7, caption(title), theme.text);
    state.render.quad({bounds.x + 8, bounds.y + 27, bounds.w - 16, 1}, theme.shadow);
    state.render.clip = {bounds.x + 2, bounds.y + 29, bounds.w - 4, bounds.h - 31};
    state.cursorY = bounds.y + 39;
}

void Context::end_window() {
    if (!impl->layouts.empty())
        throw std::logic_error("Close child panels before end_window");
    end_panel();
}

void Context::begin_panel(std::string_view title, Rect bounds) {
    auto& state = *impl;
    state.require_frame();
    if (state.inPanel) {
        state.layouts.push_back({state.panel, state.render.clip, state.cursorY, state.panelId,
                                 state.lastBounds, state.hasItem, state.panelIdDepth});
        bounds.x += state.panel.x;
        bounds.y += state.panel.y;
        state.panelId += "/" + std::to_string(title.size()) + ":" + std::string(title);
    } else
        state.panelId = std::to_string(title.size()) + ":" + std::string(title);
    state.panelIdDepth = state.idStack.size();
    state.hasItem = false;
    state.inlineNext = false;
    state.nextWidth = 0;
    bounds.w = std::max(40.f, bounds.w);
    bounds.h = std::max(40.f, bounds.h);
    state.render.quad(bounds, theme.panel);
    state.render.bevel(bounds, true);
    state.render.clip = state.intersect(state.render.clip,
                                        {bounds.x + 2, bounds.y + 2, bounds.w - 4, bounds.h - 4});
    state.render.label(bounds.x + 10, bounds.y + 9, caption(title), theme.accent);
    state.panel = bounds;
    state.cursorY = bounds.y + 33;
    state.inPanel = true;
    state.render.clip = state.intersect(state.render.clip,
                                        {bounds.x + 2, bounds.y + 30, bounds.w - 4, bounds.h - 32});
}

void Context::end_panel() {
    auto& state = *impl;
    if (!state.inPanel)
        throw std::logic_error("No panel open");
    if (state.idStack.size() != state.panelIdDepth)
        throw std::logic_error("Balance push_id/pop_id before closing a container");
    state.inlineNext = false;
    state.nextWidth = 0;
    if (!state.layouts.empty()) {
        auto parent = state.layouts.back();
        state.layouts.pop_back();
        state.panel = parent.panel;
        state.render.clip = parent.clip;
        state.cursorY = parent.cursorY;
        state.panelId = parent.id;
        state.lastBounds = parent.lastBounds;
        state.hasItem = parent.hasItem;
        state.panelIdDepth = parent.idDepth;
    } else {
        state.inPanel = false;
        state.hasItem = false;
        state.render.clip = {0, 0, static_cast<float>(state.render.width),
                             static_cast<float>(state.render.height)};
    }
}

void Context::set_next_item_width(float width) {
    impl->require_frame();
    if (!std::isfinite(width) || width < 0)
        throw std::invalid_argument("Item width must be finite and nonnegative");
    if (!impl->inPanel)
        throw std::logic_error("Item width requires a container");
    impl->nextWidth = width;
}

void Context::same_line(float spacing) {
    auto& state = *impl;
    state.require_frame();
    if (!state.inPanel || !state.hasItem)
        throw std::logic_error("same_line requires a preceding widget in this container");
    if (!std::isfinite(spacing))
        throw std::invalid_argument("Spacing must be finite");
    state.inlineNext = true;
    state.lineSpacing = spacing < 0 ? style.item_spacing : spacing;
}

void Context::spacing(float pixels) {
    auto& state = *impl;
    state.require_frame();
    if (!state.inPanel)
        throw std::logic_error("Spacing requires a container");
    if (!std::isfinite(pixels))
        throw std::invalid_argument("Spacing must be finite");
    state.cursorY += pixels < 0 ? style.item_spacing : pixels;
    state.inlineNext = false;
}

void Context::separator() {
    auto& state = *impl;
    auto bounds = state.row(2);
    state.render.quad({bounds.x, bounds.y, bounds.w, 1}, theme.shadow);
    state.render.quad({bounds.x, bounds.y + 1, bounds.w, 1}, theme.light);
}

void Context::push_id(std::string_view value) {
    impl->require_frame();
    impl->idStack.emplace_back(value);
}
void Context::push_id(int value) {
    push_id(std::to_string(value));
}
void Context::pop_id() {
    auto& state = *impl;
    state.require_frame();
    if (state.idStack.empty() || (state.inPanel && state.idStack.size() <= state.panelIdDepth))
        throw std::logic_error("pop_id has no matching push_id in this scope");
    state.idStack.pop_back();
}
}
