#include "settings.hpp"

namespace examples {
void show_settings(vgui::Context& ui, Settings& settings) {
    static const std::vector<std::string> pages{"General", "Audio", "Video"};
    static const std::vector<std::string> resolutions{"800 x 600", "1920 x 1080", "2560 x 1440"};

    ui.begin_window("Settings", settings.window);
    ui.tabs("pages", settings.page, pages);

    switch (settings.page) {
    case 0:
        ui.text_entry("Player", settings.player, 48);
        break;
    case 1:
        ui.checkbox("Enable sound", settings.sound);
        ui.slider("Volume", settings.volume, 0.0f, 1.0f);
        break;
    case 2:
        ui.combo_box("Resolution", settings.resolution, resolutions);
        break;
    }

    ui.spacing();
    ui.separator();
    ui.set_next_item_width(120);
    if (ui.button("Reset")) {
        settings.sound = true;
        settings.volume = 0.65f;
        settings.player = "Player";
        settings.resolution = 1;
    }
    ui.same_line();
    ui.set_next_item_width(120);
    if (ui.button("Mute"))
        settings.sound = false;
    ui.end_window();
}
}
