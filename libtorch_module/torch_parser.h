#ifndef TORCH_PARSER_H
#define TORCH_PARSER_H


#include <torch/torch.h>
#include <torch/script.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <regex>
#include <iostream>
#include <fstream>

inline std::vector<char> get_the_bytes(const std::string& filename) {
    std::ifstream input(filename, std::ios::binary);
    std::vector<char> bytes(
        (std::istreambuf_iterator<char>(input)),
        (std::istreambuf_iterator<char>()));
    input.close();
    return bytes;
}

class WeightParser {
public:
    WeightParser(const std::string& weight_path) {
        load_weights(weight_path);
    }

    void load_to(torch::nn::Module& model) {
        torch::NoGradGuard no_grad;

        auto params = model.named_parameters();
        auto buffers = model.named_buffers();

        int loaded_count = 0;
        int total_count = 0;

        for (auto& pair : params) {
            total_count++;
            std::string cpp_key = pair.key();
            std::string py_key = map_cpp_to_python(cpp_key);

            if (try_copy(pair.value(), py_key, cpp_key)) {
                loaded_count++;
            }
        }

        for (auto& pair : buffers) {
            total_count++;
            std::string cpp_key = pair.key();
            std::string py_key = map_cpp_to_python(cpp_key);

            if (cpp_key.find("num_batches_tracked") != std::string::npos) {
                continue;
            }

            if (try_copy(pair.value(), py_key, cpp_key)) {
                loaded_count++;
            }
        }

        std::cout << "[WeightParser] Loaded " << loaded_count << " / " << total_count
            << " tensors successfully." << std::endl;
    }

private:
    std::unordered_map<std::string, torch::Tensor> weight_map_;

    void load_weightso(const std::string& path) {
        try {
            std::vector<char> f = get_the_bytes(path);
            torch::IValue data = torch::pickle_load(f);

            if (data.isGenericDict()) {
                auto dict = data.toGenericDict();
                for (auto& item : dict) {
                    std::string key = item.key().toStringRef();
                    torch::Tensor val = item.value().toTensor();
                    weight_map_[key] = val;
                }
            }
            else {
                std::cerr << "[Error] Weight file is not a dictionary!" << std::endl;
            }
            std::cout << "[WeightParser] Loaded " << weight_map_.size() << " keys from " << path << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "[Error] Failed to load weights: " << e.what() << std::endl;
        }
    }
    void load_weights(const std::string& path) {
        try {
            std::vector<char> f = get_the_bytes(path);
            torch::IValue data = torch::pickle_load(f);

            std::cout << "[Debug] Loaded IValue tag: " << data.tagKind() << std::endl;

            if (data.isGenericDict()) {
                auto dict = data.toGenericDict();
                for (auto& item : dict) {
                    std::string key = item.key().toStringRef();
                    torch::Tensor val = item.value().toTensor();
                    weight_map_[key] = val;
                }
            }
            else if (data.isTuple()) {
                std::cerr << "[Warning] Loaded data is a Tuple, not a Dict. Trying to parse as list of pairs..." << std::endl;
            }
            else if (data.isNone()) {
                std::cerr << "[Error] Loaded data is None! Check your python export script." << std::endl;
                return;
            }
            else {
                std::cerr << "[Error] Unexpected data type. Expected GenericDict." << std::endl;
                return;
            }

            std::cout << "[WeightParser] Loaded " << weight_map_.size() << " keys from " << path << std::endl;

        }
        catch (const std::exception& e) {
            std::cerr << "[Error] Failed to load weights: " << e.what() << std::endl;
        }
    }

    bool try_copy(torch::Tensor& target, const std::string& py_key, const std::string& cpp_key) {
        if (weight_map_.count(py_key)) {
            torch::Tensor source = weight_map_[py_key];

            if (target.sizes() != source.sizes()) {

                std::cerr << "[Mismatch] Shape mismatch for " << cpp_key
                    << " (Cpp: " << target.sizes() << " vs Py: " << source.sizes() << ")" << std::endl;
                return false;
            }

            target.copy_(source);
            return true;
        }
        else {
            return false;
        }
    }

    std::string map_cpp_to_python(const std::string& cpp_key) {
        std::string py_key = cpp_key;

        py_key = std::regex_replace(py_key, std::regex("^backbone\\.stem"), "model.0");
        py_key = std::regex_replace(py_key, std::regex("^backbone\\.conv1"), "model.1");
        py_key = std::regex_replace(py_key, std::regex("^backbone\\.c2f1"), "model.2");
        py_key = std::regex_replace(py_key, std::regex("^backbone\\.conv2"), "model.3");
        py_key = std::regex_replace(py_key, std::regex("^backbone\\.c2f2"), "model.4");
        py_key = std::regex_replace(py_key, std::regex("^backbone\\.conv3"), "model.5");
        py_key = std::regex_replace(py_key, std::regex("^backbone\\.c2f3"), "model.6");
        py_key = std::regex_replace(py_key, std::regex("^backbone\\.conv4"), "model.7");
        py_key = std::regex_replace(py_key, std::regex("^backbone\\.c2f4"), "model.8");
        py_key = std::regex_replace(py_key, std::regex("^backbone\\.sppf"), "model.9");

        py_key = std::regex_replace(py_key, std::regex("^neck\\.c2f_p4"), "model.12");
        py_key = std::regex_replace(py_key, std::regex("^neck\\.c2f_p3"), "model.15");
        py_key = std::regex_replace(py_key, std::regex("^neck\\.down_p3"), "model.16");
        py_key = std::regex_replace(py_key, std::regex("^neck\\.c2f_n4"), "model.18");
        py_key = std::regex_replace(py_key, std::regex("^neck\\.down_p4"), "model.19");
        py_key = std::regex_replace(py_key, std::regex("^neck\\.c2f_n5"), "model.21");

        py_key = std::regex_replace(py_key, std::regex("^head"), "model.22");

        py_key = std::regex_replace(py_key, std::regex("bottlenecks_\\."), "m.");

        py_key = std::regex_replace(py_key, std::regex("cv2_layers_\\."), "cv2.");
        py_key = std::regex_replace(py_key, std::regex("cv3_layers_\\."), "cv3.");

        return py_key;
    }
};

#endif