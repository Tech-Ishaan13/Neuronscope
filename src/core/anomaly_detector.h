#pragma once
#include "neuronscope/telemetry_record.h"
#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <iomanip>
#include <sstream>

struct AnomalyEntry {
    uint64_t timestamp_ns;
    std::string severity; // "INFO", "WARNING", "CRITICAL"
    std::string message;
    std::string layer_name;
};

class AnomalyDetector {
public:
    AnomalyDetector(float outlier_threshold = 6.0f, float sparsity_threshold = 0.95f)
        : outlier_threshold_(outlier_threshold), sparsity_threshold_(sparsity_threshold) {}

    std::vector<AnomalyEntry> detect(const TelemetryRecord& record) {
        std::vector<AnomalyEntry> anomalies;

        // 1. Check for NaNs/Infs
        if (record.flags & FLAG_HAS_NAN) {
            anomalies.push_back({
                record.timestamp_ns,
                "CRITICAL",
                "NaN value detected in activations",
                record.layer_name
            });
        }
        if (record.flags & FLAG_HAS_INF) {
            anomalies.push_back({
                record.timestamp_ns,
                "CRITICAL",
                "Inf value detected in activations",
                record.layer_name
            });
        }

        // 2. Check for CPU Fallback
        if (record.flags & FLAG_CPU_FALLBACK) {
            anomalies.push_back({
                record.timestamp_ns,
                "WARNING",
                "CUDA OOM Fallback: processing on CPU Host Memory",
                record.layer_name
            });
        }

        // 3. Outlier check
        if (record.max_val > outlier_threshold_) {
            std::stringstream ss;
            ss << "Outlier Feature: Max value (" << std::fixed << std::setprecision(3) << record.max_val 
               << ") exceeds threshold (" << outlier_threshold_ << ")";
            anomalies.push_back({
                record.timestamp_ns,
                "WARNING",
                ss.str(),
                record.layer_name
            });
        }

        // 4. Sparsity check
        if (record.sparsity > sparsity_threshold_) {
            std::stringstream ss;
            ss << "High Sparsity Alert: " << std::fixed << std::setprecision(1) << (record.sparsity * 100.0f) 
               << "% of activations are zero";
            anomalies.push_back({
                record.timestamp_ns,
                "INFO",
                ss.str(),
                record.layer_name
            });
        }

        // 5. Latency spike check (if we have rolling average)
        float avg_latency = rolling_avg_latency_[record.layer_name];
        if (avg_latency > 0.0f && record.latency_us > 3.0f * avg_latency) {
            std::stringstream ss;
            ss << "Latency Spike: " << std::fixed << std::setprecision(2) << (record.latency_us / 1000.0f) 
               << " ms (normal avg: " << (avg_latency / 1000.0f) << " ms)";
            anomalies.push_back({
                record.timestamp_ns,
                "WARNING",
                ss.str(),
                record.layer_name
            });
        }

        // Update rolling average (simple EMA)
        if (avg_latency == 0.0f) {
            rolling_avg_latency_[record.layer_name] = record.latency_us;
        } else {
            rolling_avg_latency_[record.layer_name] = 0.9f * avg_latency + 0.1f * record.latency_us;
        }

        // Add to historical ledger
        for (const auto& anomaly : anomalies) {
            ledger_.push_back(anomaly);
            if (ledger_.size() > max_history_) {
                ledger_.pop_front();
            }
        }

        return anomalies;
    }

    const std::deque<AnomalyEntry>& get_ledger() const {
        return ledger_;
    }

    void clear() {
        ledger_.clear();
        rolling_avg_latency_.clear();
    }

private:
    float outlier_threshold_;
    float sparsity_threshold_;
    size_t max_history_ = 200;
    std::deque<AnomalyEntry> ledger_;
    std::unordered_map<std::string, float> rolling_avg_latency_;
};
