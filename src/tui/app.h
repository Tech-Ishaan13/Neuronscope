#pragma once
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "core/model_topology.h"
#include "core/anomaly_detector.h"
#include "core/ring_buffer.h"
#include "neuronscope/telemetry_record.h"
#include "model/transformer.h"

#include "tui/panels/topology_panel.h"
#include "tui/panels/packet_stream_panel.h"
#include "tui/panels/attention_panel.h"
#include "tui/panels/metrics_panel.h"
#include "tui/panels/anomaly_panel.h"

#include <thread>
#include <atomic>
#include <memory>

class App {
public:
    App();
    ~App();

    bool init();
    void run();
    void stop();

private:
    void model_inference_loop();
    void process_telemetry_loop();
    void setup_layout();
    void setup_model_hooks();

    std::atomic<bool> running_{false};
    std::string model_name_ = "llama-3-8b (C++ Local Mode)";
    std::string status_msg_ = "Inference thread active.";
    uint64_t packet_id_ = 1;

    // Core Data structures
    ModelTopology topology_;
    AnomalyDetector anomaly_detector_;
    
    // Model Engine
    LlamaModel model_;
    
    // Local telemetry ring buffer
    std::unique_ptr<typename SPSCRingBuffer<TelemetryRecord, 1024>::Layout> buffer_layout_;
    std::unique_ptr<SPSCRingBuffer<TelemetryRecord, 1024>> ring_buffer_;

    // Timing helper
    std::unordered_map<std::string, uint64_t> start_times_;

    // TUI panels
    std::shared_ptr<TopologyPanel> topology_panel_;
    std::shared_ptr<PacketStreamPanel> packet_stream_panel_;
    std::shared_ptr<AttentionPanel> attention_panel_;
    std::shared_ptr<MetricsPanel> metrics_panel_;
    std::shared_ptr<AnomalyPanel> anomaly_panel_;

    ftxui::Component main_container_;
    ftxui::Component main_renderer_;
    std::unique_ptr<ftxui::ScreenInteractive> screen_;

    std::thread inference_thread_;
    std::thread telemetry_thread_;
};
