# vgui-framework documentation

C++17 immediate-mode UI with a DirectX 11 renderer and a theme inspired by CS 1.6.
The library has no ImGui dependency and uses no Valve source code.

## Guides

1. [Getting started](getting-started.md): builds, DLL linking, Win32 and the frame loop.
2. [API reference](api.md): widgets, parameters and return values.
3. [Examples](examples.md): tabs, IDs, layout and themes.
4. [Architecture](architecture.md): rendering, input and widget implementation.
5. [FAQ](faq.md): common mistakes and current limits.

## Quick example

Keep widget values in your application and submit the UI each frame:

```cpp
// Keep these between frames.
vgui::Rect window{20, 20, 400, 260};
bool sound = true;
float volume = 0.65f;

// Inside the application loop.
if (ui.begin_frame()) {
    ui.begin_window("test", window);
    ui.checkbox("Sound", sound);
    ui.slider("Volume", volume, 0.0f, 1.0f);
    if (ui.button("Reset")) volume = 0.65f;
    ui.end_window();
    ui.end_frame();
}
```

[demo/main.cpp](../demo/main.cpp) is a complete desktop application.
[examples/settings.cpp](../examples/settings.cpp) shows a small settings menu
that is also compiled and exercised by the tests.

## Basic rules

- Use one `Context` per HWND, on the window's UI thread.
- Forward WndProc input to `message()` before submitting the next frame.
- If `begin_frame()` returns false, skip UI submission and `end_frame()`.
- Balance all begin/end and push/pop calls.
- Widget data belongs to the caller. References to that data are not kept between frames.
- Keep labels stable. `test##audio` displays `test` but uses the full label as its ID.
- Coordinates are client pixels. DPI scaling is not automatic.
- Buttons return true on activation; value controls return true when their value changes.

The API is still evolving. See [vgui.hpp](../include/vgui.hpp) for public declarations;
headers in `src/` are private. Planned widgets are listed in [TODO.md](../TODO.md).
