#ifndef TORCH_MERMAID_H
#define TORCH_MERMAID_H


#include <torch/torch.h>
#include <torch/script.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <regex>

namespace torch_utils {

    inline std::string get_module_type(const torch::nn::Module& module) {
        std::string name = module.name();
        size_t last_colon = name.find_last_of(':');
        if (last_colon != std::string::npos) {
            name = name.substr(last_colon + 1);
        }
        if (name.size() > 4 && name.substr(name.size() - 4) == "Impl") {
            name = name.substr(0, name.size() - 4);
        }
        return name;
    }

    inline std::string get_module_info(const torch::nn::Module& module) {
        std::stringstream ss;
        if (auto conv = module.as<torch::nn::Conv2dImpl>()) {
            auto opt = conv->options;
            ss << "k=" << opt.kernel_size()->at(0)
                << ", s=" << opt.stride()->at(0)
                << ", c=" << opt.out_channels();
        }
        else if (auto linear = module.as<torch::nn::LinearImpl>()) {
            ss << "in=" << linear->options.in_features()
                << ", out=" << linear->options.out_features();
        }
        else if (auto pool = module.as<torch::nn::MaxPool2dImpl>()) {
            ss << "MaxPool";
        }
        return ss.str();
    }

    class MermaidGenerator {
    public:
        static void generate(const torch::nn::Module& model, const std::string& output_path, const std::string& title = "Model Architecture") {
            std::ofstream file(output_path);
            if (!file.is_open()) {
                std::cerr << "[ERROR] Failed to open file: " << output_path << std::endl;
                return;
            }

            file << "%% " << title << "\n";
            file << "flowchart TD\n";
            file << "    classDef conv fill:#2196F3,stroke:#1976D2,color:white\n";
            file << "    classDef norm fill:#FF9800,stroke:#EF6C00,color:white\n";
            file << "    classDef act fill:#9C27B0,stroke:#7B1FA2,color:white\n";
            file << "    classDef block fill:#673AB7,stroke:#512DA8,color:white\n";
            file << "    classDef other fill:#607D8B,stroke:#455A64,color:white\n\n";

            int node_counter = 0;
            std::string prev_node = "INPUT";

            file << "    INPUT[\"Input\"]:::other\n";

            traverse_modules(model, file, prev_node, node_counter, "", 0, 2);

            file.close();
            std::cout << "[SUCCESS] Mermaid diagram generated: " << output_path << std::endl;
        }

    private:
        static void traverse_modules(const torch::nn::Module& module, std::ofstream& file,
            std::string& prev_node, int& counter,
            const std::string& parent_prefix, int current_depth, int max_depth) {

            if (current_depth >= max_depth) return;

            auto children = module.named_children();
            if (children.is_empty()) return;

            for (const auto& child : children) {
                std::string name = child.key();
                const auto& sub_mod = *child.value();
                std::string type = get_module_type(sub_mod);
                std::string info = get_module_info(sub_mod);

                std::string node_id = "NODE_" + std::to_string(counter++);
                std::string label = name + "\\n" + type + (info.empty() ? "" : "\\n" + info);

                std::string style = "other";
                if (type.find("Conv") != std::string::npos) style = "conv";
                else if (type.find("Norm") != std::string::npos) style = "norm";
                else if (type.find("ReLU") != std::string::npos || type.find("SiLU") != std::string::npos) style = "act";
                else if (type.find("Block") != std::string::npos || type.find("C2f") != std::string::npos) style = "block";

                file << "    " << node_id << "[\"" << label << "\"]:::" << style << "\n";

                file << "    " << prev_node << " --> " << node_id << "\n";

                prev_node = node_id;

                bool is_container = !sub_mod.children().empty();
                if (is_container && current_depth + 1 < max_depth) {
                    file << "    subgraph " << node_id << "_SG [" << name << "]\n";
                    file << "    direction TB\n";
                    traverse_modules(sub_mod, file, prev_node, counter, name, current_depth + 1, max_depth);
                    file << "    end\n";
                }
            }
        }
    };

    

}

#endif