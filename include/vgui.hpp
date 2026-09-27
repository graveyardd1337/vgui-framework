#pragma once
#include <Windows.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
struct ID3D11ShaderResourceView;

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
// Logical pixels at 96 DPI. Child positions are relative to the parent.
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
// Text uses UTF-8.
class Context {
public:
    VGUI_API explicit Context(HWND window);
    // External mode never clears, resizes or presents the host's swap chain.
    VGUI_API Context(HWND window, ID3D11Device* device, ID3D11DeviceContext* context);
    VGUI_API ~Context();
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    VGUI_API void message(UINT message, WPARAM wparam, LPARAM lparam);
    // Return result from WndProc when this returns true. Also forwards input.
    VGUI_API bool window_message(UINT message, WPARAM wparam, LPARAM lparam, LRESULT& result);
    // Minimized? Returns false; skip UI and end_frame in that case.
    [[nodiscard]] VGUI_API bool begin_frame();
    // External mode: target dimensions in physical pixels.
    [[nodiscard]] VGUI_API bool begin_frame(unsigned width, unsigned height);
    VGUI_API void end_frame();
    VGUI_API void end_frame(ID3D11RenderTargetView* target);
    // Call outside a frame. Zero selects automatic window DPI.
    VGUI_API void set_dpi_scale(float scale);
    [[nodiscard]] VGUI_API float dpi_scale() const;
    [[nodiscard]] VGUI_API bool device_lost() const;
    // Borrowed pointer for creating image textures. Reacquire after device reset.
    [[nodiscard]] VGUI_API ID3D11Device* device() const;
    // Reconnect an external renderer after the host replaces its device.
    VGUI_API void reset_device(ID3D11Device* device, ID3D11DeviceContext* context);
    // Standalone mode: recreate device resources; widget values stay with the caller.
    VGUI_API void reset_device();
    // Dragging changes bounds, so keep it around between frames.
    VGUI_API void begin_window(std::string_view title, Rect& bounds,
                               WindowMode mode = WindowMode::Panel);
    // Native mode fills the HWND; borderless removes the OS frame.
    VGUI_API void set_borderless(bool enabled);
    VGUI_API void set_resizable(bool enabled, float min_width = 180, float min_height = 80);
    VGUI_API void end_window();
    // Panel positions are relative to the parent, including its title bar.
    VGUI_API void begin_panel(std::string_view title, Rect bounds);
    VGUI_API void end_panel();
    VGUI_API void begin_scroll_panel(std::string_view title, Rect bounds);
    VGUI_API void end_scroll_panel();
    VGUI_API void text(std::string_view value);
    VGUI_API void progress_bar(float fraction, int segments = 16);
    // Call after a widget. Delay is in seconds; zero shows immediately.
    VGUI_API void tooltip(std::string_view text, float delay = 0.4f);
    // SRV must belong to this renderer's device. Retained until end_frame.
    VGUI_API void image(ID3D11ShaderResourceView* texture, float width, float height,
                        Rect uv = {0, 0, 1, 1}, Color tint = {255, 255, 255, 255});
    VGUI_API bool button(std::string_view label);
    VGUI_API bool checkbox(std::string_view label, bool& value);
    // Widgets with a value return true when it changes. Slider needs min < max.
    VGUI_API bool slider(std::string_view label, float& value, float minimum, float maximum);
    VGUI_API bool combo_box(std::string_view label, int& selected,
                            const std::vector<std::string>& items);
    VGUI_API bool tabs(std::string_view id, int& selected, const std::vector<std::string>& items);
    // max_length is a byte limit for new UTF-8 input; existing text is not trimmed.
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
