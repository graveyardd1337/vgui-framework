#pragma once
#include <Windows.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#if defined(VGUI_SHARED)
#if defined(VGUI_BUILDING_LIBRARY)
#define VGUI_API __declspec(dllexport)
#else
#define VGUI_API __declspec(dllimport)
#endif
#else
#define VGUI_API
#endif

namespace vgui {
// Pixels. Child panel positions are relative to the parent.
struct Rect {
    float x, y, w, h;
};
// RGBA, 0..255.
struct Color {
    unsigned char r, g, b, a = 255;
};
struct Theme {
    Color background{62, 70, 54}, panel{76, 88, 68}, light{136, 145, 123};
    Color shadow{35, 40, 30}, text{216, 222, 197}, accent{196, 181, 80};
    Color track{30, 35, 26}, check{225, 230, 215};
    Color scroll_track{54, 61, 46};
    Color hovered{87, 99, 77}, selected{101, 111, 80};
};

// Change these before starting a frame.
struct Style {
    float padding = 14.0f;
    float item_spacing = 9.0f;
};

// Info about the last widget. Coordinates are relative to the client area.
struct ItemState {
    Rect bounds{};
    bool hovered = false;
    bool active = false;
    bool focused = false;
};

struct FrameStats {
    size_t vertices = 0;
    size_t draw_calls = 0;
};

enum class WindowMode { Panel, Native };

// One Context per HWND. Feed it messages from WndProc.
// "test##volume" shows "test", but the whole string is used as the ID.
// ASCII only for now.
class Context {
public:
    VGUI_API explicit Context(HWND window);
    VGUI_API ~Context();
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    VGUI_API void message(UINT message, WPARAM wparam, LPARAM lparam);
    // Minimized? Returns false; skip UI and end_frame in that case.
    [[nodiscard]] VGUI_API bool begin_frame();
    VGUI_API void end_frame();
    // Dragging changes bounds, so keep it around between frames.
    VGUI_API void begin_window(std::string_view title, Rect& bounds,
                               WindowMode mode = WindowMode::Panel);
    // Native mode fills the HWND; borderless removes the OS frame.
    VGUI_API void set_borderless(bool enabled);
    VGUI_API void end_window();
    // Panel positions are relative to the parent, including its title bar.
    VGUI_API void begin_panel(std::string_view title, Rect bounds);
    VGUI_API void end_panel();
    VGUI_API void text(std::string_view value);
    VGUI_API void progress_bar(float fraction, int segments = 16);
    VGUI_API bool button(std::string_view label);
    VGUI_API bool checkbox(std::string_view label, bool& value);
    // Widgets with a value return true when it changes. Slider needs min < max.
    VGUI_API bool slider(std::string_view label, float& value, float minimum, float maximum);
    VGUI_API bool combo_box(std::string_view label, int& selected,
                            const std::vector<std::string>& items);
    VGUI_API bool tabs(std::string_view id, int& selected, const std::vector<std::string>& items);
    // max_length limits typing, it does not trim an existing string.
    VGUI_API bool text_entry(std::string_view label, std::string& value, size_t max_length = 256);
    VGUI_API bool scroll_bar(std::string_view label, int& position, int total, int visible,
                             float height = 140);
    VGUI_API bool list_box(std::string_view label, int& selected,
                           const std::vector<std::string>& items, int visible_rows = 5);
    VGUI_API bool multi_box(std::string_view label, std::vector<bool>& selected,
                            const std::vector<std::string>& items, int visible_rows = 5);

    // Width is only for the next item. 0 = use the space left.
    VGUI_API void set_next_item_width(float width);
    VGUI_API void same_line(float spacing = -1.0f);
    VGUI_API void spacing(float pixels = -1.0f);
    VGUI_API void separator();

    // Use these for repeated labels, e.g. buttons in a loop.
    VGUI_API void push_id(std::string_view id);
    VGUI_API void push_id(int id);
    VGUI_API void pop_id();

    // Read this right after the widget you care about.
    [[nodiscard]] VGUI_API ItemState last_item() const;
    // Last completed frame.
    [[nodiscard]] VGUI_API FrameStats frame_stats() const;
    // VSync is on by default.
    VGUI_API void set_vsync(bool enabled);

    Theme theme;
    Style style;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
