#include "internal.hpp"

namespace vgui {
void Context::image(ID3D11ShaderResourceView* texture, float width, float height, Rect uv,
                    Color tint) {
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0 ||
        !std::isfinite(uv.x) || !std::isfinite(uv.y) || !std::isfinite(uv.w) ||
        !std::isfinite(uv.h))
        throw std::invalid_argument("Invalid image size or UV coordinates");
    auto& s = *impl;
    auto bounds = s.row(height);
    bounds.w = std::min(bounds.w, width);
    s.lastBounds = bounds;
    s.lastItem = {bounds, s.hit(bounds), false, false};
    s.render.image(bounds, texture, uv, tint);
}
void Context::tooltip(std::string_view text, float delay) {
    auto& s = *impl;
    s.require_frame();
    if (!std::isfinite(delay) || delay < 0)
        throw std::invalid_argument("Tooltip delay must be nonnegative");
    if (!s.hasItem || !s.lastItem.hovered || s.mouseDown || text.empty())
        return;
    const auto now = GetTickCount64();
    if (s.hoverKey != s.lastKey) {
        s.hoverKey = s.lastKey;
        s.hoverSince = now;
    }
    s.hoverSeen = true;
    if (static_cast<double>(now - s.hoverSince) < delay * 1000.0)
        return;
    const int lines = 1 + static_cast<int>(std::count(text.begin(), text.end(), '\n'));
    float width = std::min(s.render.width, s.render.measure(text) + 16), height = lines * 17.f + 12;
    Rect bounds{std::clamp(s.mouseX + 12, 0.f, std::max(0.f, s.render.width - width)),
                std::clamp(s.mouseY + 20, 0.f, std::max(0.f, s.render.height - height)), width,
                height};
    auto clip = s.render.clip;
    int layer = s.render.layer;
    s.render.clip = {0, 0, s.render.width, s.render.height};
    s.render.layer = 2000000;
    s.render.quad(bounds, theme.background);
    s.render.bevel(bounds);
    s.render.clip =
        s.intersect(s.render.clip, {bounds.x + 2, bounds.y + 2, bounds.w - 4, bounds.h - 4});
    s.render.label(bounds.x + 8, bounds.y + 6, text, theme.text);
    s.render.clip = clip;
    s.render.layer = layer;
}
}
