#pragma once
#include "ftxui/component/component.hpp"
#include "core/model_topology.h"
#include "ipc/control_pipe.h"
#include <functional>
#include <string>

class TopologyPanel : public ftxui::ComponentBase {
public:
    TopologyPanel(ModelTopology* topology, ControlPipe* control_pipe);

    ftxui::Element Render();
    bool OnEvent(ftxui::Event event) override;

    bool Focusable() const override { return true; }

    // Called when user presses Enter on a node. Args: full_path, is_now_active
    void set_on_target_selected(std::function<void(const std::string&, bool)> cb) {
        on_target_selected_ = cb;
    }

private:
    ModelTopology* topology_;
    ControlPipe* control_pipe_;
    int selected_index_ = 0;
    std::function<void(const std::string&, bool)> on_target_selected_;
};
