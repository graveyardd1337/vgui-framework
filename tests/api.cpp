#include "../examples/settings.hpp"
#include "vgui.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* description) {
    if (!condition)
        throw std::runtime_error(description);
}
template <class Function> void requires_logic_error(Function action) {
    bool caught = false;
    try {
        action();
    } catch (const std::logic_error&) {
        caught = true;
    }
    require(caught, "Expected a lifecycle or scope error");
}
}

int main() {
    HWND window = CreateWindowExW(0, L"STATIC", L"API checks", WS_OVERLAPPEDWINDOW, 0, 0, 700, 600,
                                  nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window)
        return 1;
    try {
        vgui::Context ui(window);
        ui.set_vsync(false);
        vgui::Rect bounds{0, 0, 400, 450};
        bool first = false, second = false;
        vgui::ItemState firstItem, secondItem, nextRow;
        auto draw = [&] {
            require(ui.begin_frame(), "begin frame");
            ui.begin_window("test", bounds);
            ui.push_id(1);
            ui.set_next_item_width(100);
            ui.checkbox("same", first);
            firstItem = ui.last_item();
            ui.pop_id();
            ui.same_line(8);
            ui.push_id(2);
            ui.set_next_item_width(110);
            ui.checkbox("same", second);
            secondItem = ui.last_item();
            ui.pop_id();
            ui.text("next row");
            nextRow = ui.last_item();
            ui.begin_panel("child", {10, 140, 250, 180});
            ui.text("child");
            ui.end_panel();
            ui.same_line();
            ui.set_next_item_width(40);
            ui.button("restored row");
            require(ui.last_item().bounds.y == nextRow.bounds.y, "child restores parent layout");
            ui.end_window();
            ui.end_frame();
        };
        requires_logic_error([&] { ui.end_frame(); });
        requires_logic_error([&] { ui.button("outside frame"); });
        draw();
        require(firstItem.bounds.w == 100 && secondItem.bounds.w == 110, "explicit widths");
        require(secondItem.bounds.x == firstItem.bounds.x + 108 &&
                    secondItem.bounds.y == firstItem.bounds.y,
                "same line");
        require(nextRow.bounds.y > firstItem.bounds.y && nextRow.bounds.w == 372,
                "next row and one-shot width");
        int x = static_cast<int>(secondItem.bounds.x + 8),
            y = static_cast<int>(secondItem.bounds.y + 8);
        ui.message(WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
        draw();
        require(secondItem.active && secondItem.hovered && secondItem.focused,
                "last_item pressed state");
        ui.message(WM_LBUTTONUP, 0, MAKELPARAM(x, y));
        draw();
        require(!first && second, "ID scopes isolate repeated labels");
        require(ui.frame_stats().draw_calls == 1 && ui.frame_stats().vertices > 0,
                "frame statistics");

        require(ui.begin_frame(), "scope checks begin frame");
        requires_logic_error([&] { (void)ui.begin_frame(); });
        requires_logic_error([&] { ui.pop_id(); });
        ui.begin_window("test", bounds);
        requires_logic_error([&] { ui.same_line(); });
        ui.push_id("scope");
        requires_logic_error([&] { ui.end_window(); });
        ui.pop_id();
        ui.end_window();
        ui.end_frame();

        const auto oldStyle = GetWindowLongPtrW(window, GWL_STYLE);
        ui.set_borderless(true);
        require((GetWindowLongPtrW(window, GWL_STYLE) & WS_CAPTION) == 0, "borderless style");
        require(ui.begin_frame(), "native frame");
        ui.begin_window("native", bounds, vgui::WindowMode::Native);
        RECT client{};
        GetClientRect(window, &client);
        require(bounds.x == 0 && bounds.y == 0 && bounds.w == client.right &&
                    bounds.h == client.bottom,
                "native frame fills host");
        ui.end_window();
        ui.end_frame();
        ui.set_borderless(false);
        require(GetWindowLongPtrW(window, GWL_STYLE) == oldStyle, "restore host style");

        auto progressVertices = [&](float fraction) {
            require(ui.begin_frame(), "progress frame");
            ui.begin_window("progress", bounds);
            ui.progress_bar(fraction, 16);
            ui.end_window();
            ui.end_frame();
            return ui.frame_stats().vertices;
        };
        const auto emptyProgress = progressVertices(0.f);
        require(progressVertices(0.01f) == emptyProgress, "no partial segment");
        const auto firstBlock = progressVertices(0.0625f);
        require(firstBlock > emptyProgress, "first full segment");
        require(progressVertices(0.1f) == firstBlock, "segment stays unchanged between steps");
        require(progressVertices(1.f) > firstBlock, "completed progress");

        examples::Settings settings;
        for (int page = 0; page < 3; ++page) {
            settings.page = page;
            require(ui.begin_frame(), "example begin frame");
            examples::show_settings(ui, settings);
            ui.end_frame();
        }
        require(ui.begin_frame(), "empty frame");
        ui.end_frame();
        require(ui.frame_stats().draw_calls == 0, "empty frame has no draw call");
        std::cout << "API layout, scopes, lifecycle and compiled example passed.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        DestroyWindow(window);
        return 1;
    }
    DestroyWindow(window);
    return 0;
}
