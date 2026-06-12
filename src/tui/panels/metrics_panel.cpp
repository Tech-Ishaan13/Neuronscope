#include "tui/panels/metrics_panel.h"
#include "tui/theme.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

MetricsPanel::MetricsPanel() {}

void MetricsPanel::update_record(const TelemetryRecord& record) {
    current_record_ = record;
    has_record_ = true;
}

void MetricsPanel::clear() {
    has_record_ = false;
}

ftxui::Element make_sparsity_gauge(float sparsity) {
    using namespace ftxui;
    int filled = static_cast<int>(sparsity * 10.0f);
    std::string gauge_str = "";
    for (int i = 0; i < 10; ++i) {
        if (i < filled) {
            if (i < 7) {
                gauge_str += "🟩";
            } else {
                gauge_str += "🟨";
            }
        } else {
            gauge_str += "⬜";
        }
    }
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << (sparsity * 100.0f) << "%";
    return hbox({
        text(gauge_str),
        text(" " + ss.str()) | bold | color(Theme::TextWhite)
    });
}

ftxui::Element MetricsPanel::Render() {
    using namespace ftxui;

    if (!has_record_) {
        return vbox({
            text("┌── 4. RUNTIME METRICS INSPECTOR ──────────────────────┐") | bold | color(Focused() ? Theme::BorderActive : Theme::BorderMuted),
            text("Select a packet in stream to inspect runtime metrics.") | color(Theme::TextDim) | center | flex
        }) | Theme::StyledBorder(Focused());
    }

    const auto& rec = current_record_;

    // Shape formulation: e.g. [1, 32, 4096]
    std::stringstream shape_ss;
    shape_ss << "[";
    bool first = true;
    for (int i = 0; i < 4; ++i) {
        if (rec.shape[i] > 0) {
            if (!first) shape_ss << ", ";
            shape_ss << rec.shape[i];
            first = false;
        }
    }
    shape_ss << "]";

    std::stringstream lat_ss;
    lat_ss << std::fixed << std::setprecision(3) << (rec.latency_us / 1000.0f) << " ms";

    // Latency status check
    Element latency_status = text("Within Normal Bounds") | color(Theme::AccentGPU) | bold;
    if (rec.latency_us > 15000.0f) {
        latency_status = text("Critical Spike Detected") | color(Theme::AccentError) | bold;
    } else if (rec.latency_us > 5000.0f) {
        latency_status = text("Elevated Latency") | color(Theme::AccentWarning) | bold;
    }

    return vbox({
        text("┌── 4. RUNTIME METRICS INSPECTOR ──────────────────────┐") | bold | color(Focused() ? Theme::BorderActive : Theme::BorderMuted),
        vbox({
            hbox({ text(" Tensor Shape : ") | color(Theme::TextDim), text(shape_ss.str()) | bold | color(Theme::TextWhite), text("   Dtype: ") | color(Theme::TextDim), text(rec.dtype) | bold | color(Theme::TextWhite) }),
            hbox({ text(" Sparsity Rate: ") | color(Theme::TextDim), make_sparsity_gauge(rec.sparsity) }),
            hbox({ text(" Latency Delta: ") | color(Theme::TextDim), text(lat_ss.str()) | bold | color(Theme::TextWhite), text(" ("), latency_status, text(")") }),
            separator(),
            hbox({ text(" Mean Activation : ") | color(Theme::TextDim), text(std::to_string(rec.mean)) | color(Theme::TextWhite) }),
            hbox({ text(" Range (Min/Max) : ") | color(Theme::TextDim), text(std::to_string(rec.min_val) + " / " + std::to_string(rec.max_val)) | color(Theme::TextWhite) }),
            hbox({ text(" Allocated VRAM  : ") | color(Theme::TextDim), text(std::to_string(rec.memory_bytes / (1024 * 1024)) + " MB") | color(Theme::TextWhite) }),
        }) | flex | vscroll_indicator,
    }) | Theme::StyledBorder(Focused());
}
