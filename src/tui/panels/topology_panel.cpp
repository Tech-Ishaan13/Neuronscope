#include "tui/panels/topology_panel.h"
#include "tui/theme.h"
#include "ftxui/component/event.hpp"
#include <sstream>

TopologyPanel::TopologyPanel(ModelTopology* topology, ControlPipe* control_pipe)
    : topology_(topology), control_pipe_(control_pipe) {}

int get_depth(TopologyNode* node) {
    int depth = 0;
    TopologyNode* p = node->parent;
    while (p && p->parent) {
        depth++;
        p = p->parent;
    }
    return depth;
}

ftxui::Element TopologyPanel::Render() {
    using namespace ftxui;

    auto visible_nodes = topology_->get_visible_nodes();
    if (visible_nodes.empty()) {
        return vbox({
            text("No model topology loaded.") | color(Theme::TextDim) | center
        }) | Theme::StyledBorder(Focused());
    }

    // Keep selection in bounds
    if (selected_index_ < 0) selected_index_ = 0;
    if (selected_index_ >= static_cast<int>(visible_nodes.size())) {
        selected_index_ = static_cast<int>(visible_nodes.size()) - 1;
    }

    Elements items;
    for (int i = 0; i < static_cast<int>(visible_nodes.size()); ++i) {
        TopologyNode* node = visible_nodes[i];
        int depth = get_depth(node);

        std::string indent(depth * 2, ' ');
        std::string prefix = node->children.empty() ? Theme::Bullet : 
            (node->expanded ? Theme::TreeExpanded : Theme::TreeCollapsed);

        std::string name = node->name;
        if (!node->type.empty()) {
            name += " (" + node->type + ")";
        }

        Element item_el = text(indent + prefix + name);

        // 1. Selection and focus styling
        bool is_selected = (i == selected_index_);
        if (is_selected) {
            item_el = item_el | focus;
            if (Focused()) {
                item_el = item_el | color(Theme::TextWhite) | bold | bgcolor(Color::RGB(30, 41, 59));
            } else {
                item_el = item_el | color(Theme::TextWhite) | bold;
            }
        } else if (node->is_active_target) {
            item_el = item_el | bold | color(Theme::BorderActive);
        } else {
            item_el = item_el | color(Theme::TextDim);
        }

        // 2. Active target tag
        if (node->is_active_target) {
            item_el = hbox({
                item_el,
                text("  [Active Capture Target]") | color(Theme::AccentGPU) | bold
            });
        }

        items.push_back(item_el);
    }

    return vbox({
        hbox({
            text(" 1. MODEL TOPOLOGY") | bold | color(Focused() ? Theme::BorderActive : Theme::TextWhite),
            text(" [j/k: Nav | Spc: Expand | Enter: Capture]") | color(Theme::TextDim),
            filler()
        }),
        separator(),
        vbox(std::move(items)) | frame | flex | vscroll_indicator
    }) | Theme::StyledBorder(Focused());
}

bool TopologyPanel::OnEvent(ftxui::Event event) {
    auto visible_nodes = topology_->get_visible_nodes();
    if (visible_nodes.empty()) return false;

    if (event == ftxui::Event::Character('j') || event == ftxui::Event::ArrowDown) {
        selected_index_ = (selected_index_ + 1) % visible_nodes.size();
        return true;
    }
    if (event == ftxui::Event::Character('k') || event == ftxui::Event::ArrowUp) {
        selected_index_ = (selected_index_ - 1 + visible_nodes.size()) % visible_nodes.size();
        return true;
    }
    if (event == ftxui::Event::Character(' ') || event == ftxui::Event::Return) {
        TopologyNode* node = visible_nodes[selected_index_];
        if (event == ftxui::Event::Character(' ')) {
            node->expanded = !node->expanded;
        } else { // Return / Enter — toggle active target
            bool was_active = node->is_active_target;
            if (was_active) {
                // Toggle OFF: clear the active target
                topology_->set_active_target("");
                if (on_target_selected_) on_target_selected_("", false);
            } else {
                // Set as active target
                topology_->set_active_target(node->full_path);
                if (on_target_selected_) on_target_selected_(node->full_path, true);
                if (control_pipe_ && control_pipe_->is_connected()) {
                    control_pipe_->write_msg("ACTIVE:" + node->full_path);
                }
            }
        }
        return true;
    }
    return false;
}
