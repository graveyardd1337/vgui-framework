# TODO

Completed items are available in the source tree; the original v0.1.0 demo predates them. Unchecked items are planned.

## Main priorities

- [x] External D3D11 rendering: accept a caller-owned device and context, draw into the caller's target, and restore any GPU state changed by the library. Leave clearing, swap-chain resizing and Present to the host application.
- [x] UTF-8 text: support Cyrillic and other glyphs, decode text correctly, and keep caret movement and deletion on character boundaries.
- [x] DPI scaling: scale fonts, widgets, spacing and hit testing together, including DPI changes when moving between monitors.

## Usability

- [x] TextEntry selection and clipboard: mouse/keyboard selection, Ctrl+A, Ctrl+C and Ctrl+V.
- [x] Scrollable panels with mouse-wheel input, clipping and an automatic scrollbar.
- [x] Tooltip on hover.
- [x] Image widget for caller-provided D3D11 textures, with documented texture lifetime and UV coordinates.
- [x] Borderless window resizing from edges and corners.

## Reliability

- [x] Device-lost recovery: rebuild renderer resources after device removal/reset. In external-device mode, let the host replace the device and reconnect the library.
- [x] Panel z-order within one HWND: bring panels to front on activation, route input to the topmost panel and keep drawing order consistent with focus.

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
