# Getting started

[Documentation](README.md)

## Requirements

Windows, Visual Studio with **Desktop development with C++** and CMake tools,
a Windows SDK, CMake 3.20+ and a C++17 compiler. No external libraries or downloaded
assets are needed. Tahoma is rasterized through GDI when a context is created.

## Build

Run from the project root in PowerShell:

```powershell
.\build.ps1 -StaticRuntime
.\build\vgui_demo.exe
```

The script finds Visual Studio, including Insiders installations, an x64 toolchain
and a Windows SDK. It uses the CMake/Ninja tools shipped with Visual Studio, builds
Release and runs CTest. Build files go in `build/ninja`; the demo is also copied to
`build/vgui_demo.exe`. Close running copies before rebuilding.

`-StaticRuntime` links the C++ runtime into the executable. Omit it to use the shared
runtime. `-SkipTests` skips test execution.

For a DLL and a demo linked against it:

```powershell
.\build.ps1 -Shared
.\build\shared\vgui_demo.exe
```

This produces `vgui-framework.dll`, its import library `vgui-framework.lib`, and the demo in
`build/shared`. Keep the DLL beside the executable. DLL builds use the shared MSVC
runtime; `-Shared` cannot be combined with `-StaticRuntime`.

You can also use CMake from a Visual Studio developer shell:

```powershell
cmake -S . -B build/cmake
cmake --build build/cmake --config Release
ctest --test-dir build/cmake -C Release --output-on-failure
```

Add `-DVGUI_BUILD_SHARED=ON` for a DLL, or `-DVGUI_STATIC_RUNTIME=ON` for a standalone
static-runtime build. For Ninja, add `-G Ninja -DCMAKE_BUILD_TYPE=Release`.

## Link with CMake

```cmake
add_subdirectory(path/to/vgui vgui-build)
add_executable(my_app WIN32 main.cpp)
target_link_libraries(my_app PRIVATE vgui::vgui)
```

`vgui::vgui` is an alias for `vgui-framework`. The target supplies include paths, C++17,
Windows definitions and system libraries. A manual source build needs all five
`.cpp` files in `src/` and `d3d11`, `d3dcompiler`, `dxgi`, `gdi32`, `user32`.

To build the library as a DLL, set `VGUI_BUILD_SHARED` before `add_subdirectory`.
CMake propagates `VGUI_SHARED` to clients. When linking a prebuilt DLL manually,
define `VGUI_SHARED`, include `vgui.hpp`, and link its import library. Do not define
`VGUI_BUILDING_LIBRARY` in client code.

Use matching architecture, compatible MSVC/STL versions, runtime settings and
Debug/Release configuration for the DLL and client. This API passes C++ types across
the DLL boundary. Create and destroy contexts in normal application code, outside `DllMain`.

## Win32 integration

Create a native window, then a context:

```cpp
#include "vgui.hpp"

// window is an HWND owned by your application.
vgui::Context ui(window);
SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&ui));
```

Forward input from WndProc:

```cpp
LRESULT CALLBACK window_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    auto* ui = reinterpret_cast<vgui::Context*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (ui) ui->message(msg, wp, lp);
    if (msg == WM_CLOSE) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, msg, wp, lp);
}
```

Clear `GWLP_USERDATA` before destroying the context, including error paths. The
example assumes your application owns that slot. `message()` handles input; normal
Win32 processing and application shutdown remain the caller's responsibility.

Drain Win32 messages before submitting UI. Call `TranslateMessage` before
`DispatchMessageW` to generate `WM_CHAR` for text input. If all contexts return false
from `begin_frame()`, the application can use `WaitMessage()`.
See [demo/main.cpp](https://github.com/graveyardd1337/vgui-framework/blob/main/demo/main.cpp) for the complete loop and cleanup.

The context owns its device, swap chain and render target. It handles resize in
`begin_frame()`, then clears, draws and presents in `end_frame()`. If hardware D3D11
creation fails, the renderer tries WARP.

## Borderless desktop windows

Call `ui.set_borderless(true)` outside a frame to remove the Windows title bar and
border. Pass `vgui::WindowMode::Native` to `begin_window()` so the VGUI frame fills
the HWND and dragging its title moves the desktop window.

Use one native root frame per HWND. The demo creates two HWNDs with separate contexts.
Run it with `--framed` to keep the Windows borders. The default `WindowMode::Panel`
creates a movable panel inside the client area instead.

## Errors

DirectX/GDI failures throw `std::runtime_error`. Invalid lifecycle or scope calls
throw `std::logic_error`; invalid ranges and layout metrics throw `std::invalid_argument`.
Keep exceptions from crossing WndProc. The demo performs UI submission and rendering
inside a `try` block in the main loop.
