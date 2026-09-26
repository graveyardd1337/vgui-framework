#include "internal.hpp"
#include <windowsx.h>

namespace vgui {
void Context::Impl::reset_input() {
    mousePressed = mouseReleased = tabPressed = activatePressed = false;
    arrowX = arrowY = wheel = 0;
    homePressed = endPressed = deletePressed = escapePressed = false;
    textInput.clear();
}
void Context::message(UINT msg, WPARAM wp, LPARAM lp) {
    auto& state = *impl;
    if (msg == WM_MOUSEMOVE || msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) {
        state.mouseX = static_cast<float>(GET_X_LPARAM(lp));
        state.mouseY = static_cast<float>(GET_Y_LPARAM(lp));
    }
    if (msg == WM_LBUTTONDOWN) {
        state.mouseDown = true;
        state.mousePressed = true;
        SetCapture(state.hwnd);
    }
    if (msg == WM_LBUTTONUP) {
        state.mouseDown = false;
        state.mouseReleased = true;
        ReleaseCapture();
    }
    if (msg == WM_CHAR && (wp == 8 || (wp >= 32 && wp <= 126)))
        state.textInput += static_cast<char>(wp);
    if (msg == WM_MOUSEWHEEL) {
        POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        ScreenToClient(state.hwnd, &point);
        state.mouseX = static_cast<float>(point.x);
        state.mouseY = static_cast<float>(point.y);
        state.wheel += GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
    }
    if (msg == WM_KILLFOCUS || (msg == WM_CAPTURECHANGED && state.mouseDown)) {
        state.mouseDown = false;
        state.reset_input();
        state.activeId.clear();
        state.openPopup.clear();
        state.focusedId.clear();
    }
    if (msg == WM_KEYDOWN) {
        if (wp == VK_TAB && !(lp & (1LL << 30)))
            state.tabPressed = true;
        if ((wp == VK_SPACE || wp == VK_RETURN) && !(lp & (1LL << 30)))
            state.activatePressed = true;
        if (wp == VK_LEFT)
            state.arrowX = -1;
        if (wp == VK_RIGHT)
            state.arrowX = 1;
        if (wp == VK_UP)
            state.arrowY = -1;
        if (wp == VK_DOWN)
            state.arrowY = 1;
        if (wp == VK_HOME)
            state.homePressed = true;
        if (wp == VK_END)
            state.endPressed = true;
        if (wp == VK_DELETE)
            state.deletePressed = true;
        if (wp == VK_ESCAPE)
            state.escapePressed = true;
    }
}
}
