#include "vgui.hpp"
#include <algorithm>
#include <chrono>
#include <exception>
#include <string>

LRESULT CALLBACK window_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    auto* ui = reinterpret_cast<vgui::Context*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (ui)
        ui->message(msg, wp, lp);
    if (msg == WM_CLOSE) {
        PostQuitMessage(0);
        return 0;
    }
    if (msg == WM_ERASEBKGND)
        return 1;
    return DefWindowProcW(window, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR arguments, int show) {
    const std::wstring args(arguments);
    const bool smoke = args.find(L"--smoke") != std::wstring::npos;
    const bool framed = args.find(L"--framed") != std::wstring::npos;
    SetProcessDPIAware();
    WNDCLASSW wc{};
    wc.hInstance = instance;
    wc.lpfnWndProc = window_proc;
    wc.lpszClassName = L"VGUIFrameworkDemo";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassW(&wc))
        return 1;

    const int x = std::max(0, (GetSystemMetrics(SM_CXSCREEN) - 610) / 2);
    const int y = std::max(0, (GetSystemMetrics(SM_CYSCREEN) - 340) / 2);
    HWND downloadWindow = CreateWindowExW(0, wc.lpszClassName, L"Pre-loading", WS_OVERLAPPEDWINDOW,
                                          x, y, 350, 340, nullptr, nullptr, instance, nullptr);
    HWND gamesWindow =
        CreateWindowExW(0, wc.lpszClassName, L"Play games", WS_OVERLAPPEDWINDOW, x + 364, y + 32,
                        230, 340, nullptr, nullptr, instance, nullptr);
    if (!downloadWindow || !gamesWindow) {
        if (downloadWindow)
            DestroyWindow(downloadWindow);
        if (gamesWindow)
            DestroyWindow(gamesWindow);
        return 1;
    }
    int exitCode = 0;
    try {
        vgui::Context downloadUi(downloadWindow);
        vgui::Context gamesUi(gamesWindow);
        SetWindowLongPtrW(downloadWindow, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&downloadUi));
        SetWindowLongPtrW(gamesWindow, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&gamesUi));
        downloadUi.set_borderless(!framed);
        gamesUi.set_borderless(!framed);
        gamesUi.set_vsync(false);
        if (!smoke) {
            ShowWindow(downloadWindow, show);
            ShowWindow(gamesWindow, show);
        }

        vgui::Rect download{}, library{};
        float progress = 0.f;
        bool loading = false, browse = false;
        int selected = 0, frames = 0;
        std::vector<std::string> games{"Half-Life 2",        "Counter-Strike: Source",
                                       "Condition Zero",     "Counter-Strike",
                                       "Day of Defeat",      "Team Fortress Classic",
                                       "Deathmatch Classic", "Opposing Force",
                                       "Ricochet",           "Half-Life"};
        auto previousTime = std::chrono::steady_clock::now();
        bool running = true;
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
            const auto now = std::chrono::steady_clock::now();
            const float elapsed = std::chrono::duration<float>(now - previousTime).count();
            previousTime = now;
            if (loading) {
                progress = std::min(1.f, progress + std::min(elapsed, 0.1f) / 20.f);
                if (progress >= 1.f)
                    loading = false;
            }

            bool drew = false;
            if (downloadUi.begin_frame()) {
                drew = true;
                auto& ui = downloadUi;
                ui.begin_window("Pre-loading", download, vgui::WindowMode::Native);
                const auto normalText = ui.theme.text;
                ui.theme.text = ui.theme.accent;
                ui.text(games[selected]);
                ui.theme.text = normalText;
                ui.spacing(6);
                ui.text(std::to_string(static_cast<int>(progress * 100)) + "% pre-loaded");
                ui.progress_bar(progress); // 16 whole blocks by default.
                if (ui.button("Start / pause pre-loading")) {
                    if (progress >= 1.f)
                        progress = 0.f;
                    loading = !loading;
                }
                ui.spacing(10);
                ui.theme.text = ui.theme.light;
                ui.text(progress >= 1.f ? "Pre-load complete."
                                        : "Ready to pre-load onto your computer.");
                ui.text("Just a UI test. No downloads involved.");
                ui.theme.text = normalText;
                ui.spacing(35);
                ui.set_next_item_width(90);
                if (ui.button("Close"))
                    running = false;
                ui.same_line();
                ui.set_next_item_width(90);
                if (ui.button("Reset")) {
                    progress = 0.f;
                    loading = false;
                }
                ui.end_window();
                ui.end_frame();
            }
            if (gamesUi.begin_frame()) {
                drew = true;
                auto& ui = gamesUi;
                ui.begin_window("Play games", library, vgui::WindowMode::Native);
                ui.style.item_spacing = 4;
                const auto normalText = ui.theme.text;
                ui.theme.text = ui.theme.accent;
                ui.text(browse ? "AVAILABLE GAMES" : "MY GAMES");
                ui.theme.text = normalText;
                if (ui.list_box("##games", selected, games, 7)) {
                    progress = 0.f;
                    loading = false;
                }
                if (ui.button("Browse games...")) {
                    browse = !browse;
                    if (browse) {
                        games.push_back("Portal");
                        games.push_back("Team Fortress 2");
                    } else {
                        games.resize(10);
                        selected = std::min(selected, 9);
                    }
                }
                ui.end_window();
                ui.end_frame();
            }
            if (smoke && ++frames == 3)
                break;
            if (!drew)
                WaitMessage();
        }
        SetWindowLongPtrW(downloadWindow, GWLP_USERDATA, 0);
        SetWindowLongPtrW(gamesWindow, GWLP_USERDATA, 0);
    } catch (const std::exception& error) {
        SetWindowLongPtrW(downloadWindow, GWLP_USERDATA, 0);
        SetWindowLongPtrW(gamesWindow, GWLP_USERDATA, 0);
        if (!smoke)
            MessageBoxA(nullptr, error.what(), "VGUI error", MB_OK | MB_ICONERROR);
        exitCode = 1;
    }
    DestroyWindow(gamesWindow);
    DestroyWindow(downloadWindow);
    return exitCode;
}
