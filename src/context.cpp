#include "internal.hpp"

namespace vgui {
Context::Context(HWND window) : impl(std::make_unique<Impl>(window, &theme, &style)) {}
Context::Context(HWND window, ID3D11Device* device, ID3D11DeviceContext* context)
    : impl(std::make_unique<Impl>(window, &theme, &style, device, context)) {
    if (!device || !context)
        throw std::invalid_argument("External device and context are required");
}
Context::~Context() = default;
void Context::set_borderless(bool enabled) {
    auto& state = *impl;
    if (state.inFrame)
        throw std::logic_error("Change window style between frames");
    if (state.borderless == enabled)
        return;
    state.borderless = enabled; // WM_NCCALCSIZE arrives during the style change.
    if (enabled) {
        state.savedWindowStyle = GetWindowLongPtrW(state.hwnd, GWL_STYLE);
        GetWindowRect(state.hwnd, &state.savedWindowRect);
        SetWindowLongPtrW(state.hwnd, GWL_STYLE,
                          (state.savedWindowStyle & ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW)) |
                              WS_POPUP | (state.resizable ? WS_THICKFRAME : 0));
        SetWindowPos(state.hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    } else {
        const LONG_PTR liveFlags = WS_VISIBLE | WS_MINIMIZE | WS_MAXIMIZE;
        auto current = GetWindowLongPtrW(state.hwnd, GWL_STYLE);
        SetWindowLongPtrW(state.hwnd, GWL_STYLE,
                          (state.savedWindowStyle & ~liveFlags) | (current & liveFlags));
        const auto& r = state.savedWindowRect;
        SetWindowPos(state.hwnd, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
}
void Context::set_resizable(bool enabled, float width, float height) {
    if (impl->inFrame)
        throw std::logic_error("Change window style between frames");
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0)
        throw std::invalid_argument("Minimum window size must be positive");
    impl->resizable = enabled;
    impl->minWidth = width;
    impl->minHeight = height;
    if (impl->borderless) {
        auto flags = GetWindowLongPtrW(impl->hwnd, GWL_STYLE);
        flags = enabled ? flags | WS_THICKFRAME : flags & ~static_cast<LONG_PTR>(WS_THICKFRAME);
        SetWindowLongPtrW(impl->hwnd, GWL_STYLE, flags);
        SetWindowPos(impl->hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
}
void Context::set_dpi_scale(float scale) {
    if (impl->inFrame)
        throw std::logic_error("Change DPI between frames");
    if (!std::isfinite(scale) || (scale != 0 && (scale < 0.5f || scale > 8)))
        throw std::invalid_argument("DPI scale must be zero (automatic) or 0.5..8");
    impl->requestedScale = scale;
}
float Context::dpi_scale() const {
    return impl->render.scale;
}
bool Context::device_lost() const {
    return impl->render.lost();
}
ID3D11Device* Context::device() const {
    return impl->render.get_device();
}
void Context::reset_device(ID3D11Device* device, ID3D11DeviceContext* context) {
    if (impl->inFrame)
        throw std::logic_error("Reset device between frames");
    impl->render.reset(device, context);
}
void Context::reset_device() {
    if (impl->inFrame)
        throw std::logic_error("Reset device between frames");
    impl->render.reset();
}
bool Context::begin_frame() {
    RECT r{};
    GetClientRect(impl->hwnd, &r);
    return impl->start_frame(IsIconic(impl->hwnd) ? 0 : r.right,
                             IsIconic(impl->hwnd) ? 0 : r.bottom);
}
bool Context::begin_frame(unsigned width, unsigned height) {
    if (!impl->render.external)
        throw std::logic_error("Explicit target size requires external mode");
    return impl->start_frame(width, height);
}
bool Context::Impl::start_frame(unsigned width, unsigned height) {
    if (inFrame)
        throw std::logic_error("Call end_frame before starting another frame");
    if (!std::isfinite(style->padding) || style->padding < 0 ||
        !std::isfinite(style->item_spacing) || style->item_spacing < 0)
        throw std::invalid_argument("Style metrics must be finite and nonnegative");
    if (!width || !height || !render.ready()) {
        reset_input();
        activeId.clear();
        return false;
    }
    try {
        UINT dpi = hwnd ? GetDpiForWindow(hwnd) : 96;
        render.set_scale(requestedScale ? requestedScale
                                        : std::clamp((dpi ? dpi : 96) / 96.f, 0.5f, 8.f));
        render.resize(width, height);
    } catch (const detail::DxError&) {
        if (!render.lost())
            throw;
        reset_input();
        activeId.clear();
        return false;
    }
    mouseX = pointerX / render.scale;
    mouseY = pointerY / render.scale;
    render.clear_frame();
    popupDrawn = false;
    tabOrder.clear();
    hoverSeen = false;
    render.clip = {0, 0, render.width, render.height};
    pointerRoot.clear();
    for (auto it = zOrder.rbegin(); it != zOrder.rend(); ++it)
        if (contains(roots[*it].bounds, mouseX, mouseY)) {
            pointerRoot = *it;
            break;
        }
    if (mousePressed && !pointerRoot.empty() && openPopup.empty()) {
        zOrder.erase(std::remove(zOrder.begin(), zOrder.end(), pointerRoot), zOrder.end());
        zOrder.push_back(pointerRoot);
        if (focusRoot != pointerRoot) {
            focusedId.clear();
            activeId.clear();
        }
        focusRoot = pointerRoot;
    }
    for (auto& entry : roots)
        entry.second.seen = false;
    wheelPanel.clear();
    size_t depth = 0;
    for (auto& entry : scrollPanels) {
        auto& scrollPanel = entry.second;
        if (scrollPanel.seen && scrollPanel.root == pointerRoot &&
            contains(scrollPanel.viewport, mouseX, mouseY) &&
            (wheelPanel.empty() || scrollPanel.depth >= depth)) {
            wheelPanel = entry.first;
            depth = scrollPanel.depth;
        }
        scrollPanel.seen = false;
    }
    if (!openPopup.empty() && (escapePressed || tabPressed ||
                               (mousePressed && !contains(popupBounds, mouseX, mouseY) &&
                                !contains(popupAnchor, mouseX, mouseY)))) {
        openPopup.clear();
        activeId.clear();
        mousePressed = false;
        mouseReleased = false;
    }
    if (tabPressed && !prevTabOrder.empty()) {
        auto it = std::find(prevTabOrder.begin(), prevTabOrder.end(), focusedId);
        int count = static_cast<int>(prevTabOrder.size());
        int index = it == prevTabOrder.end() ? -1 : static_cast<int>(it - prevTabOrder.begin());
        index = shift ? (index < 0 ? count - 1 : (index + count - 1) % count) : (index + 1) % count;
        focusedId = prevTabOrder[index];
    }
    inFrame = true;
    lastItem = {};
    hasItem = false;
    rootId.clear();
    lastKey.clear();
    return true;
}
void Context::end_frame() {
    impl->finish_frame(nullptr);
}
void Context::end_frame(ID3D11RenderTargetView* target) {
    impl->finish_frame(target);
}
void Context::Impl::finish_frame(ID3D11RenderTargetView* target) {
    require_frame();
    if (inPanel)
        throw std::logic_error("Missing end_panel");
    if (!idStack.empty())
        throw std::logic_error("Missing pop_id");
    if (render.external && !target)
        throw std::invalid_argument("External rendering needs a target");
    if (!popupDrawn)
        openPopup.clear();
    render.vertices.insert(render.vertices.end(), render.overlay.begin(), render.overlay.end());
    auto complete = [&] {
        stats = {render.vertices.size(), render.drawCalls};
        if (mouseReleased || !mouseDown)
            activeId.clear();
        prevTabOrder = tabOrder;
        reset_input();
        inFrame = false;
        if (!hoverSeen)
            hoverKey.clear();
        zOrder.erase(std::remove_if(zOrder.begin(), zOrder.end(),
                                    [&](const std::string& id) {
                                        if (roots[id].seen)
                                            return false;
                                        roots.erase(id);
                                        return true;
                                    }),
                     zOrder.end());
        if (!focusRoot.empty() && roots.find(focusRoot) == roots.end()) {
            focusRoot = zOrder.empty() ? std::string{} : zOrder.back();
            focusedId.clear();
            prevTabOrder.clear();
        }
    };
    try {
        render.present(target);
    } catch (const detail::DxError&) {
        complete();
        if (!render.lost())
            throw;
        return; // Standalone recovers on the next begin_frame; external waits for reset_device.
    } catch (...) {
        complete();
        throw;
    }
    complete();
}
void Context::Impl::require_frame() const {
    if (!inFrame)
        throw std::logic_error("UI submission needs begin_frame");
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
