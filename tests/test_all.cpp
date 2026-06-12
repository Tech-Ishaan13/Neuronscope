#include "core/ring_buffer.h"
#include "core/model_topology.h"
#include "core/anomaly_detector.h"
#include "neuronscope/telemetry_record.h"
#include <iostream>
#include <cassert>
#include <cstring>

#define TEST_ASSERT(cond, msg) \
    if (!(cond)) { \
        std::cerr << "  [FAIL] " << msg << "\n"; \
        return false; \
    } else { \
        std::cout << "  [PASS] " << msg << "\n"; \
    }

bool test_ring_buffer() {
    std::cout << "Running Ring Buffer Tests...\n";

    using BufferType = SPSCRingBuffer<int, 4>;
    BufferType::Layout layout;
    BufferType ring(&layout);
    ring.init_metadata();

    TEST_ASSERT(ring.empty(), "Buffer starts empty");
    TEST_ASSERT(ring.size() == 0, "Initial size is 0");

    int item = 0;
    TEST_ASSERT(!ring.try_pop(item), "Pop on empty returns false");

    // Push 3 elements
    TEST_ASSERT(ring.try_push(10), "Push 10");
    TEST_ASSERT(ring.try_push(20), "Push 20");
    std::cout << "  Size: " << ring.size() << "\n";
    TEST_ASSERT(ring.try_push(30), "Push 30");
    TEST_ASSERT(ring.size() == 3, "Size is 3");

    // Pop 1 element
    TEST_ASSERT(ring.try_pop(item) && item == 10, "Pop element is 10");
    TEST_ASSERT(ring.size() == 2, "Size is 2");

    // Push 2 more (total pushed 5, popped 1, size = 4)
    TEST_ASSERT(ring.try_push(40), "Push 40");
    TEST_ASSERT(ring.try_push(50), "Push 50");
    TEST_ASSERT(ring.size() == 4, "Buffer size matches capacity (4)");

    // Push one more (60) to force overwrite of oldest remaining (20)
    TEST_ASSERT(ring.try_push(60), "Push 60 (overflow overwrite)");
    TEST_ASSERT(ring.size() == 4, "Size remains 4 after overflow");

    // Pop all elements to verify correct FIFO ordering
    TEST_ASSERT(ring.try_pop(item) && item == 30, "Pop element is 30 (20 was overwritten)");
    TEST_ASSERT(ring.try_pop(item) && item == 40, "Pop element is 40");
    TEST_ASSERT(ring.try_pop(item) && item == 50, "Pop element is 50");
    TEST_ASSERT(ring.try_pop(item) && item == 60, "Pop element is 60");
    TEST_ASSERT(ring.empty(), "Buffer is empty again");

    return true;
}

bool test_model_topology() {
    std::cout << "Running Model Topology Tests...\n";

    ModelTopology topo;
    topo.add_layer("model.embed_tokens", "Embedding");
    topo.add_layer("model.layers.0.self_attn", "Attn");
    topo.add_layer("model.layers.0.mlp", "MLP");

    auto visible = topo.get_visible_nodes();
    std::cout << "  Visible nodes count: " << visible.size() << "\n";
    TEST_ASSERT(visible.size() == 6, "Discovered nodes count (model, model.embed_tokens, model.layers, model.layers.0, model.layers.0.self_attn, model.layers.0.mlp)");

    topo.set_active_target("model.layers.0.self_attn");
    TEST_ASSERT(topo.get_active_target() == "model.layers.0.self_attn", "Active target set correctly");

    return true;
}

bool test_anomaly_detector() {
    std::cout << "Running Anomaly Detector Tests...\n";

    AnomalyDetector detector;

    TelemetryRecord rec;
    std::memset(&rec, 0, sizeof(rec));
    std::strcpy(rec.layer_name, "test_layer");
    rec.timestamp_ns = 1000000;
    rec.max_val = 2.5f;
    rec.sparsity = 0.2f;
    rec.latency_us = 1000.0f;

    auto list = detector.detect(rec);
    TEST_ASSERT(list.empty(), "No anomalies on clean record");

    // Test NaN
    rec.flags = FLAG_HAS_NAN;
    list = detector.detect(rec);
    TEST_ASSERT(list.size() == 1 && list[0].severity == "CRITICAL", "Detects NaN critical anomaly");

    // Test Outlier
    rec.flags = FLAG_NONE;
    rec.max_val = 7.5f;
    list = detector.detect(rec);
    TEST_ASSERT(list.size() == 1 && list[0].severity == "WARNING", "Detects max_val outlier warning");

    // Test CPU Fallback
    rec.max_val = 1.0f;
    rec.flags = FLAG_CPU_FALLBACK;
    list = detector.detect(rec);
    TEST_ASSERT(list.size() == 1 && list[0].severity == "WARNING", "Detects CPU fallback warning");

    return true;
}

int main() {
    bool ok = true;
    ok &= test_ring_buffer();
    std::cout << "\n";
    ok &= test_model_topology();
    std::cout << "\n";
    ok &= test_anomaly_detector();
    std::cout << "\n";

    if (ok) {
        std::cout << "=== ALL TESTS PASSED ===\n";
        return 0;
    } else {
        std::cout << "=== SOME TESTS FAILED ===\n";
        return 1;
    }
}
