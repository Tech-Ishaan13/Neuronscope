#pragma once
#include "ftxui/component/component.hpp"
#include "neuronscope/telemetry_record.h"
#include <vector>
#include <string>

class AttentionPanel : public ftxui::ComponentBase {
public:
    AttentionPanel();

    ftxui::Element Render();
    bool OnEvent(ftxui::Event event) override;
    bool Focusable() const override { return true; }

    void update_record(const TelemetryRecord& record);
    void clear();
    bool is_fullscreen() const { return fullscreen_; }
    void toggle_fullscreen() { fullscreen_ = !fullscreen_; }

private:
    TelemetryRecord current_record_;
    bool has_record_ = false;
    
    int head_index_ = 0;
    float contrast_scale_ = 1.0f;
    int viewport_x_ = 0;
    int viewport_y_ = 0;
    bool fullscreen_ = false;
    std::vector<std::string> tokens_; // Simulated or parsed token list

    void generate_tokens(int count);
};
