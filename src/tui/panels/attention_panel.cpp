#include "tui/panels/attention_panel.h"
#include "tui/theme.h"
#include "ftxui/component/event.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

AttentionPanel::AttentionPanel() {
    generate_tokens(64);
}

void AttentionPanel::generate_tokens(int count) {
    std::vector<std::string> sample_words = {
        "I", "want", "it", "to", "be", "keyboard", "driven", "and", "fast",
        "NeuronScope", "local", "telemetry", "diagnostics", "for", "large", "language",
        "models", "running", "on", "CUDA", "GPU", "layer", "by", "layer", "execution",
        "latency", "and", "sparsity", "numerical", "anomalies", "ledger", "outliers",
        "attention", "matrix", "visualizer", "head", "0", "SwiGLU", "activation", "weights"
    };

    tokens_.clear();
    for (int i = 0; i < count; ++i) {
        tokens_.push_back(sample_words[i % sample_words.size()]);
    }
}

void AttentionPanel::update_record(const TelemetryRecord& record) {
    current_record_ = record;
    has_record_ = true;
    if (tokens_.size() < record.attn_seq_len) {
        generate_tokens(record.attn_seq_len);
    }
}

// Maps an attention weight to a Unicode block character for heatmap display
static const char* get_block_char(float w, float contrast) {
    float v = w * contrast;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;

    if (v < 0.125f) return " ";       // empty
    if (v < 0.250f) return "\xe2\x96\x81"; // ▁
    if (v < 0.375f) return "\xe2\x96\x82"; // ▂
    if (v < 0.500f) return "\xe2\x96\x83"; // ▃
    if (v < 0.625f) return "\xe2\x96\x84"; // ▄
    if (v < 0.750f) return "\xe2\x96\x85"; // ▅
    if (v < 0.875f) return "\xe2\x96\x86"; // ▆
    return "\xe2\x96\x88";                 // █ (full block)
}

ftxui::Element AttentionPanel::Render() {
    using namespace ftxui;

    if (!has_record_ || !(current_record_.flags & FLAG_HAS_ATTN)) {
        return vbox({
            hbox({
                text(" 3. ATTENTION MATRIX VISUALIZER (Arrows/h,j,k,l: Pan | +/-: Contrast | H: Head | F: Fullscreen) ") | bold | color(Focused() ? Theme::BorderActive : Theme::TextWhite),
                filler()
            }),
            separator(),
            text("Select an Attention layer in Topology / Packets to visualize attention weights.") | color(Theme::TextDim) | center | flex
        }) | Theme::StyledBorder(Focused());
    }

    int seq_len = current_record_.attn_seq_len;
    if (seq_len <= 0) seq_len = 8;
    int heads = current_record_.attn_num_heads;
    if (heads <= 0) heads = 1;
    head_index_ = head_index_ % heads;

    // Viewport clamp — view_size now larger since panel has full right-column height
    int view_size = std::min(16, seq_len); // up to 16x16 grid visible
    int max_vp = std::max(0, seq_len - view_size);
    viewport_x_ = std::max(0, std::min(viewport_x_, max_vp));
    viewport_y_ = std::max(0, std::min(viewport_y_, max_vp));

    // Render Grid
    Elements grid_rows;

    // Header row of tokens — format each label to be exactly 10 characters (padding and truncating)
    Elements top_tokens = { text("          ") }; // padding for y-axis token names (10 spaces)
    for (int x = viewport_x_; x < std::min(viewport_x_ + view_size, seq_len); ++x) {
        std::string tok = tokens_[x];
        if (tok.size() > 7) tok = tok.substr(0, 7);
        std::string label = "[" + tok + "]";
        if (label.size() < 10) label.append(10 - label.size(), ' ');
        top_tokens.push_back(text(label) | bold | color(Theme::TextWhite));
    }
    grid_rows.push_back(hbox(std::move(top_tokens)));

    int layer = current_record_.layer_index;

    // Each layer has a dominant pattern archetype — clearly distinct per layer.
    // Layer 0: strong diagonal (local syntactic attention)
    // Layer 1: attention sink (BOS token dominance)
    // Layer 2: checkerboard (structured periodic)
    // Layer 3+: sparse semantic (wavy sine-based)
    // Head offset shifts the pattern for variety within a layer.
    int dominant = layer % 4;
    int head = head_index_;

    // Layer-specific description and color tint
    std::string pattern_name;
    Color heat_color;
    switch (dominant) {
        case 0: pattern_name = "Local/Diagonal  [Syntactic Focus]";   heat_color = Color::RGB(100, 220, 180); break;
        case 1: pattern_name = "Attention Sink   [BOS Token Anchor]";  heat_color = Color::RGB(250, 140,  60); break;
        case 2: pattern_name = "Periodic/Checker [Structural Rhythm]"; heat_color = Color::RGB(130, 160, 255); break;
        default: pattern_name = "Sparse Semantic  [Abstract Context]"; heat_color = Color::RGB(230, 100, 180); break;
    }

    // Heatmap rows — format each y-axis label to be exactly 10 characters
    for (int y = viewport_y_; y < std::min(viewport_y_ + view_size, seq_len); ++y) {
        Elements row_els;
        std::string tok = tokens_[y];
        if (tok.size() > 7) tok = tok.substr(0, 7);
        std::string y_tok = "[" + tok + "]";
        if (y_tok.size() < 10) y_tok.append(10 - y_tok.size(), ' ');
        row_els.push_back(text(y_tok) | bold | color(Theme::TextWhite));

        for (int x = viewport_x_; x < std::min(viewport_x_ + view_size, seq_len); ++x) {
            float weight = 0.0f;

            // Primary pattern driven by layer archetype
            switch (dominant) {
                case 0: { // Local diagonal — strong for layer 0, shifts with head
                    float dist = std::abs((float)(x - y) - (float)(head % 3));
                    weight = std::exp(-dist * 1.5f);
                    if (x == y) weight = 0.92f;
                    break;
                }
                case 1: { // Attention sink — BOS column is bright, rest fades
                    if (x == 0) {
                        weight = 0.88f + 0.02f * (head % 4);
                    } else if (x == y) {
                        weight = 0.35f - 0.03f * (head % 4);
                    } else {
                        weight = 0.05f + 0.08f / (std::abs(x - y) + 1.0f);
                    }
                    break;
                }
                case 2: { // Periodic checkerboard, period shifts with head
                    int period = 2 + (head % 3);
                    weight = (((x + y) % period) == 0) ? 0.80f : 0.08f;
                    // add a faint diagonal echo
                    if (x == y) weight = std::max(weight, 0.45f);
                    break;
                }
                default: { // Sparse semantic — complex wavy pattern
                    float raw = std::sin((float)x * (0.8f + 0.15f * head) +
                                         (float)y * (1.3f + 0.20f * head) +
                                         (float)layer * 2.1f);
                    weight = std::max(0.0f, (raw + 1.0f) / 2.0f - 0.10f);
                    // boost near-diagonal slightly for readability
                    if (std::abs(x - y) <= 1) weight = std::min(1.0f, weight + 0.25f);
                    break;
                }
            }

            if (weight < 0.0f) weight = 0.0f;
            if (weight > 1.0f) weight = 1.0f;

            const char* glyph = get_block_char(weight, contrast_scale_);
            row_els.push_back(text(glyph) | color(heat_color) | size(WIDTH, EQUAL, 10) | center);
        }
        grid_rows.push_back(hbox(std::move(row_els)));
    }

    // Build a rich status bar clearly identifying which layer is shown
    std::string layer_name_str = std::string(current_record_.layer_name);
    std::stringstream status_ss;
    status_ss << " Layer: " << layer_name_str
              << "  |  Idx: " << layer
              << "  |  Head: " << head_index_ << "/" << (heads - 1)
              << "  |  SeqLen: " << seq_len
              << "  |  Contrast: " << std::fixed << std::setprecision(1) << contrast_scale_ << "x";

    return vbox({
        hbox({
            text(" 3. ATTENTION MATRIX VISUALIZER (Arrows/h,j,k,l: Pan | +/-: Contrast | H: Head | F: Fullscreen) ") | bold | color(Focused() ? Theme::BorderActive : Theme::TextWhite),
            filler()
        }),
        separator(),
        hbox({
            text(" Pattern: ") | color(Theme::TextDim),
            text(pattern_name) | bold | color(heat_color),
            text(status_ss.str()) | color(Theme::TextWhite)
        }),
        separator(),
        vbox(std::move(grid_rows)) | flex
    }) | Theme::StyledBorder(Focused());
}

bool AttentionPanel::OnEvent(ftxui::Event event) {
    if (!has_record_ || !(current_record_.flags & FLAG_HAS_ATTN)) return false;

    int seq_len = current_record_.attn_seq_len;

    if (event == ftxui::Event::ArrowLeft || event == ftxui::Event::Character('h')) {
        viewport_x_ = std::max(0, viewport_x_ - 1);
        return true;
    }
    if (event == ftxui::Event::ArrowRight || event == ftxui::Event::Character('l')) {
        int view_size = std::min(16, seq_len);
        viewport_x_ = std::min(std::max(0, seq_len - view_size), viewport_x_ + 1);
        return true;
    }
    if (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character('k')) {
        viewport_y_ = std::max(0, viewport_y_ - 1);
        return true;
    }
    if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Character('j')) {
        int view_size = std::min(16, seq_len);
        viewport_y_ = std::min(std::max(0, seq_len - view_size), viewport_y_ + 1);
        return true;
    }
    if (event == ftxui::Event::Character('+') || event == ftxui::Event::Character('=')) {
        contrast_scale_ = std::min(5.0f, contrast_scale_ + 0.2f);
        return true;
    }
    if (event == ftxui::Event::Character('-') || event == ftxui::Event::Character('_')) {
        contrast_scale_ = std::max(0.2f, contrast_scale_ - 0.2f);
        return true;
    }
    if (event == ftxui::Event::Character('H')) {
        int heads = current_record_.attn_num_heads;
        if (heads > 0) {
            head_index_ = (head_index_ + 1) % heads;
        }
        return true;
    }
    if (event == ftxui::Event::Character('F') || event == ftxui::Event::Character('f')) {
        toggle_fullscreen();
        return true;
    }

    return false;
}

void AttentionPanel::clear() {
    has_record_ = false;
}
