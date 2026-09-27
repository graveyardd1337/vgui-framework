#include "vgui.hpp"
#include <algorithm>
#include <d3d11.h>
#include <iostream>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
LRESULT CALLBACK gallery_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    auto* ui = reinterpret_cast<vgui::Context*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    LRESULT result = 0;
    if (ui && ui->window_message(msg, wp, lp, result))
        return result;
    if (msg == WM_CLOSE) {
        PostQuitMessage(0);
        return 0;
    }
    if (msg == WM_ERASEBKGND)
        return 1;
    return DefWindowProcW(window, msg, wp, lp);
}
ComPtr<ID3D11ShaderResourceView> checker(ID3D11Device* device) {
    uint32_t pixels[64];
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            pixels[y * 8 + x] = (x + y) % 2 ? 0xff506c91 : 0xff35422a;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = desc.Height = 8;
    desc.MipLevels = desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{pixels, 8 * 4, 0};
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> view;
    if (FAILED(device->CreateTexture2D(&desc, &data, &texture)) ||
        FAILED(device->CreateShaderResourceView(texture.Get(), nullptr, &view)))
        throw std::runtime_error("Could not create demo texture");
    return view;
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR args, int show) {
    const bool smoke = std::wstring_view(args).find(L"--smoke") != std::wstring_view::npos;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSW wc{};
    wc.hInstance = instance;
    wc.lpfnWndProc = gallery_proc;
    wc.lpszClassName = L"VGUIFeatures";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassW(&wc))
        return 1;
    HWND window = CreateWindowExW(0, wc.lpszClassName, L"test", WS_OVERLAPPEDWINDOW, 100, 100, 900,
                                  650, nullptr, nullptr, instance, nullptr);
    if (!window)
        return 1;
    int exitCode = 0;
    try {
        vgui::Context ui(window);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&ui));
        ui.set_borderless(true);
        ui.set_resizable(true, 520, 360);
        if (!smoke)
            ShowWindow(window, show);
        ComPtr<ID3D11Device> textureDevice = ui.device();
        auto texture = checker(textureDevice.Get());
        std::string text = u8"\u041f\u0440\u0438\u0432\u0435\u0442, VGUI!";
        vgui::Rect main{8, 8, 400, 600}, floating{360, 90, 320, 240};
        int scale = 0, applied = -1, clicks = 0, frames = 0;
        bool running = true, reset = false, enabled = true;
        while (running) {
            MSG msg{};
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    running = false;
                    break;
                }
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            if (!running)
                break;
            if (scale != applied) {
                ui.set_dpi_scale(scale == 0 ? 0 : scale == 1 ? 1 : scale == 2 ? 1.5f : 2);
                applied = scale;
            }
            if (reset) {
                ui.reset_device();
                reset = false;
            }
            if (!ui.begin_frame()) {
                if (smoke)
                    break;
                Sleep(16);
                continue;
            }
            if (textureDevice.Get() != ui.device()) {
                textureDevice = ui.device();
                texture = checker(textureDevice.Get());
            }
            RECT client{};
            GetClientRect(window, &client);
            main.h = std::max(260.f, client.bottom / ui.dpi_scale() - 16);
            ui.begin_window("test", main);
            ui.begin_scroll_panel(
                "test", {10, 35, std::max(260.f, main.w - 20), std::max(150.f, main.h - 48)});
            ui.set_next_item_width(280);
            ui.combo_box("DPI", scale, {"Automatic", "100%", "150%", "200%"});
            ui.set_next_item_width(320);
            ui.text_entry("UTF-8", text);
            ui.tooltip("Select text, then Ctrl+C / Ctrl+V.\nCtrl+A selects everything.");
            ui.text(text);
            ui.image(texture.Get(), 96, 64);
            ui.tooltip("Image from a D3D11 texture");
            ui.set_next_item_width(180);
            if (ui.button("Reset device"))
                reset = true;
            ui.tooltip("Rebuild GPU resources; keep widget values.");
            for (int i = 0; i < 16; ++i) {
                ui.push_id(i);
                ui.checkbox("test", enabled);
                ui.pop_id();
            }
            if (ui.button("Close"))
                running = false;
            ui.end_scroll_panel();
            ui.end_window();
            ui.begin_window("test##floating", floating);
            ui.text("Drag me. Click an exposed window to raise it.");
            if (ui.button("test"))
                ++clicks;
            ui.tooltip("Only the front window receives this click.");
            ui.text("clicks: " + std::to_string(clicks));
            ui.text("Resize the desktop window by its edges.");
            ui.end_window();
            ui.end_frame();
            if (smoke && ++frames == 3)
                break;
        }
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
    } catch (const std::exception& e) {
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        exitCode = 1;
        if (!smoke)
            MessageBoxA(window, e.what(), "VGUI", MB_ICONERROR);
    }
    DestroyWindow(window);
    return exitCode;
}
