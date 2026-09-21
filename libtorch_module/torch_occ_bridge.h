#ifndef TORCH_OCC_BRIDGE_H
#define TORCH_OCC_BRIDGE_H

#include <stdexcept>
#include <string>
#include <vector>

#include <torch/torch.h>

class IOccTensorBridge {
public:
    virtual ~IOccTensorBridge() = default;

    virtual torch::Tensor to_tensor(const std::vector<float> & values) const = 0;
};

class OccTensorBridge final : public IOccTensorBridge {
public:
    explicit OccTensorBridge(int64_t expected_dim = 96)
        : expected_dim_(expected_dim) {}

    torch::Tensor to_tensor(const std::vector<float> & values) const override {
        if (expected_dim_ <= 0) {
            throw std::invalid_argument("OccTensorBridge expected_dim must be positive.");
        }

        if (values.empty()) {
            return torch::zeros({1, expected_dim_}, torch::TensorOptions().dtype(torch::kFloat32));
        }

        const auto actual_dim = static_cast<int64_t>(values.size());
        if (actual_dim != expected_dim_) {
            throw std::invalid_argument(
                "OccTensorBridge descriptor size mismatch. Expected " +
                std::to_string(expected_dim_) +
                " got " +
                std::to_string(actual_dim) + ".");
        }

        return torch::tensor(
            values,
            torch::TensorOptions().dtype(torch::kFloat32)).unsqueeze(0);
    }

    int64_t expected_dim() const {
        return expected_dim_;
    }

private:
    int64_t expected_dim_;
};

#endif
