#include "internal.hpp"

namespace vgui {
Context::Context(HWND window) : impl(std::make_unique<Impl>(window, &theme, &style)) {}
Context::~Context() = default;

void Context::set_borderless(bool enabled) {
    auto& state = *impl;
    if (state.borderless == enabled)
        return;
    if (state.inFrame)
        throw std::logic_error("Change the host style between frames");
    if (enabled) {
        state.savedWindowStyle = GetWindowLongPtrW(state.hwnd, GWL_STYLE);
        GetWindowRect(state.hwnd, &state.savedWindowRect);
        SetWindowLongPtrW(state.hwnd, GWL_STYLE,
                          (state.savedWindowStyle & ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW)) |
                              WS_POPUP);
        SetWindowPos(state.hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    } else {
        const LONG_PTR liveFlags = WS_VISIBLE | WS_MINIMIZE | WS_MAXIMIZE;
        const auto currentStyle = GetWindowLongPtrW(state.hwnd, GWL_STYLE);
        SetWindowLongPtrW(state.hwnd, GWL_STYLE,
                         (state.savedWindowStyle & ~liveFlags) | (currentStyle & liveFlags));
        const auto& rect = state.savedWindowRect;
        SetWindowPos(state.hwnd, nullptr, rect.left, rect.top, rect.right - rect.left,
                     rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    state.borderless = enabled;
}

bool Context::begin_frame() {
    auto& state = *impl;
    if (state.inFrame)
        throw std::logic_error("Call end_frame before starting another frame");
    if (!std::isfinite(style.padding) || style.padding < 0 || !std::isfinite(style.item_spacing) ||
        style.item_spacing < 0)
        throw std::invalid_argument("Style metrics must be finite and nonnegative");

    RECT client{};
    GetClientRect(state.hwnd, &client);
    if (IsIconic(state.hwnd) || client.right <= 0 || client.bottom <= 0) {
        state.reset_input();
        state.activeId.clear();
        return false;
    }
    state.render.resize(client.right, client.bottom);
    state.render.vertices.clear();
    state.render.overlay.clear();
    state.popupDrawn = false;
    state.tabOrder.clear();
    state.render.clip = {0, 0, static_cast<float>(state.render.width),
                         static_cast<float>(state.render.height)};

    if (!state.openPopup.empty() &&
        (state.escapePressed || state.tabPressed ||
         (state.mousePressed && !contains(state.popupBounds, state.mouseX, state.mouseY) &&
          !contains(state.popupAnchor, state.mouseX, state.mouseY)))) {
        state.openPopup.clear();
        state.activeId.clear();
        state.mousePressed = false;
        state.mouseReleased = false;
    }
    // Use last frame's order: this frame's widgets haven't been added yet.
    if (state.tabPressed && !state.prevTabOrder.empty()) {
        auto it = std::find(state.prevTabOrder.begin(), state.prevTabOrder.end(), state.focusedId);
        int count = static_cast<int>(state.prevTabOrder.size());
        int index =
            it == state.prevTabOrder.end() ? -1 : static_cast<int>(it - state.prevTabOrder.begin());
        if (GetKeyState(VK_SHIFT) & 0x8000)
            index = index < 0 ? count - 1 : (index + count - 1) % count;
        else
            index = (index + 1) % count;
        state.focusedId = state.prevTabOrder[index];
    }

    state.inFrame = true;
    state.lastItem = {};
    state.hasItem = false;
    return true;
}

void Context::end_frame() {
    auto& state = *impl;
    state.require_frame();
    if (state.inPanel)
        throw std::logic_error("Missing end_panel");
    if (!state.idStack.empty())
        throw std::logic_error("Missing pop_id");
    if (!state.popupDrawn)
        state.openPopup.clear();
    state.render.vertices.insert(state.render.vertices.end(), state.render.overlay.begin(),
                                 state.render.overlay.end());
    state.render.present();
    state.stats = {state.render.vertices.size(),
                   state.render.vertices.empty() ? size_t{0} : size_t{1}};
    if (state.mouseReleased || !state.mouseDown)
        state.activeId.clear();
    state.prevTabOrder = state.tabOrder;
    state.reset_input();
    state.inFrame = false;
}

void Context::Impl::require_frame() const {
    if (!inFrame)
        throw std::logic_error("UI submission requires a successful begin_frame");
}

ItemState Context::last_item() const {
    return impl->lastItem;
}
FrameStats Context::frame_stats() const {
    return impl->stats;
}
void Context::set_vsync(bool enabled) {
    impl->render.vsync = enabled;
}
}
