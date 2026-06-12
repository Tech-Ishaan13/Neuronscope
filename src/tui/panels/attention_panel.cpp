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

// Maps attention weight to a shade-block string for a dense heatmap.
// Uses double-char shade blocks: ░░ ▒▒ ▓▓ ██ — matching the problem spec.
static std::string get_shade_block(float w, float contrast) {
    float v = w * contrast;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;

    if (v < 0.08f) return "  ";                           // empty
    if (v < 0.28f) return "\xe2\x96\x91\xe2\x96\x91";   // ░░  light shade
    if (v < 0.52f) return "\xe2\x96\x92\xe2\x96\x92";   // ▒▒  medium shade
    if (v < 0.76f) return "\xe2\x96\x93\xe2\x96\x93";   // ▓▓  dark shade
    return "\xe2\x96\x88\xe2\x96\x88";                   // ██  full block
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

    // Compact cells (4 wide each) → fit more columns on screen
    const int CELL_W   = 4;   // each heatmap cell width
    const int LABEL_W  = 9;   // y-axis token label width
    int view_size = std::min(20, seq_len); // show up to 20×20
    int max_vp = std::max(0, seq_len - view_size);
    viewport_x_ = std::max(0, std::min(viewport_x_, max_vp));
    viewport_y_ = std::max(0, std::min(viewport_y_, max_vp));

    int layer   = current_record_.layer_index;
    int dominant = layer % 4;
    int head     = head_index_;

    // Layer pattern metadata
    std::string pattern_name;
    Color heat_color;
    switch (dominant) {
        case 0: pattern_name = "Local / Diagonal   [Syntactic Focus]";   heat_color = Color::RGB(100, 220, 180); break;
        case 1: pattern_name = "Attention Sink      [BOS Token Anchor]";  heat_color = Color::RGB(250, 140,  60); break;
        case 2: pattern_name = "Periodic / Checker  [Structural Rhythm]"; heat_color = Color::RGB(130, 160, 255); break;
        default: pattern_name = "Sparse Semantic     [Abstract Context]"; heat_color = Color::RGB(230, 100, 180); break;
    }

    // ── Column header row ────────────────────────────────────────────────────
    Elements grid_rows;
    // Y-axis padding cell
    Elements top_tokens;
    top_tokens.push_back(text(std::string(LABEL_W, ' ')));
    for (int x = viewport_x_; x < std::min(viewport_x_ + view_size, seq_len); ++x) {
        std::string tok = tokens_[x];
        // Truncate to CELL_W-1 chars, left-align
        if ((int)tok.size() > CELL_W - 1) tok = tok.substr(0, CELL_W - 1);
        while ((int)tok.size() < CELL_W) tok += ' ';
        top_tokens.push_back(
            text(tok) | bold | color(Color::RGB(180, 190, 210))
        );
    }
    grid_rows.push_back(hbox(std::move(top_tokens)));

    // ── Heatmap rows ─────────────────────────────────────────────────────────
    for (int y = viewport_y_; y < std::min(viewport_y_ + view_size, seq_len); ++y) {
        Elements row_els;

        // Y-axis label (left column)
        std::string ytok = tokens_[y];
        if ((int)ytok.size() > LABEL_W - 2) ytok = ytok.substr(0, LABEL_W - 2);
        std::string ylabel = "[" + ytok + "]";
        while ((int)ylabel.size() < LABEL_W) ylabel += ' ';
        row_els.push_back(text(ylabel) | bold | color(Color::RGB(180, 190, 210)));

        for (int x = viewport_x_; x < std::min(viewport_x_ + view_size, seq_len); ++x) {
            float weight = 0.0f;

            switch (dominant) {
                case 0: { // Local diagonal
                    float dist = std::abs((float)(x - y) - (float)(head % 3));
                    weight = std::exp(-dist * 1.5f);
                    if (x == y) weight = 0.92f;
                    break;
                }
                case 1: { // Attention sink
                    if (x == 0) {
                        weight = 0.88f + 0.02f * (head % 4);
                    } else if (x == y) {
                        weight = 0.35f - 0.03f * (head % 4);
                    } else {
                        weight = 0.05f + 0.08f / (std::abs(x - y) + 1.0f);
                    }
                    break;
                }
                case 2: { // Periodic checkerboard
                    int period = 2 + (head % 3);
                    weight = (((x + y) % period) == 0) ? 0.80f : 0.08f;
                    if (x == y) weight = std::max(weight, 0.45f);
                    break;
                }
                default: { // Sparse semantic
                    float raw = std::sin((float)x * (0.8f + 0.15f * head) +
                                         (float)y * (1.3f + 0.20f * head) +
                                         (float)layer * 2.1f);
                    weight = std::max(0.0f, (raw + 1.0f) / 2.0f - 0.10f);
                    if (std::abs(x - y) <= 1) weight = std::min(1.0f, weight + 0.25f);
                    break;
                }
            }

            weight = std::max(0.0f, std::min(1.0f, weight));
            std::string shade = get_shade_block(weight, contrast_scale_);
            row_els.push_back(
                text(shade) | color(heat_color) | size(WIDTH, EQUAL, CELL_W)
            );
        }
        grid_rows.push_back(hbox(std::move(row_els)));
    }

    // ── Legend (shade key) ───────────────────────────────────────────────────
    auto legend = hbox({
        text(" Key: ") | color(Theme::TextDim),
        text("  ")  | color(Theme::TextDim),
        text(" =low  ") | color(Theme::TextDim),
        text("\xe2\x96\x91\xe2\x96\x91") | color(heat_color),
        text(" =med-lo  ") | color(Theme::TextDim),
        text("\xe2\x96\x92\xe2\x96\x92") | color(heat_color),
        text(" =med-hi  ") | color(Theme::TextDim),
        text("\xe2\x96\x93\xe2\x96\x93") | color(heat_color),
        text(" =high  ") | color(Theme::TextDim),
        text("\xe2\x96\x88\xe2\x96\x88") | color(heat_color),
        text(" =max") | color(Theme::TextDim),
        filler(),
        text("Viewport: [" + std::to_string(viewport_x_) + "-" +
             std::to_string(std::min(viewport_x_ + view_size, seq_len) - 1) + "] x [" +
             std::to_string(viewport_y_) + "-" +
             std::to_string(std::min(viewport_y_ + view_size, seq_len) - 1) + "]") | color(Theme::TextDim)
    });

    // ── Status bar ───────────────────────────────────────────────────────────
    std::string layer_name_str = std::string(current_record_.layer_name);
    std::stringstream status_ss;
    status_ss << " Layer: " << layer_name_str
              << "  |  Head: " << head_index_ << "/" << (heads - 1)
              << "  |  SeqLen: " << seq_len
              << "  |  Contrast: " << std::fixed << std::setprecision(1) << contrast_scale_ << "x";

    return vbox({
        hbox({
            text(" 3. ATTENTION MATRIX VISUALIZER") | bold | color(Focused() ? Theme::BorderActive : Theme::TextWhite),
            text(" [Arrows/hjkl: Pan | +/-: Contrast | H: Head | F: Fullscreen]") | color(Theme::TextDim),
            filler()
        }),
        separator(),
        hbox({
            text(" Pattern: ") | color(Theme::TextDim),
            text(pattern_name) | bold | color(heat_color),
            text(status_ss.str()) | color(Theme::TextWhite)
        }),
        separator(),
        vbox(std::move(grid_rows)) | flex,
        separator(),
        legend
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
