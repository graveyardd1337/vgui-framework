# Examples

[Documentation](README.md)

These snippets assume an open frame and window unless begin/end calls are shown.
Keep application state between frames.

## Buttons on one row

```cpp
ui.set_next_item_width(100);
if (ui.button("Apply")) save_settings(); // Your application function.
ui.same_line(8);
ui.set_next_item_width(100);
if (ui.button("Reset")) volume = 0.65f;
ui.separator();
ui.text("Next row");
```

## Repeated widgets

```cpp
for (int index = 0; index < 3; ++index) {
    ui.push_id(index);
    if (ui.button("test")) selected = index;
    ui.pop_id();
}
```

If collection order can change, use a stable object ID instead of its index.
For repeated captions outside a loop, a hidden suffix works too:

```cpp
ui.checkbox("test##music", music);
ui.checkbox("test##voice", voice);
```

## Tabs

```cpp
// page, volume and player are application state kept between frames.
ui.tabs("settings", page, {"General", "Audio"});
if (page == 0) ui.text_entry("Player", player);
if (page == 1) ui.slider("Volume", volume, 0.0f, 1.0f);
```

## Panels

```cpp
ui.begin_window("test", window_bounds);
ui.begin_panel("Left", {12, 40, 220, 240});
ui.text("test");
ui.end_panel();
ui.begin_panel("Right", {244, 40, 220, 240});
ui.checkbox("test", enabled);
ui.end_panel();
ui.end_window();
```

Panels are positioned relative to the parent, not the layout cursor. Give the parent
enough room for both panels. Panels do not scroll automatically; use Listbox or
Multibox for scrollable lists.

## Lists and multiple selection

```cpp
// Keep these between frames.
std::vector<std::string> maps{"de_dust2", "de_nuke", "cs_office"};
int map = 0;
std::vector<bool> channels{true, false, true};

// Submit each frame.
ui.combo_box("Map", map, maps);
ui.list_box("Maps", map, maps, 3);
ui.multi_box("Channels", channels, {"Music", "Voice", "Effects"}, 3);
```

## Hover state

```cpp
ui.button("test");
const auto item = ui.last_item();
if (item.hovered) ui.text("hovered");
```

Read or save ItemState before submitting another widget: `text()` updates it too.

## Theme

Set layout metrics before starting a frame:

```cpp
ui.theme.scroll_track = {58, 65, 49};
ui.theme.check = {235, 240, 225};
ui.style.padding = 12;
ui.style.item_spacing = 7;
```

## Progress blocks

```cpp
ui.progress_bar(progress);     // 16 blocks.
ui.progress_bar(progress, 10); // 10 blocks.
```

The caller supplies progress from 0 to 1. Blocks appear whole; with 10 blocks,
0.3 fills three of them.

## Borderless window

```cpp
// After creating the context, outside a frame.
ui.set_borderless(true);
vgui::Rect bounds{};

// Inside the application loop.
if (ui.begin_frame()) {
    ui.begin_window("test", bounds, vgui::WindowMode::Native);
    ui.text("test");
    if (ui.button("Close")) running = false;
    ui.end_window();
    ui.end_frame();
}
```

Here `running` controls your application's loop. Use one native root per HWND.
For separate desktop windows, create an HWND and context for each.

## Complete settings menu

[settings.hpp](https://github.com/graveyardd1337/vgui-framework/blob/main/examples/settings.hpp) holds application state;
[settings.cpp](https://github.com/graveyardd1337/vgui-framework/blob/main/examples/settings.cpp) implements `show_settings(ui, settings)`.
Call it inside a successful frame, without another window open. CTest checks
submission of each settings tab.

[demo/main.cpp](https://github.com/graveyardd1337/vgui-framework/blob/main/demo/main.cpp) is a standalone old-Steam-style demo with a game
list and simulated pre-loading. It also serves as the DLL client example.
