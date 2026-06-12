#pragma once
#include "ftxui/component/component.hpp"
#include "neuronscope/telemetry_record.h"

class MetricsPanel : public ftxui::ComponentBase {
public:
    MetricsPanel();

    ftxui::Element Render();
    bool Focusable() const override { return true; }

    void update_record(const TelemetryRecord& record);
    void clear();

private:
    TelemetryRecord current_record_;
    bool has_record_ = false;
};
