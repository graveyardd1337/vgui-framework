# FAQ and limits

[Documentation](README.md)

### Is this ImGui with a different theme?

No. It uses the same general idea of submitting UI each frame, but input, layout,
widgets and DX11 rendering are implemented here. It does not implement the ImGui
API or provide binary compatibility with Valve VGUI.

### Why do widgets with the same caption conflict?

The label is part of the widget ID. Use `test##first` / `test##second`, or wrap
repeated widgets in `push_id()` / `pop_id()`. Container IDs also contribute to the
ID. Keep labels stable; changing a label every frame changes the widget's identity.

### Why do buttons not fit with same_line?

The first widget uses the full width by default. Call `set_next_item_width(100)`
before it. Set a width for the second widget too if it should not fill the remaining
space. Layout does not wrap to the next row automatically.

### Where is the keyboard focus outline?

Tab and Shift+Tab work, but dotted outlines are disabled. TextEntry shows a caret.
Use `last_item().focused` if you want to draw your own focus indicator.

### Does it support Cyrillic or other Unicode text?

Yes. Text uses UTF-8, glyphs are rasterized on demand through GDI, and TextEntry converts WM_CHAR surrogate pairs to UTF-8. Selection and Ctrl+A/C/V/X are supported. Font coverage determines glyph availability. Complex-script shaping, grapheme-cluster editing, IME and undo are not implemented.

### Can it use an existing game's DirectX 11 renderer?

Yes. Use `Context(hwnd, device, immediate_context)`, `begin_frame(width, height)` and `end_frame(rtv)`. The library restores the host pipeline state and leaves clearing, resizing and Present to the host. See [external rendering](external-rendering.md). The application must integrate those calls into its render loop.

### Does loading the DLL create the interface automatically?

No. The application must create a context, forward input and submit UI each frame.
`-Shared` builds the library as a DLL and the demo as its client. Keep the DLL beside
the executable and use compatible MSVC/STL, runtime and architecture settings.

### How do I remove the background host window?

Create an HWND sized for your UI, call `set_borderless(true)` outside a frame, and
use `WindowMode::Native` in `begin_window()`. The VGUI frame then fills the HWND and
its title bar moves the desktop window. The demo does this by default; `--framed`
keeps the normal Windows border. Forward messages through `window_message()` for edge/corner resizing. `set_resizable(false)` disables it. Custom minimize/maximize buttons are not provided.

### Can I use multiple windows or threads?

Yes, with one Context per HWND. Use each context on its window's UI thread; several
windows can share that thread. A single Context is not thread-safe. Inside one HWND, root panels/windows maintain z-order and only the frontmost root under the pointer receives mouse input. Child containers inherit their root's order; independently floating surfaces should be separate roots.

### Why does ScrollBar not scroll the whole document?

ScrollBar changes a numeric `position`. The application chooses which document rows
to draw. For automatic content scrolling use `begin_scroll_panel()` / `end_scroll_panel()`. The panel measures its submitted content, clips it and adds a scrollbar when needed. Listbox and Multibox also scroll their own contents.

### How does DPI scaling work?

Enable per-monitor DPI awareness before creating windows and forward messages through `window_message()`. Layout coordinates are logical pixels at 96 DPI. Fonts, geometry and input follow the window's DPI. Call `set_dpi_scale(1.5f)` for an override or zero for automatic mode, between frames. Explicit external target dimensions are always physical pixels.

### What happens after device removal?

Standalone mode attempts to rebuild GPU resources on the next frame. External mode returns false from `begin_frame()` until the host reconnects a replacement device through `reset_device(device, context)`. Recreate application-owned image textures on the replacement device too. Explicit reset/reconnection is tested; an actual driver crash is not forced by the tests.

### How do I change the number of progress blocks?

Use `progress_bar(value, count)`, with `value` between 0 and 1. The default count is
16. Blocks appear whole, one step at a time. A narrow bar may show fewer blocks so
they fit within its width.

### Where are settings saved?

Widget values live in the application. The library does not write configuration
files or save window positions between runs. Save your own state as needed.

### What else is missing?

There is no docking, general-purpose list virtualization or accessibility integration. Text and list APIs are intended
for modest data sets. Caret and scroll metadata stay until the Context is destroyed,
so avoid generating new IDs every frame. Planned widgets are in [TODO.md](https://github.com/insomfaze/vgui-framework/blob/main/TODO.md).

### How do I check changes?

Run `./build.ps1`, or `./build.ps1 -Shared` for the DLL configuration. For an existing
Ninja build, run `ctest --test-dir build/ninja --output-on-failure`.
If an executable or DLL is locked, close the demo before rebuilding.
