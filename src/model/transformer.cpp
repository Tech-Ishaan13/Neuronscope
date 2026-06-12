#include "model/transformer.h"
#include <cmath>
#include <cstdlib>
#include <chrono>
#include <thread>

// Embedding Layer
Embedding::Embedding(const std::string& name, int vocab_size, int hidden_dim)
    : vocab_size_(vocab_size), hidden_dim_(hidden_dim) {
    this->name = name;
    this->type = "Embedding";
}

Tensor Embedding::forward(const Tensor& input) {
    trigger_pre_hooks(input);
    
    // Simulate shape: [batch, seq_len] -> [batch, seq_len, hidden_dim]
    int batch = input.shape[0];
    int seq_len = input.shape[1];
    
    Tensor output;
    output.shape = { batch, seq_len, hidden_dim_ };
    output.dtype = "float16";
    output.device = input.device;
    output.data.resize(batch * seq_len * hidden_dim_);
    
    // Fill with random activations
    for (auto& val : output.data) {
        val = static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f;
    }
    
    // Simulate execution delay
    std::this_thread::sleep_for(std::chrono::microseconds(200));

    trigger_post_hooks(input, output);
    return output;
}

// RMSNorm Layer
RMSNorm::RMSNorm(const std::string& name, int dim) : dim_(dim) {
    this->name = name;
    this->type = "RMSNorm";
}

Tensor RMSNorm::forward(const Tensor& input) {
    trigger_pre_hooks(input);
    
    Tensor output = input; // normalized output retains shape and data size
    
    // Normalization computation (RMSNorm)
    float sum_sq = 0.0f;
    for (float val : input.data) {
        sum_sq += val * val;
    }
    float rms = std::sqrt(sum_sq / input.data.size() + 1e-6f);
    for (auto& val : output.data) {
        val /= rms;
    }
    
    std::this_thread::sleep_for(std::chrono::microseconds(80));

    trigger_post_hooks(input, output);
    return output;
}

// Multi-Head Self Attention Layer
Attention::Attention(const std::string& name, int hidden_dim, int num_heads)
    : hidden_dim_(hidden_dim), num_heads_(num_heads) {
    this->name = name;
    this->type = "Attention";
}

Tensor Attention::forward(const Tensor& input) {
    trigger_pre_hooks(input);
    
    Tensor output = input; // outputs same shape
    int seq_len = input.shape[1];
    
    // Simulate attention calculation and store attention matrix in output metadata
    // We store simulated attention weights inside a special array. In C++ TUI, we can pass this
    // via a side-channel or by storing attention matrix inside the telemetry packet.
    
    std::this_thread::sleep_for(std::chrono::microseconds(600));

    trigger_post_hooks(input, output);
    return output;
}

// MLP Layer
MLP::MLP(const std::string& name, int hidden_dim, int intermediate_dim)
    : hidden_dim_(hidden_dim), intermediate_dim_(intermediate_dim) {
    this->name = name;
    this->type = "MLP";
}

Tensor MLP::forward(const Tensor& input) {
    trigger_pre_hooks(input);
    
    Tensor output = input;
    
    // Simulate sparsity and activation values
    for (size_t i = 0; i < output.data.size(); ++i) {
        // SwiGLU activation logic simulation
        float raw = static_cast<float>(rand()) / RAND_MAX * 4.0f - 2.0f;
        // 54.2% sparsity simulation
        if (static_cast<float>(rand()) / RAND_MAX < 0.542f) {
            output.data[i] = 0.0f;
        } else {
            output.data[i] = raw;
        }
    }
    
    std::this_thread::sleep_for(std::chrono::microseconds(950));

    trigger_post_hooks(input, output);
    return output;
}

// LlamaDecoderLayer
LlamaDecoderLayer::LlamaDecoderLayer(const std::string& name, int layer_idx, int hidden_dim, int num_heads)
    : layer_idx_(layer_idx) {
    this->name = name;
    this->type = "DecoderLayer";
    
    input_layernorm_ = std::make_shared<RMSNorm>(name + ".input_layernorm", hidden_dim);
    self_attn_ = std::make_shared<Attention>(name + ".self_attn", hidden_dim, num_heads);
    post_attention_layernorm_ = std::make_shared<RMSNorm>(name + ".post_attention_layernorm", hidden_dim);
    mlp_ = std::make_shared<MLP>(name + ".mlp", hidden_dim, hidden_dim * 8 / 3);
    
    children.push_back(input_layernorm_);
    children.push_back(self_attn_);
    children.push_back(post_attention_layernorm_);
    children.push_back(mlp_);
}

Tensor LlamaDecoderLayer::forward(const Tensor& input) {
    trigger_pre_hooks(input);
    
    Tensor x = input_layernorm_->forward(input);
    Tensor attn_out = self_attn_->forward(x);
    
    // Residual connection
    for (size_t i = 0; i < attn_out.data.size(); ++i) {
        attn_out.data[i] += input.data[i];
    }
    
    Tensor y = post_attention_layernorm_->forward(attn_out);
    Tensor mlp_out = mlp_->forward(y);
    
    // Residual connection
    for (size_t i = 0; i < mlp_out.data.size(); ++i) {
        mlp_out.data[i] += attn_out.data[i];
    }
    
    trigger_post_hooks(input, mlp_out);
    return mlp_out;
}

// LlamaModel
LlamaModel::LlamaModel(const std::string& name, int num_layers, int hidden_dim, int num_heads) {
    this->name = name;
    this->type = "LlamaModel";
    
    embed_tokens_ = std::make_shared<Embedding>(name + ".embed_tokens", 32000, hidden_dim);
    children.push_back(embed_tokens_);
    
    for (int i = 0; i < num_layers; ++i) {
        auto layer = std::make_shared<LlamaDecoderLayer>(name + ".layers." + std::to_string(i), i, hidden_dim, num_heads);
        layers_.push_back(layer);
        children.push_back(layer);
    }
    
    norm_ = std::make_shared<RMSNorm>(name + ".norm", hidden_dim);
    children.push_back(norm_);
}

Tensor LlamaModel::forward(const Tensor& input) {
    trigger_pre_hooks(input);
    
    Tensor x = embed_tokens_->forward(input);
    for (auto& layer : layers_) {
        x = layer->forward(x);
    }
    Tensor out = norm_->forward(x);
    
    trigger_post_hooks(input, out);
    return out;
}

std::vector<std::shared_ptr<Module>> LlamaModel::get_all_submodules() {
    std::vector<std::shared_ptr<Module>> list;
    
    // Pre-order traversal to populate in hierarchical structure
    std::function<void(Module*)> traverse = [&](Module* mod) {
        for (const auto& child : mod->children) {
            list.push_back(child);
            traverse(child.get());
        }
    };
    
    list.push_back(embed_tokens_);
    for (auto& layer : layers_) {
        list.push_back(layer);
        for (auto& child : layer->children) {
            list.push_back(child);
        }
    }
    list.push_back(norm_);
    
    return list;
}
