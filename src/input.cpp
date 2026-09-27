#include "internal.hpp"
#include <windowsx.h>

namespace vgui {
void Context::Impl::reset_input() {
    mousePressed = mouseReleased = tabPressed = activatePressed = false;
    arrowX = arrowY = wheel = 0;
    homePressed = endPressed = deletePressed = escapePressed = false;
    selectAll = copy = paste = cut = false;
    textInput.clear();
}
void Context::message(UINT msg, WPARAM wp, LPARAM lp) {
    auto& s = *impl;
    if (msg == WM_MOUSEMOVE || msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) {
        s.pointerX = static_cast<float>(GET_X_LPARAM(lp));
        s.pointerY = static_cast<float>(GET_Y_LPARAM(lp));
        s.mouseX = s.pointerX / s.render.scale;
        s.mouseY = s.pointerY / s.render.scale;
    }
    if (msg == WM_LBUTTONDOWN) {
        s.mouseDown = true;
        s.mousePressed = true;
        SetCapture(s.hwnd);
    }
    if (msg == WM_LBUTTONUP) {
        s.mouseDown = false;
        s.mouseReleased = true;
        ReleaseCapture();
    }
    if (msg == WM_CHAR) {
        uint32_t c = static_cast<uint32_t>(wp);
        if (c >= 0xd800 && c <= 0xdbff) {
            if (s.highSurrogate)
                s.textInput += detail::encode(0xfffd);
            s.highSurrogate = static_cast<wchar_t>(c);
        } else {
            if (s.highSurrogate) {
                if (c >= 0xdc00 && c <= 0xdfff)
                    c = 0x10000 + ((s.highSurrogate - 0xd800) << 10) + (c - 0xdc00);
                else
                    s.textInput += detail::encode(0xfffd);
                s.highSurrogate = 0;
            }
            if (c == 8 || (c >= 32 && c != 127))
                s.textInput += detail::encode(c);
        }
    }
    if (msg == WM_UNICHAR && wp != UNICODE_NOCHAR)
        s.textInput += detail::encode(static_cast<uint32_t>(wp));
    if (msg == WM_MOUSEWHEEL) {
        POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        ScreenToClient(s.hwnd, &p);
        s.pointerX = static_cast<float>(p.x);
        s.pointerY = static_cast<float>(p.y);
        s.mouseX = s.pointerX / s.render.scale;
        s.mouseY = s.pointerY / s.render.scale;
        s.wheel += GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
    }
    if (msg == WM_KILLFOCUS || (msg == WM_CAPTURECHANGED && s.mouseDown)) {
        s.mouseDown = false;
        s.reset_input();
        s.activeId.clear();
        s.openPopup.clear();
        s.focusedId.clear();
        s.highSurrogate = 0;
        s.ctrl = s.shift = false;
    }
    if (msg == WM_KEYUP) {
        if (wp == VK_CONTROL || wp == VK_LCONTROL || wp == VK_RCONTROL)
            s.ctrl = false;
        if (wp == VK_SHIFT || wp == VK_LSHIFT || wp == VK_RSHIFT)
            s.shift = false;
    }
    if (msg == WM_KEYDOWN) {
        if (wp == VK_CONTROL || wp == VK_LCONTROL || wp == VK_RCONTROL)
            s.ctrl = true;
        if (wp == VK_SHIFT || wp == VK_LSHIFT || wp == VK_RSHIFT)
            s.shift = true;
        if (s.ctrl || (GetKeyState(VK_CONTROL) & 0x8000)) {
            if (wp == 'A')
                s.selectAll = true;
            if (wp == 'C')
                s.copy = true;
            if (wp == 'V')
                s.paste = true;
            if (wp == 'X')
                s.cut = true;
        }
        if (wp == VK_TAB && !(lp & (1LL << 30)))
            s.tabPressed = true;
        if ((wp == VK_SPACE || wp == VK_RETURN) && !(lp & (1LL << 30)))
            s.activatePressed = true;
        if (wp == VK_LEFT)
            s.arrowX = -1;
        if (wp == VK_RIGHT)
            s.arrowX = 1;
        if (wp == VK_UP)
            s.arrowY = -1;
        if (wp == VK_DOWN)
            s.arrowY = 1;
        if (wp == VK_HOME)
            s.homePressed = true;
        if (wp == VK_END)
            s.endPressed = true;
        if (wp == VK_DELETE)
            s.deletePressed = true;
        if (wp == VK_ESCAPE)
            s.escapePressed = true;
    }
}
bool Context::window_message(UINT msg, WPARAM wp, LPARAM lp, LRESULT& result) {
    message(msg, wp, lp);
    auto& s = *impl;
    if (msg == WM_UNICHAR && wp == UNICODE_NOCHAR) {
        result = TRUE;
        return true;
    }
    if (msg == WM_DPICHANGED && lp) {
        const auto* r = reinterpret_cast<const RECT*>(lp);
        SetWindowPos(s.hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        result = 0;
        return true;
    }
    if (!s.borderless)
        return false;
    if (msg == WM_NCCALCSIZE && wp) {
        result = 0;
        return true;
    }
    if (msg == WM_GETMINMAXINFO && lp) {
        auto* size = reinterpret_cast<MINMAXINFO*>(lp);
        const float scale =
            s.requestedScale ? s.requestedScale : std::max(96u, GetDpiForWindow(s.hwnd)) / 96.f;
        size->ptMinTrackSize = {static_cast<LONG>(std::ceil(s.minWidth * scale)),
                                static_cast<LONG>(std::ceil(s.minHeight * scale))};
        result = 0;
        return true;
    }
    if (msg == WM_NCHITTEST && s.resizable && !IsZoomed(s.hwnd)) {
        RECT r{};
        GetWindowRect(s.hwnd, &r);
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        if (x < r.left || x >= r.right || y < r.top || y >= r.bottom)
            return false;
        int edge = static_cast<int>(std::ceil(5 * s.render.scale));
        bool left = x < r.left + edge, right = x >= r.right - edge, top = y < r.top + edge,
             bottom = y >= r.bottom - edge;
        result = top      ? (left    ? HTTOPLEFT
                             : right ? HTTOPRIGHT
                                     : HTTOP)
                 : bottom ? (left    ? HTBOTTOMLEFT
                             : right ? HTBOTTOMRIGHT
                                     : HTBOTTOM)
                 : left   ? HTLEFT
                 : right  ? HTRIGHT
                          : HTCLIENT;
        return result != HTCLIENT;
    }
    return false;
}
}
