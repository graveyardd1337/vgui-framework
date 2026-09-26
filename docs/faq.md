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

Not yet. The atlas contains printable ASCII; other bytes render as `?`. TextEntry
accepts ASCII through WM_CHAR. Unicode, IME, text selection, clipboard and undo
are not implemented.

### Can it use an existing game's DirectX 11 renderer?

The current Context owns its device, device context and swap chain, clears its
render target and presents. It is designed for standalone windows. An existing
renderer would need an integration layer that accepts external GPU objects and
preserves the caller's rendering state.

### Does loading the DLL create the interface automatically?

No. The application must create a context, forward input and submit UI each frame.
`-Shared` builds the library as a DLL and the demo as its client. Keep the DLL beside
the executable and use compatible MSVC/STL, runtime and architecture settings.

### How do I remove the background host window?

Create an HWND sized for your UI, call `set_borderless(true)` outside a frame, and
use `WindowMode::Native` in `begin_window()`. The VGUI frame then fills the HWND and
its title bar moves the desktop window. The demo does this by default; `--framed`
keeps the normal Windows border. Custom minimize/maximize buttons and borderless
resize handles are not implemented.

### Can I use multiple windows or threads?

Yes, with one Context per HWND. Use each context on its window's UI thread; several
windows can share that thread. A single Context is not thread-safe. Inside one HWND,
avoid overlapping root panels because window z-order/focus management is incomplete.

### Why does ScrollBar not scroll the whole document?

ScrollBar changes a numeric `position`. The application chooses which document rows
to draw. Listbox and Multibox scroll their own contents; arbitrary Panels do not.

### How do I change the number of progress blocks?

Use `progress_bar(value, count)`, with `value` between 0 and 1. The default count is
16. Blocks appear whole, one step at a time. A narrow bar may show fewer blocks so
they fit within its width.

### Where are settings saved?

Widget values live in the application. The library does not write configuration
files or save window positions between runs. Save your own state as needed.

### What else is missing?

There is no automatic DPI scaling, docking, general-purpose list virtualization,
device-lost recovery or accessibility integration. Text and list APIs are intended
for modest data sets. Caret and scroll metadata stay until the Context is destroyed,
so avoid generating new IDs every frame. Planned widgets are in [TODO.md](https://github.com/graveyardd1337/vgui-framework/blob/main/TODO.md).

### How do I check changes?

Run `./build.ps1`, or `./build.ps1 -Shared` for the DLL configuration. For an existing
Ninja build, run `ctest --test-dir build/ninja --output-on-failure`.
If an executable or DLL is locked, close the demo before rebuilding.
