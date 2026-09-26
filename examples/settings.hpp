#pragma once
#include "vgui.hpp"

namespace examples {
struct Settings {
    vgui::Rect window{20, 20, 460, 390};
    bool sound = true;
    float volume = 0.65f;
    int page = 0;
    int resolution = 1;
    std::string player = "Player";
};

// Call inside a frame.
void show_settings(vgui::Context& ui, Settings& settings);
}
