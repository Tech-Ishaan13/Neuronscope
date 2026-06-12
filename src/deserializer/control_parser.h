#pragma once
#include <string>
#include <vector>

enum class ControlMessageType {
    TOPOLOGY,
    ACTIVE_TARGET,
    STATUS,
    UNKNOWN
};

struct ControlMessage {
    ControlMessageType type = ControlMessageType::UNKNOWN;
    std::string arg1;
    std::string arg2;
};

class ControlParser {
public:
    static ControlMessage parse(const std::string& raw_msg) {
        ControlMessage msg;
        if (raw_msg.rfind("TOPO:", 0) == 0) {
            msg.type = ControlMessageType::TOPOLOGY;
            size_t first_colon = 4;
            size_t second_colon = raw_msg.find(':', first_colon + 1);
            if (second_colon != std::string::npos) {
                msg.arg1 = raw_msg.substr(first_colon + 1, second_colon - first_colon - 1);
                msg.arg2 = raw_msg.substr(second_colon + 1);
            }
        } else if (raw_msg.rfind("ACTIVE:", 0) == 0) {
            msg.type = ControlMessageType::ACTIVE_TARGET;
            msg.arg1 = raw_msg.substr(7);
        } else if (raw_msg.rfind("STATUS:", 0) == 0) {
            msg.type = ControlMessageType::STATUS;
            msg.arg1 = raw_msg.substr(7);
        }
        return msg;
    }
};
