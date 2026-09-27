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
| `src/text.cpp`, `src/text.hpp` | UTF-8 decoding, character boundaries and UTF-16 conversion. |
| `src/text_entry.cpp` | Single-line editing, selection, clipboard and caret scrolling. |
| `src/extras.cpp` | Images and hover tooltips. |
| `src/render.hpp` | Private DX11 Renderer declaration. |
| `src/render.cpp` | Device/swap chain, shaders, font atlas, geometry and Present. |
| `src/internal.hpp` | Private interaction and layout state in Context::Impl. |
| `demo/main.cpp` | Desktop demo with a pre-load dialog and game list. |
| `demo/features.cpp` | Interactive feature gallery. |
| `examples/settings.*` | Settings menu using the public API. |
| `tests/interaction.cpp` | Mouse and keyboard interaction checks. |
| `tests/api.cpp` | Layout, IDs, item state, scope balance, native windows and progress checks. |
| `tests/features.cpp` | External rendering, Unicode, clipboard, DPI, resize, reset, z-order and scrolling. |

## Frame flow

```text
Win32 WndProc -> Context::window_message -> native handling + accumulated input
begin_frame  -> device check + DPI + resize + clear draw buffers + focus
widgets      -> layout + ID + interaction -> renderer geometry
end_frame    -> layer sort -> state swap -> upload -> texture batches -> restore state
standalone   -> Present (external mode leaves this to the host)
```

The application owns checkbox, slider, text and list values. The context keeps
interaction state: focus and active IDs, mouse position, carets, scroll positions
and layout. A widget that is not submitted is not drawn that frame.

`Context::Impl` owns a `detail::Renderer`. Widgets call its `quad`, `label` and
`bevel` methods rather than D3D11 directly. The public header exposes Win32 types
for integration but does not include D3D11 headers.

Standalone contexts own a device and swap chain. External contexts retain a supplied device and immediate context and draw into a supplied RTV. Native mode fills its HWND; panel mode places movable VGUI frames within that HWND. Root draw order and pointer routing follow persistent z-order. Activating an exposed root raises it. Child panels inherit the root layer and use submission order within that layer.

## Rendering

Tahoma glyphs are rasterized on demand through GDI into 2048-square atlas pages. UTF-8 codepoints map to glyph records; an opaque white texel draws solid rectangles. DPI changes rebuild the atlas at the requested scale. Widget coordinates and input stay in logical pixels, while the D3D viewport uses physical target dimensions.

Clipping happens when building quads: positions and UVs are clipped together.
Checkmarks and arrows use pixel geometry rather than font glyphs. Combo menus put
their vertices in a separate overlay buffer that is appended after the normal UI.
Vertices are stably sorted by layer and batched by adjacent texture use. Images and additional font pages can introduce more Draw calls. Tooltips use a layer above combo menus. External drawing swaps the complete D3D11.1 context state and restores it afterward; saved UI state releases host target and image bindings. Standalone mode also clears and presents its own target.

Device removal is checked at frame boundaries and on GPU failures. Standalone mode attempts a full resource rebuild; external mode requires the host to reconnect replacement objects. CPU widget values are independent of GPU resources. Application image textures must be recreated too.

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
or automatic hooks. External renderer integration is explicit through the public constructor and frame calls.

## Style and checks

`.clang-format` uses four-space indentation, attached braces and a 100-column limit.
With clang-format installed, format the source files from PowerShell:

```powershell
Get-ChildItem include, src, demo, tests, examples -Recurse -File |
    Where-Object { $_.Extension -in '.hpp', '.cpp' } |
    ForEach-Object { clang-format -i $_.FullName }
```

`build.ps1` runs six CTest checks: widget interactions, public API behavior, feature regressions, and three startup/render smoke tests. Feature tests use WARP targets for pixel readback, verify selected host-state bindings and backbuffer release, and exercise resource replacement. Both static and DLL builds run the same checks. No test forces a driver timeout or physical display change.
