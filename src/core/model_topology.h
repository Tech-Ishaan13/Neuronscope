#pragma once
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <algorithm>

struct TopologyNode {
    std::string name;
    std::string full_path;
    std::string type; // e.g. "Attn", "MLP", "RMSNorm", etc.
    bool expanded = true;
    bool is_active_target = false;
    std::vector<std::shared_ptr<TopologyNode>> children;
    TopologyNode* parent = nullptr;
};

class ModelTopology {
public:
    ModelTopology() {
        root_ = std::make_shared<TopologyNode>();
        root_->name = "Model Root";
        root_->full_path = "";
        root_->expanded = true;
    }

    void add_layer(const std::string& path, const std::string& type) {
        std::stringstream ss(path);
        std::string part;
        TopologyNode* current = root_.get();
        std::string current_path = "";

        while (std::getline(ss, part, '.')) {
            if (current_path.empty()) {
                current_path = part;
            } else {
                current_path += "." + part;
            }

            auto it = std::find_if(current->children.begin(), current->children.end(),
                [&part](const std::shared_ptr<TopologyNode>& node) {
                    return node->name == part;
                });

            if (it == current->children.end()) {
                auto new_node = std::make_shared<TopologyNode>();
                new_node->name = part;
                new_node->full_path = current_path;
                new_node->parent = current;
                current->children.push_back(new_node);
                current = new_node.get();
            } else {
                current = it->get();
            }
        }
        current->type = type;
    }

    std::shared_ptr<TopologyNode> get_root() const {
        return root_;
    }

    std::vector<TopologyNode*> get_visible_nodes() {
        std::vector<TopologyNode*> visible;
        for (const auto& child : root_->children) {
            get_visible_nodes_recurse(child.get(), visible);
        }
        return visible;
    }

    void set_active_target(const std::string& full_path) {
        active_target_path_ = full_path;
        clear_active_recurse(root_.get());
        set_active_recurse(root_.get(), full_path);
    }

    std::string get_active_target() const {
        return active_target_path_;
    }

    void clear() {
        root_->children.clear();
        active_target_path_ = "";
    }

private:
    void get_visible_nodes_recurse(TopologyNode* node, std::vector<TopologyNode*>& list) {
        list.push_back(node);
        if (node->expanded) {
            for (const auto& child : node->children) {
                get_visible_nodes_recurse(child.get(), list);
            }
        }
    }

    void clear_active_recurse(TopologyNode* node) {
        node->is_active_target = false;
        for (auto& child : node->children) {
            clear_active_recurse(child.get());
        }
    }

    bool set_active_recurse(TopologyNode* node, const std::string& full_path) {
        if (node->full_path == full_path) {
            node->is_active_target = true;
            return true;
        }
        for (auto& child : node->children) {
            if (set_active_recurse(child.get(), full_path)) {
                return true;
            }
        }
        return false;
    }

    std::shared_ptr<TopologyNode> root_;
    std::string active_target_path_;
};
