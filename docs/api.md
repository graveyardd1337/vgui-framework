# API reference

[Documentation](README.md) · [Public header](https://github.com/insomfaze/vgui-framework/blob/main/include/vgui.hpp)

All methods below belong to `vgui::Context`. Submit layout and widgets between a
successful `begin_frame()` and its matching `end_frame()`.

## Context and frames

| Call | Behavior |
| --- | --- |
| `Context(HWND window)` | Creates a DX11 renderer for an existing window. Not copyable or movable. |
| `Context(HWND, ID3D11Device*, ID3D11DeviceContext*)` | Uses a supplied device and immediate context; holds COM references. No owned swap chain. |
| `message(UINT, WPARAM, LPARAM)` | Processes input from WndProc. Does not return whether a message was consumed. |
| `bool window_message(UINT, WPARAM, LPARAM, LRESULT&)` | Forwards input and handles DPI/borderless native messages. If true, return the supplied result from WndProc. Do not also call `message()` for that message. |
| `bool begin_frame()` | Starts a frame, handles resize and Tab navigation. Returns false for minimized or zero-sized windows. |
| `end_frame()` | Checks scope balance, draws and presents. |
| `bool begin_frame(unsigned width, unsigned height)` | External mode only: physical target dimensions. Returns false for zero size or device loss. |
| `end_frame(ID3D11RenderTargetView*)` | External mode only: draws without clearing or presenting, then restores the host pipeline state. |
| `set_dpi_scale(float)` | Between frames: zero selects window DPI; an explicit scale must be 0.5 through 8. |
| `float dpi_scale() const` | Scale applied by the most recent successful begin_frame. |
| `bool device_lost() const` | True if the renderer is unavailable or the device was removed. |
| `ID3D11Device* device() const` | Borrowed device pointer for creating textures. Reacquire after reset. |
| `reset_device()` | Recreates standalone GPU resources between frames. |
| `reset_device(ID3D11Device*, ID3D11DeviceContext*)` | Reconnects external mode to a matching device and immediate context between frames. |
| `set_borderless(bool)` | Removes or restores the native window frame. Call outside a frame. |
| `set_resizable(bool, float min_width = 180, float min_height = 80)` | Enables borderless edge/corner resizing with logical minimum dimensions. Call outside a frame. |
| `set_vsync(bool)` | VSync is on by default. Disabling it does not enable the DXGI tearing flag. |
| `FrameStats frame_stats() const` | Vertex and draw-call counts for the last completed frame; zero before the first frame. |

All layout dimensions and item rectangles are logical pixels at 96 DPI. Target dimensions are physical pixels. Enable per-monitor DPI awareness before creating windows. Text is UTF-8. External mode ignores VSync; the host owns presentation. See [external rendering](external-rendering.md) for ownership and recovery.

## Windows and panels

```cpp
void begin_window(std::string_view title, Rect& bounds, WindowMode mode = WindowMode::Panel);
void end_window();
void begin_panel(std::string_view title, Rect bounds);
void end_panel();
```

`Rect` contains `float x, y, w, h`. In `WindowMode::Panel`, `begin_window()` creates
a movable root panel, at least 180 by 80 pixels. Dragging updates `bounds`, so keep
that rectangle between frames.

In `WindowMode::Native`, the root fills the HWND's client area and overwrites `bounds`
with its size and origin. Dragging the title bar moves the actual desktop window.
Use one native root per HWND. `set_borderless(true)` removes the OS frame;
`set_borderless(false)` restores the saved native style and rectangle.

Windows cannot be nested. Use `begin_panel()` for a fixed container, at least 40 by
40 pixels. Inside another container, its x/y coordinates are relative to the parent's
top-left corner, including the title bar. Panels clip their contents and can contain
other panels. `end_panel()` restores the parent's layout. Panels are positioned
explicitly and do not consume an automatic layout row. A root panel is also allowed.

Each begin needs a matching end in the same frame. Close child panels before
`end_window()`. Root panels/windows have persistent z-order: clicking an exposed root raises it, and only the topmost root under the pointer receives mouse input. Keyboard focus follows activation. Child panels inherit the root layer and draw in submission order; use separate roots for independently floating surfaces. Hit testing uses the previous completed frame's root bounds.

## Scrollable panels

```cpp
void begin_scroll_panel(std::string_view title, Rect bounds);
void end_scroll_panel();
```

Uses the same coordinates as `begin_panel()`. Rows and nested panels move with the scroll offset and are clipped to the viewport. Content height is measured during submission; a scrollbar appears when needed. A 20-pixel gutter is reserved to avoid width changes. Use a panel height of at least 80 for usable scrollbar controls. The mouse wheel moves by 44 logical pixels per notch and goes to the deepest hovered scroll panel. Lists inside a scroll panel have their own scrollbar controls; panel wheel input scrolls the enclosing panel.

Keep submitting the content each frame; this API does not virtualize large lists. A new content height becomes available after `end_scroll_panel()`, so adding/removing content may settle the offset on the following frame. Explicitly positioned child panels extend the measured content height. Close with `end_scroll_panel()`, not `end_panel()`.

## Basic widgets

```cpp
void text(std::string_view value);
bool button(std::string_view label);
bool checkbox(std::string_view label, bool& value);
bool slider(std::string_view label, float& value, float minimum, float maximum);
```

- **Text:** displays a literal string, without printf formatting. `##` is not hidden in plain text.
- **Button:** returns true on mouse release inside after a press inside, or Space/Enter when focused.
- **CheckBox / CheckButton:** toggles `value` and returns true when toggled.
- **Slider:** requires finite `minimum < maximum`. Clamps the value to the range and replaces a non-finite value with the minimum. Returns true when the value changes, including clamping. Arrow keys move by 1% of the range; dragging sets the position on the track.

## Selection and tabs

```cpp
bool combo_box(std::string_view label, int& selected,
               const std::vector<std::string>& items);
bool tabs(std::string_view id, int& selected,
          const std::vector<std::string>& items);
bool list_box(std::string_view label, int& selected,
              const std::vector<std::string>& items, int visible_rows = 5);
bool multi_box(std::string_view label, std::vector<bool>& selected,
               const std::vector<std::string>& items, int visible_rows = 5);
```

`selected` is a zero-based index. ComboBox, Listbox and Tabs clamp it to the valid
range, or set it to -1 for an empty list. They return true when the index changes.
The item vector is only used during the call.

**ComboBox** opens a menu above the UI, with up to six visible rows and scrolling.
Mouse selection or Space/Enter closes it. Escape, Tab or an outside click also closes
the menu; that outside click is consumed. The open menu blocks mouse interaction
with background widgets.

**Tabs / PropertySheet** draws tabs and updates the selected index. Draw the page
contents yourself, for example with `switch (selected)`. Tab captions are displayed
literally. Repeated captions are allowed because internal IDs include the tab index.

**Listbox** selects one row. **Multibox** toggles rows independently, without Ctrl/Shift.
Its selection vector is resized to match `items`, with new flags set to false. Its
return value reports a toggle, not a vector resize. Arrow keys move the Multibox
cursor; Space/Enter toggles the current row. There is no dotted focus outline.

`visible_rows` is clamped to at least 1. Lists support the mouse wheel, arrow keys,
Home/End and their own scrollbar. Keep item order stable because selection and scroll
positions use indices.

## ProgressBar

```cpp
void progress_bar(float fraction, int segments = 16);
```

Draws a segmented progress bar. The default is 16 blocks; the second argument sets
the requested count. Blocks appear whole, without partial fill or fading. With 16
blocks, each step is 6.25% and all blocks are visible at 100%.

The fraction is clamped to 0..1; non-finite values become zero. Counts below 1 become
1, and narrow bars reduce the count to fit. The widget accepts no input. The caller
updates the progress value.

## TextEntry / TextBox

```cpp
bool text_entry(std::string_view label, std::string& value,
                size_t max_length = 256);
```

A single-line UTF-8 editor. Returns true when the string changes. Click to place the caret, drag to select, or use Shift+Left/Right/Home/End. Ctrl+A selects all, Ctrl+C copies, Ctrl+X cuts and Ctrl+V pastes through the Windows Unicode clipboard. Left/Right, Home/End and Backspace/Delete work on codepoint boundaries. Escape clears focus without reverting edits. Long text scrolls to keep the caret visible.

`max_length` limits new input in bytes; insertion never splits a UTF-8 codepoint and does not trim existing text. Paste replaces newlines and tabs with spaces. Feed WM_CHAR through `TranslateMessage`/`DispatchMessageW`; UTF-16 surrogate pairs are combined before insertion. Malformed UTF-8 renders as replacement characters. Glyph coverage depends on Windows fonts. Grapheme-cluster editing, complex-script shaping, IME, multiline editing and undo are not implemented.

## Tooltip and Image

```cpp
void tooltip(std::string_view text, float delay = 0.4f);
void image(ID3D11ShaderResourceView* texture, float width, float height,
           Rect uv = {0, 0, 1, 1}, Color tint = {255, 255, 255, 255});
```

Call `tooltip()` immediately after its widget. The delay is in seconds; zero shows it immediately while hovered. It accepts UTF-8 and newlines, draws above the UI and is positioned near the pointer within the target. Moving to another item restarts the delay.

`image()` consumes a layout row. Width and height are positive logical dimensions; available row width may clip the requested width. Supply a Texture2D SRV from `device()` (or the external device). `uv` is normalized x/y/width/height, not two corner coordinates; tint multiplies texture RGBA. The sampler clamps at the texture edges and uses point filtering. Arrays, cubes and multisampled SRVs are rejected. Use a float-sampleable color format such as RGBA8 UNORM.

The caller owns the texture and keeps the SRV valid until the `image()` call. The library retains a COM reference through the draw. Never render to a texture while sampling that same subresource. After a device reset, recreate images on the new device; stale textures are rejected.

## ScrollBar

```cpp
bool scroll_bar(std::string_view label, int& position,
                int total, int visible, float height = 140);
```

A vertical scrollbar on the right of its row, with a label above it. `position` is
the first visible row, in `[0, max(0, total - visible)]`. `total` is clamped to at
least 0, `visible` to at least 1, and the bar height to at least 70 pixels.

Returns true when the position changes. Buttons move by one row, the wheel by three,
and a track click by one page. The thumb can be dragged.

The caller uses `position` to choose which content to draw. This control does not
automatically scroll a neighboring Panel.

## Layout

| Method | Behavior |
| --- | --- |
| `set_next_item_width(float width)` | Width of the next widget only. Zero uses the available space; larger values are clipped to it. |
| `same_line(float spacing = -1)` | Places the next widget to the right of the previous one at the same y coordinate. A negative gap uses `style.item_spacing`. |
| `spacing(float pixels = -1)` | Adds vertical space. A negative value uses the default item spacing. |
| `separator()` | Draws a two-pixel horizontal line followed by the normal item spacing. |

Widgets normally occupy a full row. Before using `same_line()`, give the preceding
widget a smaller width; there is no automatic column distribution. Calling
`same_line()` without a preceding widget in the current container is an error.
After a row of widgets, vertical layout continues below the tallest item.
Opening or closing a container clears the pending width and same-line setting.

`Style` contains `padding` (14 pixels by default) and `item_spacing` (9 pixels).
Set finite, nonnegative values before the frame. Padding affects content rows,
not title bars or frame sizes.

## IDs and item state

```cpp
void push_id(std::string_view id);
void push_id(int id);
void pop_id();
ItemState last_item() const;
```

The full label, container IDs and ID stack identify each widget. `##` hides a suffix
only when drawing the caption. `push_id(42)` and `push_id("42")` are equivalent.
Full labels must be unique within a scope.

Balance push/pop in the same frame and container. `pop_id()` cannot remove a scope
opened outside the current container. Keep IDs stable between frames so focus,
caret and scroll positions stay attached to the correct widget.

Call `last_item()` immediately after the widget:

| Field | Meaning |
| --- | --- |
| `bounds` | Last item's rectangle in client coordinates. |
| `hovered` | Mouse is inside the item and clip region, accounting for a blocking menu. |
| `active` | The item or one of its internal controls owns the active interaction. |
| `focused` | The item or one of its internal controls has keyboard focus. |

Text and Separator update bounds/hover state but do not receive focus. This query
returns state recorded during widget submission, not live OS state.

## Theme

`Color` stores RGBA bytes from 0 to 255, with alpha defaulting to 255.
The public `ui.theme` contains:

| Color | Use |
| --- | --- |
| `background`, `panel` | Client/field backgrounds and panel surfaces. |
| `light`, `shadow` | Light and dark bevel edges. |
| `text`, `accent`, `check` | Text, accents and checkmarks. |
| `track` | Slider track. |
| `scroll_track` | ScrollBar track. |
| `hovered`, `selected` | Hovered controls and selected list rows. |

Set the theme before submitting widgets. Colors can change without recreating GPU
resources.
