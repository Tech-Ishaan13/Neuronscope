#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>

class Module;

// Tensor representing intermediate states
struct Tensor {
    std::vector<float> data;
    std::vector<int> shape;
    std::string dtype = "float16";
    std::string device = "CUDA [GPU 0]";
};

// Hook signatures matching PyTorch API design
using ForwardPreHook = std::function<void(Module*, const Tensor&)>;
using ForwardHook = std::function<void(Module*, const Tensor&, const Tensor&)>;

class Module {
public:
    std::string name;
    std::string type;
    std::vector<std::shared_ptr<Module>> children;

    virtual ~Module() = default;

    void register_forward_pre_hook(ForwardPreHook hook) {
        pre_hooks_.push_back(hook);
    }

    void register_forward_hook(ForwardHook hook) {
        post_hooks_.push_back(hook);
    }

    virtual Tensor forward(const Tensor& input) = 0;

protected:
    void trigger_pre_hooks(const Tensor& input) {
        for (const auto& hook : pre_hooks_) {
            hook(this, input);
        }
    }

    void trigger_post_hooks(const Tensor& input, const Tensor& output) {
        for (const auto& hook : post_hooks_) {
            hook(this, input, output);
        }
    }

private:
    std::vector<ForwardPreHook> pre_hooks_;
    std::vector<ForwardHook> post_hooks_;
};
