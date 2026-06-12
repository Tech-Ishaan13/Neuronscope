#pragma once
#include "model/module.h"
#include <string>
#include <vector>
#include <memory>

// Embedding Layer
class Embedding : public Module {
public:
    Embedding(const std::string& name, int vocab_size, int hidden_dim);
    Tensor forward(const Tensor& input) override;
private:
    int vocab_size_;
    int hidden_dim_;
};

// RMSNorm Layer
class RMSNorm : public Module {
public:
    RMSNorm(const std::string& name, int dim);
    Tensor forward(const Tensor& input) override;
private:
    int dim_;
};

// Multi-Head Self Attention Layer (simulating projection and weight computation)
class Attention : public Module {
public:
    Attention(const std::string& name, int hidden_dim, int num_heads);
    Tensor forward(const Tensor& input) override;
private:
    int hidden_dim_;
    int num_heads_;
};

// SwiGLU MLP block
class MLP : public Module {
public:
    MLP(const std::string& name, int hidden_dim, int intermediate_dim);
    Tensor forward(const Tensor& input) override;
private:
    int hidden_dim_;
    int intermediate_dim_;
};

// Complete C++ LlamaDecoderLayer
class LlamaDecoderLayer : public Module {
public:
    LlamaDecoderLayer(const std::string& name, int layer_idx, int hidden_dim, int num_heads);
    Tensor forward(const Tensor& input) override;
private:
    int layer_idx_;
    std::shared_ptr<RMSNorm> input_layernorm_;
    std::shared_ptr<Attention> self_attn_;
    std::shared_ptr<RMSNorm> post_attention_layernorm_;
    std::shared_ptr<MLP> mlp_;
};

// Top-level Model class containing the entire architecture
class LlamaModel : public Module {
public:
    LlamaModel(const std::string& name, int num_layers = 2, int hidden_dim = 4096, int num_heads = 32);
    Tensor forward(const Tensor& input) override;
    
    std::vector<std::shared_ptr<Module>> get_all_submodules();

private:
    std::shared_ptr<Embedding> embed_tokens_;
    std::vector<std::shared_ptr<LlamaDecoderLayer>> layers_;
    std::shared_ptr<RMSNorm> norm_;
};
