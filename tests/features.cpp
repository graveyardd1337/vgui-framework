#include "vgui.hpp"
#include <algorithm>
#include <cmath>
#include <d3d11_1.h>
#include <iostream>
#include <stdexcept>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
void require(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
void check(HRESULT hr) {
    require(SUCCEEDED(hr), "D3D test setup");
}
struct GPU {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    GPU() {
        check(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
                                D3D11_SDK_VERSION, &device, nullptr, &context));
    }
};
struct Surface {
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11RenderTargetView> target;
    Surface(ID3D11Device* device) {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = 640;
        desc.Height = 480;
        desc.MipLevels = desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        check(device->CreateTexture2D(&desc, nullptr, &texture));
        check(device->CreateRenderTargetView(texture.Get(), nullptr, &target));
    }
    uint32_t pixel(GPU& gpu, int x, int y) {
        D3D11_TEXTURE2D_DESC desc{};
        texture->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> copy;
        check(gpu.device->CreateTexture2D(&desc, nullptr, &copy));
        gpu.context->CopyResource(copy.Get(), texture.Get());
        D3D11_MAPPED_SUBRESOURCE map{};
        check(gpu.context->Map(copy.Get(), 0, D3D11_MAP_READ, 0, &map));
        auto value = reinterpret_cast<const uint32_t*>(static_cast<const char*>(map.pData) +
                                                       y * map.RowPitch)[x];
        gpu.context->Unmap(copy.Get(), 0);
        return value;
    }
};
struct TestDesktop {
    HWINSTA previousStation = GetProcessWindowStation();
    HDESK previousDesktop = GetThreadDesktop(GetCurrentThreadId());
    HWINSTA station = nullptr;
    HDESK desktop = nullptr;
    TestDesktop() {
        station = CreateWindowStationW(nullptr, 0, WINSTA_ALL_ACCESS, nullptr);
        require(station && SetProcessWindowStation(station), "create private test window station");
        desktop = CreateDesktopW(L"VguiTests", nullptr, nullptr, 0, GENERIC_ALL, nullptr);
        require(desktop && SetThreadDesktop(desktop), "create private test desktop");
    }
    ~TestDesktop() {
        SetThreadDesktop(previousDesktop);
        SetProcessWindowStation(previousStation);
        if (desktop)
            CloseDesktop(desktop);
        if (station)
            CloseWindowStation(station);
    }
};
void external_render(HWND window) {
    GPU gpu;
    Surface surface(gpu.device.Get()), host(gpu.device.Get());
    vgui::Context ui(window, gpu.device.Get(), gpu.context.Get());
    ui.set_dpi_scale(1);
    uint32_t green = 0xff00ff00;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = desc.Height = desc.MipLevels = desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{&green, 4, 0};
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> image;
    check(gpu.device->CreateTexture2D(&desc, &data, &texture));
    check(gpu.device->CreateShaderResourceView(texture.Get(), nullptr, &image));
    D3D11_VIEWPORT viewport{7, 9, 123, 87, 0.2f, 0.8f};
    gpu.context->RSSetViewports(1, &viewport);
    gpu.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    gpu.context->PSSetShaderResources(0, 1, image.GetAddressOf());
    gpu.context->OMSetRenderTargets(1, host.target.GetAddressOf(), nullptr);
    const float blue[] = {0, 0, 1, 1};
    gpu.context->ClearRenderTargetView(surface.target.Get(), blue);
    vgui::Rect bounds{20, 20, 300, 220};
    require(ui.begin_frame(640, 480), "external begin");
    ui.begin_window("external", bounds);
    ui.image(image.Get(), 40, 30);
    auto imageBounds = ui.last_item().bounds;
    ui.text(u8"Cyrillic: \u041f\u0440\u0438\u0432\u0435\u0442");
    ui.end_window();
    ui.end_frame(surface.target.Get());
    ComPtr<ID3D11RenderTargetView> restoredTarget;
    gpu.context->OMGetRenderTargets(1, &restoredTarget, nullptr);
    require(restoredTarget.Get() == host.target.Get(), "host render target restored");
    UINT count = 1;
    D3D11_VIEWPORT restored{};
    gpu.context->RSGetViewports(&count, &restored);
    require(restored.TopLeftX == 7 && restored.Width == 123 && restored.MinDepth == 0.2f,
            "host viewport restored");
    D3D11_PRIMITIVE_TOPOLOGY topology;
    gpu.context->IAGetPrimitiveTopology(&topology);
    require(topology == D3D11_PRIMITIVE_TOPOLOGY_LINELIST, "host topology restored");
    ComPtr<ID3D11ShaderResourceView> restoredImage;
    gpu.context->PSGetShaderResources(0, 1, &restoredImage);
    require(restoredImage.Get() == image.Get(), "host shader resource restored");
    require(surface.pixel(gpu, 630, 470) == 0xffff0000, "external mode does not clear host pixels");
    require(surface.pixel(gpu, static_cast<int>(imageBounds.x + 10),
                          static_cast<int>(imageBounds.y + 10)) == green,
            "image texture rendered");
    require(ui.frame_stats().draw_calls >= 3, "image draw batches");
    GPU replacement;
    Surface newSurface(replacement.device.Get());
    ui.reset_device(replacement.device.Get(), replacement.context.Get());
    require(ui.device() == replacement.device.Get() && !ui.device_lost(),
            "external replacement device");
    require(ui.begin_frame(640, 480), "frame after external reset");
    ui.begin_window("external", bounds);
    ui.text(u8"\u041f\u0440\u0438\u0432\u0435\u0442");
    bool rejected = false;
    try {
        ui.image(image.Get(), 20, 20);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "reject stale image from old device");
    ui.end_window();
    ui.end_frame(newSurface.target.Get());
}
void host_resize(HWND window) {
    GPU gpu;
    ComPtr<IDXGIDevice> dxgi;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory> factory;
    check(gpu.device.As(&dxgi));
    check(dxgi->GetAdapter(&adapter));
    check(adapter->GetParent(IID_PPV_ARGS(&factory)));
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Width = 640;
    desc.BufferDesc.Height = 480;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.OutputWindow = window;
    desc.Windowed = TRUE;
    ComPtr<IDXGISwapChain> chain;
    check(factory->CreateSwapChain(gpu.device.Get(), &desc, &chain));
    vgui::Context ui(window, gpu.device.Get(), gpu.context.Get());
    for (UINT width : {640u, 720u}) {
        ComPtr<ID3D11Texture2D> back;
        ComPtr<ID3D11RenderTargetView> target;
        check(chain->GetBuffer(0, IID_PPV_ARGS(&back)));
        check(gpu.device->CreateRenderTargetView(back.Get(), nullptr, &target));
        require(ui.begin_frame(width, 480), "host swapchain frame");
        vgui::Rect bounds{0, 0, 200, 100};
        ui.begin_window("host", bounds);
        ui.text("resize");
        ui.end_window();
        ui.end_frame(target.Get());
        target.Reset();
        back.Reset();
        gpu.context->OMSetRenderTargets(0, nullptr, nullptr);
        require(SUCCEEDED(chain->ResizeBuffers(0, 720, 480, DXGI_FORMAT_UNKNOWN, 0)),
                "UI releases the host backbuffer after drawing");
    }
}
void text_and_dpi(HWND window) {

    vgui::Context ui(window);
    ui.set_vsync(false);
    ui.set_dpi_scale(1);
    vgui::Rect bounds{0, 0, 400, 300};
    std::string value = u8"\u0410\u0411\U0001f600";
    size_t maxLength = 256;
    auto draw = [&] {
        require(ui.begin_frame(), "text frame");
        ui.begin_window("editor", bounds);
        ui.text_entry("text", value, maxLength);
        ui.end_window();
        ui.end_frame();
    };
    auto key = [&](WPARAM k) {
        ui.message(WM_KEYDOWN, k, 0);
        draw();
        ui.message(WM_KEYUP, k, 0);
    };
    auto ctrl = [&](WPARAM k) {
        ui.message(WM_KEYDOWN, VK_CONTROL, 0);
        key(k);
        ui.message(WM_KEYUP, VK_CONTROL, 0);
    };
    draw();
    ui.message(WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(40, 72));
    draw();
    ui.message(WM_LBUTTONUP, 0, MAKELPARAM(40, 72));
    draw();
    key(VK_END);
    key(VK_LEFT);
    key(VK_DELETE);
    require(value == u8"\u0410\u0411", "delete one Unicode codepoint");
    ui.message(WM_CHAR, 0xd83d, 0);
    draw();
    ui.message(WM_CHAR, 0xde00, 0);
    draw();
    require(value == u8"\u0410\u0411\U0001f600", "UTF16 surrogate pair input across frames");
    ctrl('A');
    ctrl('C');
    key(VK_DELETE);
    require(value.empty(), "select all deletes selection");
    ctrl('V');
    require(value == u8"\u0410\u0411\U0001f600", "Unicode clipboard roundtrip");
    key(VK_HOME);
    ui.message(WM_KEYDOWN, VK_SHIFT, 0);
    key(VK_RIGHT);
    ui.message(WM_KEYUP, VK_SHIFT, 0);
    ui.message(WM_CHAR, 0x42f, 0);
    draw();
    require(value == u8"\u042f\u0411\U0001f600", "replace Shift selection");
    ctrl('A');
    ctrl('X');
    require(value.empty(), "cut selection");
    ctrl('V');
    ui.message(WM_CHAR, 8, 0);
    draw();
    require(value == u8"\u042f\u0411", "backspace keeps UTF8 boundaries");
    ui.message(WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(20, 72));
    draw();
    ui.message(WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(300, 72));
    draw();
    ui.message(WM_LBUTTONUP, 0, MAKELPARAM(300, 72));
    draw();
    ui.message(WM_CHAR, 'x', 0);
    draw();
    require(value == "x", "typing replaces mouse-drag selection");
    maxLength = 2;
    ui.message(WM_CHAR, 0x42f, 0);
    draw();
    require(value == "x", "byte limit rejects partial codepoint");
    ctrl('A');
    ui.message(WM_CHAR, 0x42f, 0);
    draw();
    require(value == u8"\u042f", "selection replacement uses freed byte capacity");
    ui.set_dpi_scale(2);
    bool checked = false;
    vgui::ItemState item;
    auto scaled = [&] {
        require(ui.begin_frame(), "scaled frame");
        ui.begin_window("scaled", bounds, vgui::WindowMode::Native);
        ui.checkbox("check", checked);
        item = ui.last_item();
        ui.end_window();
        ui.end_frame();
    };
    scaled();
    RECT client{};
    GetClientRect(window, &client);
    require(bounds.w == client.right / 2.f && ui.dpi_scale() == 2, "DPI logical dimensions");
    int x = static_cast<int>((item.bounds.x + 5) * 2),
        y = static_cast<int>((item.bounds.y + 5) * 2);
    ui.message(WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
    scaled();
    ui.message(WM_LBUTTONUP, 0, MAKELPARAM(x, y));
    scaled();
    require(checked, "DPI hit testing");
    ui.reset_device();
    scaled();
    require(!ui.device_lost() && checked, "standalone resource recreation preserves UI values");
    ui.set_borderless(true);
    RECT wr{};
    GetWindowRect(window, &wr);
    LRESULT result = 0;
    require(ui.window_message(WM_NCHITTEST, 0, MAKELPARAM(wr.left + 1, wr.top + 1), result) &&
                result == HTTOPLEFT,
            "borderless corner resize");
    require(ui.window_message(WM_NCHITTEST, 0, MAKELPARAM(wr.right - 1, wr.bottom - 1), result) &&
                result == HTBOTTOMRIGHT,
            "opposite corner resize");
    MINMAXINFO limits{};
    ui.set_resizable(true, 200, 100);
    require(ui.window_message(WM_GETMINMAXINFO, 0, reinterpret_cast<LPARAM>(&limits), result) &&
                limits.ptMinTrackSize.x == 400,
            "DPI minimum resize size");
    ui.set_borderless(false);
    ui.set_borderless(true);
    ui.set_resizable(false);
    require(!(GetWindowLongPtrW(window, GWL_STYLE) & WS_THICKFRAME), "disable resize style");
    require(!ui.window_message(WM_NCHITTEST, 0, MAKELPARAM(wr.left + 1, wr.top + 1), result),
            "disabled resize does not return an edge");
    ui.set_resizable(true);
    require(GetWindowLongPtrW(window, GWL_STYLE) & WS_THICKFRAME, "restore resize style");
    ui.set_borderless(false);
}
void layers_and_scroll(HWND window) {
    GPU gpu;
    Surface surface(gpu.device.Get());
    vgui::Context ui(window, gpu.device.Get(), gpu.context.Get());
    ui.set_dpi_scale(1);
    vgui::Rect a{0, 0, 240, 250}, b{100, 20, 240, 250};
    int first = 0, second = 0;
    auto draw = [&] {
        require(ui.begin_frame(640, 480), "layer frame");
        ui.theme.panel = {200, 0, 0};
        ui.begin_window("A", a);
        if (ui.button("A button"))
            ++first;
        ui.end_window();
        ui.theme.panel = {0, 200, 0};
        ui.begin_window("B", b);
        if (ui.button("B button"))
            ++second;
        ui.end_window();
        ui.end_frame(surface.target.Get());
    };
    auto click = [&](int x, int y) {
        ui.message(WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
        draw();
        ui.message(WM_LBUTTONUP, 0, MAKELPARAM(x, y));
        draw();
    };
    draw();
    require((surface.pixel(gpu, 150, 150) & 0xffffff) == 0x00c800, "initial drawing order");
    click(150, 62);
    require(first == 0 && second == 1, "only top window receives overlapping click");
    click(30, 48);
    require(first == 1, "activate exposed rear window");
    require((surface.pixel(gpu, 150, 150) & 0xffffff) == 0x0000c8,
            "activated window drawn above later submitted window");
    click(150, 62);
    require(first == 2 && second == 1, "input follows changed z-order");
    bool showTip = false;
    float firstY = 0;
    size_t noTip = 0, withTip = 0;
    auto scroll = [&] {
        require(ui.begin_frame(640, 480), "scroll frame");
        ui.begin_window("scroll", a);
        ui.begin_scroll_panel("items", {8, 35, 220, 170});
        for (int i = 0; i < 20; ++i) {
            ui.push_id(i);
            ui.button("row");
            if (i == 0)
                firstY = ui.last_item().bounds.y;
            if (showTip)
                ui.tooltip("tip", 0);
            ui.pop_id();
        }
        ui.end_scroll_panel();
        ui.end_window();
        ui.end_frame(surface.target.Get());
    };
    scroll();
    scroll();
    float initial = firstY;
    POINT p{50, 100};
    ClientToScreen(window, &p);
    ui.message(WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(-WHEEL_DELTA)), MAKELPARAM(p.x, p.y));
    scroll();
    require(firstY < initial, "wheel scrolls panel content");
    ui.message(WM_MOUSEMOVE, 0, MAKELPARAM(50, 100));
    scroll();
    noTip = ui.frame_stats().vertices;
    showTip = true;
    scroll();
    withTip = ui.frame_stats().vertices;
    require(withTip > noTip, "hover tooltip draws overlay");
    float nestedY = 0;
    auto nested = [&] {
        require(ui.begin_frame(640, 480), "nested scroll frame");
        ui.begin_window("nested", a);
        ui.begin_scroll_panel("outer", {8, 35, 220, 170});
        ui.begin_panel("child", {4, 50, 170, 280});
        ui.button("child item");
        nestedY = ui.last_item().bounds.y;
        ui.end_panel();
        ui.end_scroll_panel();
        ui.end_window();
        ui.end_frame(surface.target.Get());
    };
    nested();
    nested();
    float nestedInitial = nestedY;
    ui.message(WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(-WHEEL_DELTA)), MAKELPARAM(p.x, p.y));
    nested();
    require(nestedY < nestedInitial, "nested child panel contributes height and moves with scroll");
}
int main() {
    TestDesktop testDesktop;
    HWND window = CreateWindowExW(0, L"STATIC", L"VGUI feature tests", WS_OVERLAPPEDWINDOW, 0, 0,
                                  800, 650, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window)
        return 1;
    try {
        std::cout << "external" << std::endl;
        external_render(window);
        host_resize(window);
        std::cout << "text" << std::endl;
        text_and_dpi(window);
        std::cout << "layers" << std::endl;
        layers_and_scroll(window);
        std::cout << "External render, state restore, textures, Unicode, clipboard, DPI, resize, "
                     "reset, z-order and scrolling passed.\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        DestroyWindow(window);
        return 1;
    }
    DestroyWindow(window);
    return 0;
}
