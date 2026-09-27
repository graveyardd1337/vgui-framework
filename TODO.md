# TODO

Planned work. Unchecked items are not implemented yet.

## Main priorities

- [ ] External D3D11 rendering: accept a caller-owned device and context, draw into the caller's target, and restore any GPU state changed by the library. Leave clearing, swap-chain resizing and Present to the host application.
- [ ] UTF-8 text: support Cyrillic and other glyphs, decode text correctly, and keep caret movement and deletion on character boundaries.
- [ ] DPI scaling: scale fonts, widgets, spacing and hit testing together, including DPI changes when moving between monitors.

## Usability

- [ ] TextEntry selection and clipboard: mouse/keyboard selection, Ctrl+A, Ctrl+C and Ctrl+V.
- [ ] Scrollable panels with mouse-wheel input, clipping and an automatic scrollbar.
- [ ] Tooltip on hover.
- [ ] Image widget for caller-provided D3D11 textures, with documented texture lifetime and UV coordinates.
- [ ] Borderless window resizing from edges and corners.

## Reliability

- [ ] Device-lost recovery: rebuild renderer resources after device removal/reset. In external-device mode, let the host replace the device and reconnect the library.
- [ ] Panel z-order within one HWND: bring panels to front on activation, route input to the topmost panel and keep drawing order consistent with focus.

## More widgets

- [ ] ColorPicker
- [ ] InputInt / InputFloat
- [ ] DragFloat / DragInt
- [ ] Vertical and multi-component sliders
- [ ] Multiline TextEntry
- [ ] TreeNode / CollapsingHeader
- [ ] Popup
- [ ] Context menu
- [ ] MenuBar
- [ ] Tables
