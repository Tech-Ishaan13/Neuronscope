#include "tui/panels/packet_stream_panel.h"
#include "tui/theme.h"
#include "ftxui/component/event.hpp"
#include <iomanip>
#include <sstream>
#include <ctime>
#include <algorithm>

PacketStreamPanel::PacketStreamPanel() {}

std::string format_timestamp(uint64_t ns) {
    time_t sec = static_cast<time_t>(ns / 1000000000);
    uint32_t ms = static_cast<uint32_t>((ns % 1000000000) / 1000000);
    struct tm tm_info;
#if defined(NEURONSCOPE_WINDOWS) || defined(_WIN32)
    localtime_s(&tm_info, &sec);
#else
    localtime_r(&sec, &tm_info);
#endif
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d", 
             tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, ms);
    return std::string(buf);
}

std::string get_layer_type_name(LayerType type) {
    switch (type) {
        case LAYER_EMBEDDING: return "Embedding";
        case LAYER_ATTN_SELF: return "Attn (Self)";
        case LAYER_MLP:       return "MLP (SwiGLU)";
        case LAYER_NORM:      return "LayerNorm";
        case LAYER_LINEAR:    return "Linear";
        default:              return "Other";
    }
}

void PacketStreamPanel::add_record(const TelemetryRecord& record) {
    std::lock_guard<std::mutex> lock(records_mutex_);
    records_.push_back(record);
    if (records_.size() > 500) {
        records_.erase(records_.begin());
    }
    if (autoscroll_ || selected_packet_id_ == 0) {
        selected_packet_id_ = record.id;
    }
}

std::vector<TelemetryRecord> PacketStreamPanel::get_records() {
    std::lock_guard<std::mutex> lock(records_mutex_);
    return records_;
}

std::optional<TelemetryRecord> PacketStreamPanel::get_selected_record() {
    std::lock_guard<std::mutex> lock(records_mutex_);
    if (records_.empty()) {
        return std::nullopt;
    }

    // Filter visible records
    std::vector<TelemetryRecord> visible;
    for (const auto& rec : records_) {
        if (layer_filter_.empty()) {
            visible.push_back(rec);
        } else {
            std::string rec_name = rec.layer_name;
            if (rec_name == layer_filter_ || (rec_name.rfind(layer_filter_ + ".", 0) == 0)) {
                visible.push_back(rec);
            }
        }
    }

    if (visible.empty()) {
        return std::nullopt;
    }

    for (const auto& rec : visible) {
        if (rec.id == selected_packet_id_) {
            return rec;
        }
    }

    // Fallback to latest visible
    return visible.back();
}

void PacketStreamPanel::clear() {
    std::lock_guard<std::mutex> lock(records_mutex_);
    records_.clear();
    selected_packet_id_ = 0;
}

void PacketStreamPanel::set_layer_filter(const std::string& layer_name) {
    std::lock_guard<std::mutex> lock(records_mutex_);
    layer_filter_ = layer_name;
    autoscroll_ = true;  // follow new filtered stream
    if (!records_.empty()) {
        selected_packet_id_ = records_.back().id;
    }
}

void PacketStreamPanel::clear_layer_filter() {
    std::lock_guard<std::mutex> lock(records_mutex_);
    layer_filter_ = "";
    autoscroll_ = true;
    if (!records_.empty()) {
        selected_packet_id_ = records_.back().id;
    }
}

ftxui::Element PacketStreamPanel::Render() {
    using namespace ftxui;

    std::lock_guard<std::mutex> lock(records_mutex_);

    // Headers
    auto header = hbox({
        text(" ID   ") | bold | color(Theme::TextWhite),
        separator(),
        text(" TIMESTAMP    ") | bold | color(Theme::TextWhite),
        separator(),
        text(" LAYER TYPE   ") | bold | color(Theme::TextWhite),
        separator(),
        text(" COMPUTE DEVICE   ") | bold | color(Theme::TextWhite),
        separator(),
        text(" LATENCY  ") | bold | color(Theme::TextWhite)
    });

    if (records_.empty()) {
        return vbox({
            text("┌── 2. LIVE PACKET STREAM ────────────────────────────────────────────────────────┐") | bold | color(Focused() ? Theme::BorderActive : Theme::BorderMuted),
            header,
            separator(),
            text("No packets received yet. Run python probe or mock generator.") | color(Theme::TextDim) | center | flex,
        }) | Theme::StyledBorder(Focused());
    }

    // Build a filtered view
    std::vector<int> visible_indices;
    for (int i = 0; i < static_cast<int>(records_.size()); ++i) {
        if (layer_filter_.empty()) {
            visible_indices.push_back(i);
        } else {
            std::string rec_name = records_[i].layer_name;
            if (rec_name == layer_filter_ || (rec_name.rfind(layer_filter_ + ".", 0) == 0)) {
                visible_indices.push_back(i);
            }
        }
    }

    std::string footer_txt;
    if (!layer_filter_.empty()) {
        footer_txt = "[Filter: " + layer_filter_ + "]  ";
        footer_txt += autoscroll_ ? "[Autoscroll: ON | Enter to Freeze]" : "[Autoscroll: OFF | Enter to Resume]";
        footer_txt += "  [Backspace: Clear Filter]";
    } else {
        footer_txt = autoscroll_ ? "[Autoscroll: ON | Enter to Freeze | Arrow keys to scroll]" : "[Autoscroll: OFF | Enter to Resume]";
    }

    if (visible_indices.empty()) {
        std::string msg = layer_filter_.empty()
            ? "No packets received yet."
            : "No packets for layer: " + layer_filter_;
        return vbox({
            text("┌── 2. LIVE PACKET STREAM ────────────────────────────────────────────────────────┐") | bold | color(Focused() ? Theme::BorderActive : Theme::BorderMuted),
            header,
            separator(),
            text(msg) | color(Theme::TextDim) | center | flex,
            text(footer_txt) | color(layer_filter_.empty() ? Theme::TextDim : Theme::AccentWarning) | size(HEIGHT, EQUAL, 1)
        }) | Theme::StyledBorder(Focused());
    }

    // Find currently selected index in the visible list
    int sel_vi = -1;
    for (int vi = 0; vi < static_cast<int>(visible_indices.size()); ++vi) {
        if (records_[visible_indices[vi]].id == selected_packet_id_) {
            sel_vi = vi;
            break;
        }
    }

    // If not found (e.g. filtered out or erased), or if autoscroll is ON, snap to latest
    if (sel_vi == -1 || autoscroll_) {
        sel_vi = static_cast<int>(visible_indices.size()) - 1;
        selected_packet_id_ = records_[visible_indices[sel_vi]].id;
    }

    Elements rows;
    for (int vi = 0; vi < static_cast<int>(visible_indices.size()); ++vi) {
        int i = visible_indices[vi];
        const auto& rec = records_[i];

        bool is_selected = (vi == sel_vi);
        
        std::string id_str = std::to_string(rec.id);
        std::string ts_str = format_timestamp(rec.timestamp_ns);
        std::string type_str = get_layer_type_name(rec.layer_type);
        std::string dev_str = rec.device;
        
        std::stringstream ss;
        ss << std::fixed << std::setprecision(3) << (rec.latency_us / 1000.0f) << " ms";
        std::string lat_str = ss.str();

        // Color coding device
        Color dev_color = Theme::TextWhite;
        if (rec.flags & FLAG_CPU_FALLBACK) {
            dev_color = Theme::AccentError;
        } else if (std::string(rec.device).find("CUDA") != std::string::npos) {
            dev_color = Theme::AccentGPU;
        } else {
            dev_color = Theme::AccentCPU;
        }

        auto row = hbox({
            text(id_str) | size(WIDTH, EQUAL, 6),
            separator(),
            text(ts_str) | size(WIDTH, EQUAL, 14),
            separator(),
            text(type_str) | size(WIDTH, EQUAL, 14),
            separator(),
            text(dev_str) | color(dev_color) | size(WIDTH, EQUAL, 18),
            separator(),
            text(lat_str) | size(WIDTH, EQUAL, 10)
        });

        if (is_selected) {
            row = row | focus;
            if (Focused()) {
                row = row | bgcolor(Color::RGB(30, 41, 59)) | bold | color(Theme::TextWhite);
            } else {
                row = row | bold | color(Theme::TextWhite);
            }
        } else {
            row = row | color(Theme::TextDim);
        }

        rows.push_back(row);
    }

    return vbox({
        text("┌── 2. LIVE PACKET STREAM ────────────────────────────────────────────────────────┐") | bold | color(Focused() ? Theme::BorderActive : Theme::BorderMuted),
        header,
        separator(),
        vbox(std::move(rows)) | frame | flex | vscroll_indicator,
        text(footer_txt) | color(layer_filter_.empty() ? Theme::TextDim : Theme::AccentWarning) | size(HEIGHT, EQUAL, 1)
    }) | Theme::StyledBorder(Focused());
}

bool PacketStreamPanel::OnEvent(ftxui::Event event) {
    std::lock_guard<std::mutex> lock(records_mutex_);

    // Backspace clears the layer filter
    if (event == ftxui::Event::Backspace) {
        if (!layer_filter_.empty()) {
            layer_filter_ = "";
            autoscroll_ = true;
            if (!records_.empty()) {
                selected_packet_id_ = records_.back().id;
            }
            return true;
        }
    }

    // Build visible index list for navigation
    std::vector<int> visible_indices;
    for (int i = 0; i < static_cast<int>(records_.size()); ++i) {
        if (layer_filter_.empty()) {
            visible_indices.push_back(i);
        } else {
            std::string rec_name = records_[i].layer_name;
            if (rec_name == layer_filter_ || (rec_name.rfind(layer_filter_ + ".", 0) == 0)) {
                visible_indices.push_back(i);
            }
        }
    }
    if (visible_indices.empty()) return false;

    // Find currently selected index in the visible list
    int sel_vi = -1;
    for (int vi = 0; vi < static_cast<int>(visible_indices.size()); ++vi) {
        if (records_[visible_indices[vi]].id == selected_packet_id_) {
            sel_vi = vi;
            break;
        }
    }
    if (sel_vi == -1) {
        sel_vi = static_cast<int>(visible_indices.size()) - 1;
    }

    if (event == ftxui::Event::Character('j') || event == ftxui::Event::ArrowDown) {
        autoscroll_ = false;
        sel_vi = (sel_vi + 1) % static_cast<int>(visible_indices.size());
        selected_packet_id_ = records_[visible_indices[sel_vi]].id;
        return true;
    }
    if (event == ftxui::Event::Character('k') || event == ftxui::Event::ArrowUp) {
        autoscroll_ = false;
        sel_vi = (sel_vi - 1 + static_cast<int>(visible_indices.size())) % static_cast<int>(visible_indices.size());
        selected_packet_id_ = records_[visible_indices[sel_vi]].id;
        return true;
    }
    if (event == ftxui::Event::Character('a')) {
        autoscroll_ = true;
        selected_packet_id_ = records_[visible_indices.back()].id;
        return true;
    }
    if (event == ftxui::Event::Return) {
        autoscroll_ = !autoscroll_;
        if (autoscroll_) {
            selected_packet_id_ = records_[visible_indices.back()].id;
        } else {
            selected_packet_id_ = records_[visible_indices[sel_vi]].id;
        }
        return true;
    }
    return false;
}
