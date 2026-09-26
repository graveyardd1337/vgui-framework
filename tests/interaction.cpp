#include "vgui.hpp"
#include <iostream>
#include <stdexcept>

static void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
int main() {
    HWND window = CreateWindowExW(0, L"STATIC", L"VGUI integration test", WS_OVERLAPPEDWINDOW, 0, 0,
                                  700, 600, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window)
        return 1;
    try {
        vgui::Context ui(window);
        vgui::Rect bounds{0, 0, 400, 500};
        int mode = 0, clicks = 0, selected = 0, position = 0;
        bool checked = false;
        float amount = 0;
        std::string input = "abc";
        std::vector<bool> multiple;
        std::vector<std::string> items{"One",  "Two", "Three", "Four",
                                       "Five", "Six", "Seven", "Eight"};
        auto draw = [&] {
            require(ui.begin_frame(), "begin_frame");
            ui.begin_window("Test", bounds);
            switch (mode) {
            case 0:
                if (ui.button("Button"))
                    ++clicks;
                break;
            case 1:
                ui.checkbox("Check", checked);
                break;
            case 2:
                ui.slider("Slider", amount, 0, 1);
                break;
            case 3:
                ui.text_entry("Text", input);
                break;
            case 4:
                ui.combo_box("Combo", selected, items);
                break;
            case 5:
                ui.list_box("List", selected, items, 3);
                break;
            case 6:
                ui.multi_box("Multi", multiple, items, 3);
                break;
            case 7:
                ui.tabs("Tabs", selected, {"A", "B", "C"});
                break;
            case 8:
                ui.scroll_bar("Scroll", position, 100, 10, 140);
                break;
            case 9:
                ui.begin_panel("Nested", {10, 40, 300, 200});
                if (ui.button("Child"))
                    ++clicks;
                ui.end_panel();
                break;
            }
            ui.end_window();
            ui.end_frame();
        };
        auto point = [](int x, int y) { return MAKELPARAM(x, y); };
        auto click = [&](int x, int y) {
            ui.message(WM_LBUTTONDOWN, MK_LBUTTON, point(x, y));
            draw();
            ui.message(WM_LBUTTONUP, 0, point(x, y));
            draw();
        };
        auto key = [&](WPARAM k) {
            ui.message(WM_KEYDOWN, k, 0);
            draw();
        };
        draw();
        click(40, 50);
        require(clicks == 1, "button click");
        ui.message(WM_LBUTTONDOWN, MK_LBUTTON, point(40, 50));
        draw();
        ui.message(WM_LBUTTONUP, 0, point(500, 50));
        draw();
        require(clicks == 1, "button release outside");
        mode = 1;
        draw();
        click(40, 50);
        require(checked, "checkbox");
        key(VK_SPACE);
        require(!checked, "checkbox keyboard");
        mode = 2;
        draw();
        click(380, 70);
        require(amount > 0.95f, "slider pointer");
        key(VK_LEFT);
        require(amount < 1, "slider keyboard");
        mode = 3;
        draw();
        click(45, 72);
        key(VK_END);
        ui.message(WM_CHAR, 'X', 0);
        draw();
        require(input == "abcX", "text insertion");
        key(VK_HOME);
        key(VK_DELETE);
        require(input == "bcX", "text delete");
        key(VK_END);
        ui.message(WM_CHAR, 8, 0);
        draw();
        require(input == "bc", "text backspace");
        mode = 4;
        draw();
        click(50, 72);
        click(50, 142);
        require(selected == 2, "combo selection");
        click(50, 72);
        key(VK_ESCAPE);
        click(50, 72);
        key(VK_DOWN);
        key(VK_RETURN);
        require(selected == 3, "combo keyboard");
        mode = 5;
        selected = 0;
        draw();
        click(50, 94);
        require(selected == 1, "list selection");
        key(VK_END);
        require(selected == 7, "list keyboard scroll");
        mode = 6;
        draw();
        click(50, 72);
        click(50, 94);
        require(multiple[0] && multiple[1], "multi selection");
        click(50, 72);
        require(!multiple[0] && multiple[1], "multi toggle");
        mode = 7;
        selected = 0;
        draw();
        click(170, 50);
        require(selected == 1, "tabs");
        key(VK_RIGHT);
        require(selected == 2, "tab keyboard");
        mode = 8;
        draw();
        click(377, 190);
        require(position == 1, "scrollbar down");
        ui.message(WM_LBUTTONDOWN, MK_LBUTTON, point(377, 86));
        draw();
        ui.message(WM_MOUSEMOVE, MK_LBUTTON, point(377, 170));
        draw();
        ui.message(WM_LBUTTONUP, 0, point(377, 170));
        draw();
        require(position > 70, "scrollbar drag");
        mode = 9;
        draw();
        click(45, 85);
        require(clicks == 2, "nested panel hit testing");
        ui.message(WM_LBUTTONDOWN, MK_LBUTTON, point(50, 12));
        draw();
        ui.message(WM_MOUSEMOVE, MK_LBUTTON, point(100, 52));
        draw();
        ui.message(WM_LBUTTONUP, 0, point(100, 52));
        draw();
        require(bounds.x == 50 && bounds.y == 40, "frame drag");
        SetWindowPos(window, nullptr, 0, 0, 800, 650, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        draw();
        std::cout << "All widget interaction checks passed.\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        DestroyWindow(window);
        return 1;
    }
    DestroyWindow(window);
    return 0;
}
