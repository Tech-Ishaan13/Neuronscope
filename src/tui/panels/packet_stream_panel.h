#pragma once
#include "ftxui/component/component.hpp"
#include "neuronscope/telemetry_record.h"
#include <vector>
#include <mutex>
#include <string>

class PacketStreamPanel : public ftxui::ComponentBase {
public:
    PacketStreamPanel();

    ftxui::Element Render();
    bool OnEvent(ftxui::Event event) override;
    bool Focusable() const override { return true; }

    void add_record(const TelemetryRecord& record);
    std::vector<TelemetryRecord> get_records();
    std::optional<TelemetryRecord> get_selected_record();
    void clear();
    bool is_autoscroll() const { return autoscroll_; }

    // Layer filter — set by Panel 1 (Topology) via Enter key
    void set_layer_filter(const std::string& layer_name);
    void clear_layer_filter();
    std::string get_layer_filter() const { return layer_filter_; }

private:
    std::vector<TelemetryRecord> records_;
    std::mutex records_mutex_;
    uint32_t selected_packet_id_ = 0;
    bool autoscroll_ = true;
    std::string layer_filter_;  // empty = show all; else show only this layer
};
