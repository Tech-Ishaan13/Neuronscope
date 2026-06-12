#include "tui/panels/anomaly_panel.h"
#include "tui/theme.h"
#include "ftxui/component/event.hpp"

// We reuse the format_timestamp function from PacketStreamPanel
extern std::string format_timestamp(uint64_t ns);

AnomalyPanel::AnomalyPanel(AnomalyDetector* detector) : detector_(detector) {}

ftxui::Element AnomalyPanel::Render() {
    using namespace ftxui;

    const auto& ledger = detector_->get_ledger();

    // Filter ledger entries by the layer target
    std::vector<AnomalyEntry> visible_ledger;
    for (const auto& entry : ledger) {
        if (layer_filter_.empty()) {
            visible_ledger.push_back(entry);
        } else {
            std::string entry_name = entry.layer_name;
            if (entry_name == layer_filter_ || (entry_name.rfind(layer_filter_ + ".", 0) == 0)) {
                visible_ledger.push_back(entry);
            }
        }
    }

    if (visible_ledger.empty()) {
        std::string empty_msg = layer_filter_.empty()
            ? "No anomalies detected. Model health is nominal."
            : "No anomalies detected for layer: " + layer_filter_;
        return vbox({
            hbox({
                text(" 5. ANOMALY LEDGER") | bold | color(Focused() ? Theme::BorderActive : Theme::TextWhite),
                text(" [a: Autoscroll | j/k: Scroll]") | color(Theme::TextDim),
                filler()
            }),
            separator(),
            text(empty_msg) | color(Theme::AccentGPU) | bold | center | flex
        }) | Theme::StyledBorder(Focused());
    }

    // Keep selection in bounds
    if (selected_index_ < 0) selected_index_ = 0;
    if (selected_index_ >= static_cast<int>(visible_ledger.size())) {
        selected_index_ = static_cast<int>(visible_ledger.size()) - 1;
    }

    // If autoscroll is ON, snap selection to the latest visible anomaly
    if (autoscroll_) {
        selected_index_ = static_cast<int>(visible_ledger.size()) - 1;
    }

    Elements items;
    for (int i = 0; i < static_cast<int>(visible_ledger.size()); ++i) {
        const auto& entry = visible_ledger[i];
        
        bool is_selected = (i == selected_index_) && Focused();

        std::string ts = format_timestamp(entry.timestamp_ns);
        
        std::string icon = Theme::AnomalyInfo;
        Color severity_color = Theme::TextDim;
        
        if (entry.severity == "CRITICAL") {
            icon = Theme::AnomalyErr;
            severity_color = Theme::AccentError;
        } else if (entry.severity == "WARNING") {
            icon = Theme::AnomalyWarn;
            severity_color = Theme::AccentWarning;
        } else if (entry.severity == "INFO") {
            icon = Theme::AnomalyInfo;
            severity_color = Theme::TextWhite;
        }

        auto item = hbox({
            text(ts + " ") | color(Theme::TextDim),
            text(icon) | color(severity_color) | bold,
            text(entry.message) | color(severity_color),
            text(" [" + entry.layer_name + "]") | color(Theme::TextDim) | dim
        });

        if (is_selected) {
            item = item | bgcolor(Color::RGB(30, 41, 59)) | bold | focus;
        }

        items.push_back(item);
    }

    std::string footer_txt = autoscroll_ ? "[Autoscroll: ON]" : "[Autoscroll: OFF]";
    if (!layer_filter_.empty()) {
        footer_txt += " [Filtered]";
    }

    return vbox({
        hbox({
            text(" 5. ANOMALY LEDGER") | bold | color(Focused() ? Theme::BorderActive : Theme::TextWhite),
            text(" [a: Autoscroll | j/k: Scroll]") | color(Theme::TextDim),
            filler()
        }),
        separator(),
        vbox(std::move(items)) | frame | vscroll_indicator | flex,
        separator(),
        text(" " + footer_txt) | color(Theme::TextDim)
    }) | Theme::StyledBorder(Focused());
}

bool AnomalyPanel::OnEvent(ftxui::Event event) {
    // Re-build visible ledger to compute size for scrolling bounds
    const auto& ledger = detector_->get_ledger();
    std::vector<AnomalyEntry> visible_ledger;
    for (const auto& entry : ledger) {
        if (layer_filter_.empty()) {
            visible_ledger.push_back(entry);
        } else {
            std::string entry_name = entry.layer_name;
            if (entry_name == layer_filter_ || (entry_name.rfind(layer_filter_ + ".", 0) == 0)) {
                visible_ledger.push_back(entry);
            }
        }
    }

    if (visible_ledger.empty()) return false;

    if (event == ftxui::Event::Character('j') || event == ftxui::Event::ArrowDown) {
        autoscroll_ = false;
        selected_index_ = (selected_index_ + 1) % visible_ledger.size();
        return true;
    }
    if (event == ftxui::Event::Character('k') || event == ftxui::Event::ArrowUp) {
        autoscroll_ = false;
        selected_index_ = (selected_index_ - 1 + visible_ledger.size()) % visible_ledger.size();
        return true;
    }
    if (event == ftxui::Event::Character('a')) {
        autoscroll_ = true;
        selected_index_ = static_cast<int>(visible_ledger.size()) - 1;
        return true;
    }
    return false;
}
