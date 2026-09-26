# Architecture

[Documentation](README.md)

## Files

| File | Purpose |
| --- | --- |
| `include/vgui.hpp` | Public types, Context API and DLL import/export declarations. |
| `src/context.cpp` | Frame lifecycle, native window styling, Tab navigation and statistics. |
| `src/input.cpp` | Win32 messages, mouse capture, character input and per-frame input reset. |
| `src/layout.cpp` | Windows, panels, clipping, rows, IDs and hit testing. |
| `src/widgets.cpp` | Widget rendering and interaction, lists and combo menus. |
| `src/render.hpp` | Private DX11 Renderer declaration. |
| `src/render.cpp` | Device/swap chain, shaders, font atlas, geometry and Present. |
| `src/internal.hpp` | Private interaction and layout state in Context::Impl. |
| `demo/main.cpp` | Desktop demo with a pre-load dialog and game list. |
| `examples/settings.*` | Settings menu using the public API. |
| `tests/interaction.cpp` | Mouse and keyboard interaction checks. |
| `tests/api.cpp` | Layout, IDs, item state, scope balance, native windows and progress checks. |

## Frame flow

```text
Win32 WndProc -> Context::message -> accumulated input
begin_frame  -> resize + clear draw buffers + move focus
widgets      -> layout + ID + interaction -> renderer geometry
end_frame    -> append overlay -> upload -> Draw -> Present
```

The application owns checkbox, slider, text and list values. The context keeps
interaction state: focus and active IDs, mouse position, carets, scroll positions
and layout. A widget that is not submitted is not drawn that frame.

`Context::Impl` owns a `detail::Renderer`. Widgets call its `quad`, `label` and
`bevel` methods rather than D3D11 directly. The public header exposes Win32 types
for integration but does not include D3D11 headers.

Each context owns a device and swap chain. Native mode fills its HWND; panel mode
places movable VGUI frames within that HWND. The demo uses two contexts and HWNDs
for independent desktop windows.

## Rendering

A Tahoma atlas is created once through GDI. An opaque white texel in the same atlas
is used for rectangles, so text and geometry share a texture and shader.

Clipping happens when building quads: positions and UVs are clipped together.
Checkmarks and arrows use pixel geometry rather than font glyphs. Combo menus put
their vertices in a separate overlay buffer that is appended after the normal UI.
A non-empty frame uses one Draw call. An empty frame only clears and presents.

## Adding a widget

1. Declare the public method in `vgui.hpp`, including `VGUI_API`, and document its values and return behavior.
2. In `widgets.cpp`, get a row with `state.row(height)` and an ID with `state.id(label)`.
3. Use `state.interact` for button-like behavior. Give internal controls their own ID suffixes and call `record_item` for the complete widget.
4. Draw through `state.render`. Save and restore the clip rectangle when changing it locally.
5. Add an example and check the relevant interaction behavior.

## Static and DLL builds

CMake builds `vgui-framework` as a static library by default. `VGUI_BUILD_SHARED=ON`
selects a DLL, exports the public methods and passes `VGUI_SHARED` to consumers.
The demo uses the same API in either configuration.

The DLL is a C++ library with standard-library types in its interface. Clients need
compatible compiler, STL and runtime settings. It does not provide a stable C ABI
or an automatic integration layer for an existing renderer.

## Style and checks

`.clang-format` uses four-space indentation, attached braces and a 100-column limit.
With clang-format installed, format the source files from PowerShell:

```powershell
Get-ChildItem include, src, demo, tests, examples -Recurse -File |
    Where-Object { $_.Extension -in '.hpp', '.cpp' } |
    ForEach-Object { clang-format -i $_.FullName }
```

`build.ps1` runs four CTest checks: widget interactions, public API behavior, and
three-frame demo startup/render checks with and without Windows borders. Tests
use real DX11 contexts with hidden windows. Both static and DLL builds run the
same checks.
