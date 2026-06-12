#pragma once
#include "ftxui/component/component.hpp"
#include "core/anomaly_detector.h"
#include <string>

class AnomalyPanel : public ftxui::ComponentBase {
public:
    AnomalyPanel(AnomalyDetector* detector);

    ftxui::Element Render();
    bool OnEvent(ftxui::Event event) override;
    bool Focusable() const override { return true; }

    void add_anomaly(const AnomalyEntry& entry);
    void clear();

    void set_layer_filter(const std::string& layer_name) {
        layer_filter_ = layer_name;
        selected_index_ = 0;
        autoscroll_ = true;
    }

    void clear_layer_filter() {
        layer_filter_ = "";
        selected_index_ = 0;
        autoscroll_ = true;
    }

private:
    AnomalyDetector* detector_;
    int selected_index_ = 0;
    bool autoscroll_ = true;
    std::string layer_filter_;
};
