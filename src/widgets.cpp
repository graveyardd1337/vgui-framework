#include "internal.hpp"

namespace vgui {
void Context::progress_bar(float fraction, int segments) {
    auto& state = *impl;
    auto rect = state.row(20);
    fraction = std::clamp(std::isfinite(fraction) ? fraction : 0.f, 0.f, 1.f);
    state.render.quad(rect, theme.background);
    state.render.bevel(rect, true);
    const float available = std::max(0.f, rect.w - 4);
    const int slots = std::min(std::max(1, segments), static_cast<int>((available + 2) / 3));
    if (slots == 0)
        return;
    const float step = (available + 2) / slots;
    const int filled = static_cast<int>(fraction * slots);
    for (int i = 0; i < filled; ++i)
        state.render.quad({rect.x + 2 + i * step, rect.y + 3, step - 2, rect.h - 6}, theme.accent);
}

void Context::text(std::string_view label) {
    auto& state = *impl;
    auto bounds = state.row(17);
    state.render.label(bounds.x, bounds.y, label, theme.text);
}

bool Context::button(std::string_view label) {
    auto& state = *impl;
    auto bounds = state.row(27);
    auto key = state.id(label);
    bool clicked = state.interact(key, bounds);
    bool held = state.activeId == key && state.mouseDown;
    state.render.quad(bounds, state.hit(bounds) ? theme.hovered : theme.panel);
    state.render.bevel(bounds, held);
    state.render.label(bounds.x + 10 + (held ? 1 : 0), bounds.y + 6 + (held ? 1 : 0),
                       caption(label), theme.text);
    return clicked;
}

bool Context::checkbox(std::string_view label, bool& value) {
    auto& state = *impl;
    auto bounds = state.row(20);
    auto key = state.id(label);
    bool changed = state.interact(key, bounds);
    if (changed)
        value = !value;
    Rect box{bounds.x, bounds.y + 2, 15, 15};
    state.render.quad(box, theme.background);
    state.render.bevel(box, true);
    if (value)
        state.render.checkmark(box);
    state.render.label(bounds.x + 23, bounds.y + 2, caption(label), theme.text);
    return changed;
}

bool Context::slider(std::string_view label, float& value, float minimum, float maximum) {
    auto& state = *impl;
    auto bounds = state.row(43);
    auto key = state.id(label);
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || maximum <= minimum)
        throw std::invalid_argument("Slider range must be finite and increasing");
    float oldValue = value;
    value = std::clamp(std::isfinite(value) ? value : minimum, minimum, maximum);
    Rect track{bounds.x + 4, bounds.y + 29, bounds.w - 8, 4};
    state.interact(key, bounds);
    if (state.activeId == key && (state.mouseDown || state.mouseReleased))
        value = minimum +
                std::clamp((state.mouseX - track.x) / track.w, 0.f, 1.f) * (maximum - minimum);
    if (state.focusedId == key && state.arrowX)
        value = std::clamp(value + state.arrowX * (maximum - minimum) / 100, minimum, maximum);
    char number[32];
    std::snprintf(number, sizeof(number), "  %.2f", value);
    state.render.label(bounds.x, bounds.y, std::string(caption(label)) + number, theme.text);
    state.render.quad(track, theme.track);
    state.render.bevel(track, true);
    for (int tick = 0; tick <= 10; ++tick)
        state.render.quad({std::floor(track.x + tick * (track.w - 1) / 10), track.y + 8, 1, 4},
                          theme.light);
    float x = std::floor(track.x + (value - minimum) / (maximum - minimum) * track.w);
    Rect knob{x - 4, track.y - 6, 8, 16};
    state.render.quad(knob, theme.panel);
    state.render.bevel(knob);
    return value != oldValue;
}

bool Context::Impl::scrollbar(const std::string& key, Rect bounds, int& position, int total,
                              int visible) {
    int oldValue = position, maximum = std::max(0, total - visible);
    position = std::clamp(position, 0, maximum);
    Rect upButton{bounds.x, bounds.y, bounds.w, 17},
        downButton{bounds.x, bounds.y + bounds.h - 17, bounds.w, 17};
    render.quad(bounds, theme->scroll_track);
    render.bevel(bounds, true);
    if (interact(key + "/up", upButton))
        --position;
    if (interact(key + "/down", downButton))
        ++position;
    render.quad(upButton, theme->panel);
    render.bevel(upButton, activeId == key + "/up" && mouseDown);
    render.quad(downButton, theme->panel);
    render.bevel(downButton, activeId == key + "/down" && mouseDown);
    render.arrow_icon(upButton, true, activeId == key + "/up" && mouseDown);
    render.arrow_icon(downButton, false, activeId == key + "/down" && mouseDown);
    Rect track{bounds.x + 1, bounds.y + 18, bounds.w - 2, std::max(1.f, bounds.h - 36)};
    float thumb = std::min(
        track.h,
        std::max(18.f, track.h * std::min(1.f, static_cast<float>(visible) / std::max(1, total))));
    float travel = track.h - thumb;
    Rect handle{track.x, track.y + (maximum ? travel * position / maximum : 0), track.w, thumb};
    tabOrder.push_back(key);
    if (mousePressed && hit(track) && activeId.empty()) {
        activeId = key;
        focusedId = key;
        if (contains(handle, mouseX, mouseY))
            dragOffsetY = mouseY - handle.y;
        else {
            position += mouseY < handle.y ? -visible : visible;
            dragOffsetY = thumb / 2;
        }
    } else if (activeId == key && (mouseDown || mouseReleased) && travel > 0 && maximum > 0) {
        position = static_cast<int>(
            std::lround(std::clamp((mouseY - track.y - dragOffsetY) / travel, 0.f, 1.f) * maximum));
    }
    if (hit(bounds) && wheel)
        position -= wheel * 3;
    if (focusedId == key && (openPopup.empty() || inPopup)) {
        position += arrowY;
        if (homePressed)
            position = 0;
        if (endPressed)
            position = maximum;
    }
    position = std::clamp(position, 0, maximum);
    handle.y = track.y + (maximum ? travel * position / maximum : 0);
    render.quad(handle, theme->panel);
    render.bevel(handle);
    if (handle.h > 25)
        for (int i = 0; i < 3; ++i)
            render.quad({handle.x + 4, handle.y + handle.h / 2 - 3 + i * 3, handle.w - 8, 1},
                        theme->shadow);
    return position != oldValue;
}

bool Context::scroll_bar(std::string_view label, int& position, int total, int visible,
                         float height) {
    auto& state = *impl;
    auto bounds = state.row(std::max(70.f, height) + 20);
    state.render.label(bounds.x, bounds.y, caption(label), theme.text);
    auto key = state.id(label);
    bool changed =
        state.scrollbar(key, {bounds.x + bounds.w - 19, bounds.y + 20, 19, bounds.h - 20}, position,
                        std::max(0, total), std::max(1, visible));
    state.record_item(key, bounds);
    return changed;
}

bool Context::Impl::list(const std::string& key, Rect bounds, int* selected,
                         std::vector<bool>* multiple, const std::vector<std::string>& items,
                         int visibleRows) {
    bool changed = false;
    int itemCount = static_cast<int>(items.size());
    visibleRows = std::max(1, visibleRows);
    if (multiple)
        multiple->resize(items.size(), false);
    if (selected) {
        int value = itemCount ? std::clamp(*selected, 0, itemCount - 1) : -1;
        changed = value != *selected;
        *selected = value;
    }
    int& firstVisible = scrolls[key];
    firstVisible = std::clamp(firstVisible, 0, std::max(0, itemCount - visibleRows));
    render.quad(bounds, theme->background);
    render.bevel(bounds, true);
    Rect content{bounds.x + 2, bounds.y + 2, bounds.w - (itemCount > visibleRows ? 23.f : 4.f),
                 bounds.h - 4};
    bool hovered = hit(content);
    if (hovered && wheel)
        firstVisible =
            std::clamp(firstVisible - wheel * 3, 0, std::max(0, itemCount - visibleRows));
    if (itemCount > visibleRows)
        scrollbar(key + "/scroll", {bounds.x + bounds.w - 20, bounds.y + 2, 18, bounds.h - 4},
                  firstVisible, itemCount, visibleRows);
    tabOrder.push_back(key);
    if (mousePressed && hovered && activeId.empty()) {
        activeId = key;
        focusedId = key;
    }
    int clicked = -1;
    if (mouseReleased && activeId == key && hovered)
        clicked = firstVisible + static_cast<int>((mouseY - content.y) / 22);
    int& cursorIndex = scrolls[key + "/cursor"];
    cursorIndex = std::clamp(cursorIndex, 0, std::max(0, itemCount - 1));
    if (selected && *selected >= 0)
        cursorIndex = *selected;
    if (focusedId == key && (openPopup.empty() || inPopup) && itemCount) {
        int oldIndex = cursorIndex;
        cursorIndex = std::clamp(cursorIndex + arrowY, 0, itemCount - 1);
        if (homePressed)
            cursorIndex = 0;
        if (endPressed)
            cursorIndex = itemCount - 1;
        if (cursorIndex != oldIndex) {
            if (selected) {
                *selected = cursorIndex;
                changed = true;
            }
            firstVisible =
                std::clamp(firstVisible, std::max(0, cursorIndex - visibleRows + 1), cursorIndex);
        }
        if (activatePressed)
            clicked = cursorIndex;
    }
    if (clicked >= 0 && clicked < itemCount && clicked < firstVisible + visibleRows) {
        cursorIndex = clicked;
        if (selected) {
            changed |= *selected != clicked;
            *selected = clicked;
        }
        if (multiple) {
            (*multiple)[clicked] = !(*multiple)[clicked];
            changed = true;
        }
    }
    Rect savedClip = render.clip;
    render.clip = intersect(render.clip, content);
    for (int i = firstVisible; i < std::min(itemCount, firstVisible + visibleRows); ++i) {
        Rect item{content.x, content.y + (i - firstVisible) * 22.f, content.w, 22};
        bool chosen = selected ? *selected == i : (*multiple)[i];
        if (chosen)
            render.quad(item, theme->selected);
        else if (hit(item))
            render.quad(item, theme->hovered);
        float x = item.x + 6;
        if (multiple) {
            Rect box{x, item.y + 4, 13, 13};
            render.quad(box, theme->background);
            render.bevel(box, true);
            if (chosen)
                render.checkmark(box);
            x += 21;
        }
        render.label(x, item.y + 4, items[i], theme->text);
    }
    if (items.empty())
        render.label(content.x + 6, content.y + 4, "(empty)", theme->light);
    render.clip = savedClip;
    return changed;
}

bool Context::list_box(std::string_view label, int& selected, const std::vector<std::string>& items,
                       int visibleRows) {
    auto& state = *impl;
    visibleRows = std::max(1, visibleRows);
    auto bounds = state.row(visibleRows * 22.f + 24);
    state.render.label(bounds.x, bounds.y, caption(label), theme.text);
    auto key = state.id(label);
    bool changed = state.list(key, {bounds.x, bounds.y + 20, bounds.w, bounds.h - 20}, &selected,
                              nullptr, items, visibleRows);
    state.record_item(key, bounds);
    return changed;
}

bool Context::multi_box(std::string_view label, std::vector<bool>& selected,
                        const std::vector<std::string>& items, int visibleRows) {
    auto& state = *impl;
    visibleRows = std::max(1, visibleRows);
    auto bounds = state.row(visibleRows * 22.f + 24);
    state.render.label(bounds.x, bounds.y, caption(label), theme.text);
    auto key = state.id(label);
    bool changed = state.list(key, {bounds.x, bounds.y + 20, bounds.w, bounds.h - 20}, nullptr,
                              &selected, items, visibleRows);
    state.record_item(key, bounds);
    return changed;
}

bool Context::tabs(std::string_view label, int& selected, const std::vector<std::string>& items) {
    auto& state = *impl;
    auto bounds = state.row(28);
    int oldValue = selected, itemCount = static_cast<int>(items.size());
    selected = itemCount ? std::clamp(selected, 0, itemCount - 1) : -1;
    if (!itemCount)
        return oldValue != selected;
    float width = bounds.w / itemCount;
    for (int i = 0; i < itemCount; ++i) {
        Rect tabRect{bounds.x + i * width, bounds.y + (i == selected ? 0 : 3.f), width - 2,
                     i == selected ? 28.f : 25.f};
        auto key = state.id(label) + "/" + std::to_string(i);
        if (state.interact(key, tabRect))
            selected = i;
        if (state.focusedId == key && state.arrowX && state.openPopup.empty()) {
            selected = std::clamp(i + state.arrowX, 0, itemCount - 1);
            state.focusedId = state.id(label) + "/" + std::to_string(selected);
            state.arrowX = 0;
        }
        state.render.quad(tabRect, i == selected ? theme.panel : theme.background);
        state.render.bevel(tabRect);
        Rect savedClip = state.render.clip;
        state.render.clip = state.intersect(state.render.clip, tabRect);
        state.render.label(tabRect.x + 9, tabRect.y + 7, items[i],
                           i == selected ? theme.accent : theme.text);
        state.render.clip = savedClip;
        if (i == selected)
            state.render.quad({tabRect.x + 1, tabRect.y + tabRect.h - 1, tabRect.w - 2, 1},
                              theme.panel);
    }
    state.record_item(state.id(label), bounds);
    return selected != oldValue;
}

bool Context::text_entry(std::string_view label, std::string& value, size_t maxLength) {
    auto& state = *impl;
    auto bounds = state.row(46);
    auto key = state.id(label);
    state.render.label(bounds.x, bounds.y, caption(label), theme.text);
    Rect field{bounds.x, bounds.y + 20, bounds.w, 26};
    state.interact(key, field);
    size_t& caret = state.carets[key];
    caret = std::min(caret, value.size());
    bool editing = state.focusedId == key && state.openPopup.empty();
    std::string oldValue = value;
    float available = std::max(1.f, field.w - 12);
    size_t start = 0;
    if (editing)
        while (start < caret && state.render.measure(std::string_view(value).substr(
                                    start, caret - start)) > available)
            ++start;
    if (state.mousePressed && state.hit(field)) {
        caret = start;
        float x = field.x + 6;
        while (caret < value.size()) {
            float w = state.render.measure(std::string_view(value).substr(caret, 1));
            if (state.mouseX < x + w / 2)
                break;
            x += w;
            ++caret;
        }
    }
    if (editing) {
        if (state.arrowX < 0 && caret)
            --caret;
        if (state.arrowX > 0 && caret < value.size())
            ++caret;
        if (state.homePressed)
            caret = 0;
        if (state.endPressed)
            caret = value.size();
        if (state.deletePressed && caret < value.size())
            value.erase(caret, 1);
        for (char c : state.textInput) {
            if (c == 8) {
                if (caret)
                    value.erase(--caret, 1);
            } else if (value.size() < maxLength) {
                value.insert(caret, 1, c);
                ++caret;
            }
        }
        if (state.escapePressed) {
            state.focusedId.clear();
            editing = false;
        }
    }
    state.render.quad(field, theme.background);
    state.render.bevel(field, true);
    Rect savedClip = state.render.clip;
    state.render.clip =
        state.intersect(state.render.clip, {field.x + 5, field.y + 3, field.w - 10, field.h - 6});
    start = 0;
    if (editing)
        while (start < caret && state.render.measure(std::string_view(value).substr(
                                    start, caret - start)) > available)
            ++start;
    state.render.label(field.x + 6, field.y + 6, std::string_view(value).substr(start), theme.text);
    if (editing && (GetTickCount64() / 500) % 2 == 0)
        state.render.quad(
            {field.x + 6 +
                 state.render.measure(std::string_view(value).substr(start, caret - start)),
             field.y + 5, 1, 15},
            theme.text);
    state.render.clip = savedClip;
    return value != oldValue;
}

bool Context::combo_box(std::string_view label, int& selected,
                        const std::vector<std::string>& items) {
    auto& state = *impl;
    auto bounds = state.row(46);
    auto key = state.id(label);
    int oldValue = selected, itemCount = static_cast<int>(items.size());
    selected = itemCount ? std::clamp(selected, 0, itemCount - 1) : -1;
    state.render.label(bounds.x, bounds.y, caption(label), theme.text);
    Rect field{bounds.x, bounds.y + 20, bounds.w, 26};
    bool wasOpen = state.openPopup == key;
    state.inPopup = wasOpen;
    if (state.interact(key, field) && itemCount) {
        if (wasOpen)
            state.openPopup.clear();
        else {
            state.openPopup = key;
            state.scrolls[key + "/menu"] = std::max(0, selected - 3);
        }
    }
    if (state.focusedId == key && state.openPopup.empty() && state.arrowY && itemCount)
        selected = std::clamp(selected + state.arrowY, 0, itemCount - 1);
    state.render.quad(field, theme.background);
    state.render.bevel(field, true);
    Rect arrowButton{field.x + field.w - 24, field.y + 2, 22, 22};
    bool held = state.activeId == key && state.mouseDown;
    state.render.quad(arrowButton, theme.panel);
    state.render.bevel(arrowButton, held);
    state.render.arrow_icon(arrowButton, false, held);
    Rect savedClip = state.render.clip;
    state.render.clip =
        state.intersect(state.render.clip, {field.x + 4, field.y + 2, field.w - 30, 22});
    state.render.label(field.x + 6, field.y + 6, itemCount ? items[selected] : "(empty)",
                       theme.text);
    state.render.clip = savedClip;
    if (state.openPopup == key && itemCount) {
        state.popupDrawn = true;
        state.inPopup = true;
        state.popupAnchor = field;
        int visibleRows = std::min(itemCount, 6);
        float height = visibleRows * 22.f + 4;
        float y = field.y + field.h;
        if (y + height > state.render.height)
            y = std::max(0.f, field.y - height);
        state.popupBounds = {field.x, y, field.w, height};
        state.render.clip = {0, 0, static_cast<float>(state.render.width),
                             static_cast<float>(state.render.height)};
        state.render.vertices.swap(state.render.overlay);
        state.list(key + "/menu", state.popupBounds, &selected, nullptr, items, visibleRows);
        state.render.vertices.swap(state.render.overlay);
        state.render.clip = savedClip;
        bool release = state.mouseReleased && state.activeId == key + "/menu" &&
                       contains(state.popupBounds, state.mouseX, state.mouseY);
        bool accept = state.focusedId == key + "/menu" && state.activatePressed;
        if (release || accept) {
            state.openPopup.clear();
            state.focusedId = key;
        } else if (!wasOpen)
            state.focusedId = key + "/menu";
    }
    state.record_item(key, bounds);
    state.inPopup = false;
    return oldValue != selected;
}
}
