#pragma once
#include "../libtorchsegmentation/src/utils/json.hpp"
#include <string>
#include <vector>

namespace cxvision::supervision {
struct Conversion {
    int width = 0, height = 0;
    std::vector<unsigned char> mask;
    nlohmann::json receipt;
};
// Original-image coordinates only. Throws stable error codes on invalid input.
Conversion Convert(const nlohmann::json& sample, const nlohmann::json& rules);
}

// Development-only CxScript adapter. No model activation or training claim.
class CxSevenClassSupervision {
public:
    void loadrules(const char* path);
    void load(const char* path);
    void run();
    void expectstatus(const char* expected);
    void save(const char* new_directory);
    void clear();
    const cxvision::supervision::Conversion& result() const { return result_; }
    const std::string& status() const { return status_; }
private:
    nlohmann::json rules_, sample_;
    cxvision::supervision::Conversion result_;
    std::string status_ = "NOT_RUN";
    void invalidate();
};
