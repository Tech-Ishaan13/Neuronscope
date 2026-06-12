#include "tui/app.h"
#include "tui/theme.h"
#include "tui/keybindings.h"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include <iostream>
#include <cstring>
#include <cmath>

App::App() : model_("model") {
    buffer_layout_ = std::make_unique<typename SPSCRingBuffer<TelemetryRecord, 1024>::Layout>();
    ring_buffer_ = std::make_unique<SPSCRingBuffer<TelemetryRecord, 1024>>(buffer_layout_.get());
}

App::~App() {
    stop();
}

bool App::init() {
    ring_buffer_->init_metadata();

    // 1. Build and load topology tree
    topology_.add_layer("model.embed_tokens", "Embedding");
    for (int i = 0; i < 2; ++i) {
        std::string prefix = "model.layers." + std::to_string(i);
        topology_.add_layer(prefix + ".input_layernorm", "RMSNorm");
        topology_.add_layer(prefix + ".self_attn", "Attention");
        topology_.add_layer(prefix + ".post_attention_layernorm", "RMSNorm");
        topology_.add_layer(prefix + ".mlp", "MLP");
    }
    topology_.add_layer("model.norm", "RMSNorm");

    // Default target
    topology_.set_active_target("model.layers.0.self_attn");

    // 2. Instantiate TUI Panels
    // Pass nullptr for control pipe as it is local execution
    topology_panel_ = std::make_shared<TopologyPanel>(&topology_, nullptr);
    packet_stream_panel_ = std::make_shared<PacketStreamPanel>();
    attention_panel_ = std::make_shared<AttentionPanel>();
    metrics_panel_ = std::make_shared<MetricsPanel>();
    anomaly_panel_ = std::make_shared<AnomalyPanel>(&anomaly_detector_);

    // 3. Register hooks non-invasively on submodules
    setup_model_hooks();

    // Wire Panel 1 -> Panel 2: pressing Enter in topology filters the packet stream
    topology_panel_->set_on_target_selected([this](const std::string& path, bool active) {
        if (active && !path.empty()) {
            packet_stream_panel_->set_layer_filter(path);
            anomaly_panel_->set_layer_filter(path);
        } else {
            packet_stream_panel_->clear_layer_filter();
            anomaly_panel_->clear_layer_filter();
        }
    });

    // Apply default active target filter matching startup active target
    std::string default_tgt = topology_.get_active_target();
    if (!default_tgt.empty()) {
        packet_stream_panel_->set_layer_filter(default_tgt);
        anomaly_panel_->set_layer_filter(default_tgt);
    }

    // 4. Set up interactive container layout
    setup_layout();

    return true;
}

void App::setup_model_hooks() {
    auto submodules = model_.get_all_submodules();

    for (auto& mod : submodules) {
        // Pre-hook for telemetry latency capture
        mod->register_forward_pre_hook([this](Module* m, const Tensor& input) {
            uint64_t start_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count();
            start_times_[m->name] = start_ns;
        });

        // Post-hook for activation metrics capture
        mod->register_forward_hook([this](Module* m, const Tensor& input, const Tensor& output) {
            uint64_t end_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count();
            uint64_t start_ns = start_times_[m->name];
            float latency_us = static_cast<float>(end_ns - start_ns) / 1000.0f;

            TelemetryRecord rec;
            std::memset(&rec, 0, sizeof(rec));

            rec.id = packet_id_++;
            rec.timestamp_ns = end_ns;

            // Map LayerType enum
            if (m->type == "Embedding") rec.layer_type = LAYER_EMBEDDING;
            else if (m->type == "Attention") rec.layer_type = LAYER_ATTN_SELF;
            else if (m->type == "MLP") rec.layer_type = LAYER_MLP;
            else if (m->type == "RMSNorm") rec.layer_type = LAYER_NORM;
            else if (m->type == "Linear") rec.layer_type = LAYER_LINEAR;
            else rec.layer_type = LAYER_OTHER;

            // Parse layer index from name (e.g. model.layers.0.self_attn -> 0)
            int layer_idx = 0;
            auto layers_pos = m->name.find("layers.");
            if (layers_pos != std::string::npos) {
                auto next_dot = m->name.find(".", layers_pos + 7);
                if (next_dot != std::string::npos) {
                    layer_idx = std::stoi(m->name.substr(layers_pos + 7, next_dot - (layers_pos + 7)));
                }
            }
            rec.layer_index = layer_idx;

            std::strncpy(rec.layer_name, m->name.c_str(), sizeof(rec.layer_name) - 1);

            // CPU Fallback logic
            if (m->name.find("norm") != std::string::npos && m->name.find("input") == std::string::npos && m->name.find("post") == std::string::npos) {
                std::strncpy(rec.device, "CPU (Fallback)", sizeof(rec.device) - 1);
                rec.flags |= FLAG_CPU_FALLBACK;
            } else {
                std::strncpy(rec.device, "CUDA [GPU 0]", sizeof(rec.device) - 1);
            }

            // Dtype & shape
            std::strncpy(rec.dtype, output.dtype.c_str(), sizeof(rec.dtype) - 1);
            for (size_t i = 0; i < std::min(static_cast<size_t>(4), output.shape.size()); ++i) {
                rec.shape[i] = output.shape[i];
            }

            // Compute statistics
            float sum = 0.0f;
            float sum_sq = 0.0f;
            float min_val = 99999.0f;
            float max_val = -99999.0f;
            size_t zero_count = 0;

            for (float val : output.data) {
                sum += val;
                sum_sq += val * val;
                if (val < min_val) min_val = val;
                if (val > max_val) max_val = val;
                if (val == 0.0f) zero_count++;
            }

            size_t n = output.data.size();
            rec.mean = n > 0 ? sum / n : 0.0f;
            float var = n > 0 ? (sum_sq / n) - (rec.mean * rec.mean) : 0.0f;
            rec.std_dev = std::sqrt(std::max(0.0f, var));
            rec.min_val = min_val;
            rec.max_val = max_val;
            rec.sparsity = n > 0 ? static_cast<float>(zero_count) / n : 0.0f;
            rec.latency_us = latency_us;
            rec.memory_bytes = (rec.flags & FLAG_CPU_FALLBACK) ? 0 : 4210964480;

            // Injected anomalies
            if (m->type == "Embedding" && (rand() % 100 < 2)) {
                rec.flags |= FLAG_HAS_NAN;
            }
            if (rec.max_val > 6.0f) {
                rec.flags |= FLAG_OUTLIER;
            }

            // Attention matrix mapping
            if (rec.layer_type == LAYER_ATTN_SELF) {
                rec.flags |= FLAG_HAS_ATTN;
                rec.attn_num_heads = 32;
                rec.attn_seq_len = 8;
                for (int y = 0; y < 8; ++y) {
                    for (int x = 0; x < 8; ++x) {
                        float dist = std::abs(x - y);
                        float w = std::exp(-dist) / 2.0f;
                        if (x == y) w = 0.8f;
                        rec.attn_weights[y * 64 + x] = w;
                    }
                }
            }

            ring_buffer_->try_push(rec);
        });
    }
}

void App::setup_layout() {
    using namespace ftxui;

    auto left_col = Container::Vertical({
        topology_panel_,
        packet_stream_panel_
    });

    auto top_row = Container::Horizontal({
        left_col,
        attention_panel_
    });

    auto bottom_row = Container::Horizontal({
        metrics_panel_,
        anomaly_panel_
    });

    main_container_ = Container::Vertical({
        top_row,
        bottom_row
    });

    main_container_ = CatchEvent(main_container_, [&](Event event) {
        if (event == Event::Character('q') || event == Event::Character('Q')) {
            screen_->ExitLoopClosure()();
            return true;
        }

        if (event == Event::Escape) {
            if (attention_panel_->is_fullscreen()) {
                attention_panel_->toggle_fullscreen();
                return true;
            }
            screen_->ExitLoopClosure()();
            return true;
        }

        if (event == Event::Character('f') || event == Event::Character('F')) {
            attention_panel_->toggle_fullscreen();
            if (attention_panel_->is_fullscreen()) {
                attention_panel_->TakeFocus();
            }
            return true;
        }

        if (event == Event::Character('1')) {
            topology_panel_->TakeFocus();
            return true;
        }
        if (event == Event::Character('2')) {
            packet_stream_panel_->TakeFocus();
            return true;
        }
        if (event == Event::Character('3')) {
            attention_panel_->TakeFocus();
            return true;
        }
        if (event == Event::Character('4')) {
            metrics_panel_->TakeFocus();
            return true;
        }
        if (event == Event::Character('5')) {
            anomaly_panel_->TakeFocus();
            return true;
        }

        return false;
    });

    main_renderer_ = Renderer(main_container_, [&] {
        auto sel_rec_opt = packet_stream_panel_->get_selected_record();
        if (sel_rec_opt.has_value()) {
            metrics_panel_->update_record(sel_rec_opt.value());
            attention_panel_->update_record(sel_rec_opt.value());
        } else {
            metrics_panel_->clear();
            attention_panel_->clear();
        }

        auto header = hbox({
            text("  NeuronScope Telemetry Platform  ") | bold | bgcolor(Color::RGB(30, 41, 59)) | color(Theme::BorderActive),
            separator(),
            text(" Model: " + model_name_) | bold | color(Theme::TextWhite),
            separator(),
            text(" Status: " + status_msg_) | color(Theme::TextDim) | flex,
            separator(),
            text(" 🟩 SYSTEM ACTIVE (LOCAL ENGINE) ") | bold | color(Theme::TextWhite)
        });

        auto footer = vbox({
            separator(),
            text(Keybindings::GlobalHelp) | color(Theme::TextDim) | center
        });

        if (attention_panel_->is_fullscreen()) {
            return vbox({
                header,
                separator(),
                attention_panel_->Render() | flex,
                footer
            });
        }

        // New layout:
        //  ┌─────────────┬─────────────────────────────┐
        //  │  1. Topology│                              │
        //  │   (top-left)│   3. Attention Matrix        │
        //  ├─────────────│      (full right column)     │
        //  │  2. Packets │                              │
        //  │ (bot-left)  │                              │
        //  ├─────────────┴──────────┬────────────────── ┤
        //  │   4. Metrics           │  5. Anomaly Ledger│
        //  └────────────────────────┴───────────────────┘

        auto left_col = vbox({
            topology_panel_->Render() | size(HEIGHT, EQUAL, 10),
            separator(),
            packet_stream_panel_->Render() | flex
        }) | size(WIDTH, EQUAL, 75);

        auto right_col = attention_panel_->Render() | flex;

        auto bottom_row = hbox({
            metrics_panel_->Render() | size(WIDTH, EQUAL, 75),
            separator(),
            anomaly_panel_->Render() | flex
        }) | size(HEIGHT, EQUAL, 8);

        auto workspace = vbox({
            hbox({
                left_col,
                separator(),
                right_col
            }) | flex,
            separator(),
            bottom_row
        });

        return vbox({
            header,
            separator(),
            workspace | flex,
            footer
        }) | bgcolor(Color::RGB(30, 30, 30));
    });
}

void App::model_inference_loop() {
    Tensor input_tokens;
    input_tokens.shape = { 1, 8 }; // simulated batch_size = 1, seq_len = 8
    input_tokens.data.resize(8);
    for (int i = 0; i < 8; ++i) {
        input_tokens.data[i] = static_cast<float>(rand() % 32000);
    }

    while (running_) {
        // Run forward pass of LlamaModel
        model_.forward(input_tokens);

        // Sleep 1 second before next simulated sequence processing
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void App::process_telemetry_loop() {
    TelemetryRecord record;
    int backoff_ms = 1;

    while (running_) {
        bool has_data = false;

        while (ring_buffer_->try_pop(record)) {
            has_data = true;
            backoff_ms = 1;

            anomaly_detector_.detect(record);
            packet_stream_panel_->add_record(record);

            std::string active_tgt = topology_.get_active_target();
            if (packet_stream_panel_->is_autoscroll()) {
                bool match = active_tgt.empty() || 
                             (record.layer_name == active_tgt) || 
                             (std::string(record.layer_name).rfind(active_tgt + ".", 0) == 0);
                if (match) {
                    metrics_panel_->update_record(record);
                    if (record.flags & FLAG_HAS_ATTN) {
                        attention_panel_->update_record(record);
                    }
                }
            }

            screen_->PostEvent(ftxui::Event::Custom);
        }

        if (!has_data) {
            std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
            backoff_ms = std::min(50, backoff_ms * 2);
        }
    }
}

void App::run() {
    running_ = true;

    // Start background loops
    inference_thread_ = std::thread(&App::model_inference_loop, this);
    telemetry_thread_ = std::thread(&App::process_telemetry_loop, this);

    screen_ = std::make_unique<ftxui::ScreenInteractive>(
        ftxui::ScreenInteractive::Fullscreen()
    );
    screen_->Loop(main_renderer_);
}

void App::stop() {
    if (running_) {
        running_ = false;
        if (inference_thread_.joinable()) {
            inference_thread_.join();
        }
        if (telemetry_thread_.joinable()) {
            telemetry_thread_.join();
        }
    }
}
