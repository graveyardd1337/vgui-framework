# External D3D11 rendering

[Documentation](README.md)

Use this mode when your application already owns a D3D11 device and render loop. The library requires feature level 11.0 or higher, a matching immediate context, and D3D11.1 interfaces for pipeline-state swapping. Deferred contexts are not supported.

## Frame loop

```cpp
// Keep this context and the UI values between frames.
vgui::Context ui(hwnd, device, immediate_context);
vgui::Rect settings{20, 20, 300, 200};
float volume = 0.5f;

// Each frame, after drawing the scene:
if (ui.begin_frame(target_width, target_height)) {
    ui.begin_window("Settings", settings);
    ui.slider("Volume", volume, 0.0f, 1.0f);
    ui.tooltip("Output volume");
    ui.end_window();
    ui.end_frame(render_target_view);
}
// The host calls Present when ready.
```

`target_width` and `target_height` are physical pixels. Layout uses logical pixels at 96 DPI. Pass input from the corresponding HWND; its client coordinates must map to the target. The target should cover that client area, or you must map input to its coordinates yourself. `begin_frame()` without dimensions uses the HWND client size.

Use one thread for UI input, submission and drawing, and serialize access to the immediate context. For an engine with a separate render thread, arrange message forwarding and UI submission through your engine's synchronization; `Context` has no internal locking.

## Ownership and rendering state

The context holds COM references to the supplied device and immediate context. The host remains responsible for their lifecycle and recovery. The library does not create, clear, resize or present a swap chain in external mode. It alpha-blends UI into the RTV passed to `end_frame()` and restores the host's complete pipeline state using `SwapDeviceContextState`.

Restoration covers pipeline bindings and settings, not texture contents, query results or GPU side effects. Do not wrap UI rendering in application queries that should only measure the scene. Draw between host passes, after clearing the target and before Present. `set_vsync()` has no effect in this mode.

The RTV must belong to the supplied device. Keep it valid through `end_frame()`. The library does not keep that target in its saved UI state after drawing. When resizing, the host unbinds and releases its backbuffer views, calls ResizeBuffers, creates a new RTV, then supplies the new dimensions on the next frame.

## Images

Create a Texture2D SRV on the same device and pass it to `image()`:

```cpp
ui.image(icon_srv, 32, 32);
ui.image(atlas_srv, 64, 64, {0.25f, 0, 0.25f, 0.5f});
```

UV rectangles use normalized x/y/width/height. Texture sampling is point-filtered and clamped. RGBA8 UNORM is a suitable color format. Keep the SRV valid until the call; the renderer retains a reference until drawing finishes. Images consume layout rows, support tint and clipping, and form separate batches as needed. Do not sample the subresource currently bound as the destination.

For standalone mode, `ui.device()` returns a borrowed pointer suitable for creating image resources. Do not release that borrowed reference. Reacquire it after reset, and recreate textures on the new device.

## Device removal

When `device_lost()` is true, `begin_frame()` returns false in external mode. The host must recreate its device, immediate context, swap chain, target and images. Reconnect the library between frames:

```cpp
// After the host successfully creates its replacement GPU objects:
ui.reset_device(new_device, new_immediate_context);
// Resume frames using the new RTV and image SRVs.
```

This rebuilds shaders, font atlases and other renderer resources. Widget values remain in application storage; the context keeps interaction state. Old-device SRVs and render targets are rejected. `reset_device()` without arguments is for standalone mode, where automatic recovery is also attempted by `begin_frame()`.

Resource creation can still fail and throw. Keep normal application error handling around the render loop and reconnect only after the host has a working replacement. The regression tests exercise explicit resource recreation and external replacement; they do not trigger an actual driver timeout.

## DLL use

The same API is exported by `vgui-framework.dll`. Link with its import library and compatible MSVC/STL/runtime settings. DLL loading alone does not draw anything: the application must create a context, forward input and submit frames. No hooks or automatic renderer discovery are installed. Keep initialization and destruction outside `DllMain`.
