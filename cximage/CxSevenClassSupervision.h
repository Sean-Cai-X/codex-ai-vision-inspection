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
// Freeze declared source metadata + recomputed conversions, not image bytes.
nlohmann::json FreezeDataset(const nlohmann::json& project, const nlohmann::json& rules);
nlohmann::json ParseJson(const std::string& bytes);
// Runtime ABI uses sha256:<hex>; supervision JSON uses lowercase bare hex.
std::string DecodeBusinessSha256(const std::string& digest);
// Called after the runtime has read/hashed actual image and mask bytes.
// materialized rows: asset_ref, split (train/val), image_sha256,
// mask_pixels_sha256, width, height. VERIFY is metadata-only here.
void ValidateMaterializedDataset(const nlohmann::json& frozen,
    const nlohmann::json& materialized, const std::string& expected_revision,
    const std::string& expected_class);
}

// Development-only CxScript adapter. No model activation or training claim.
class CxSevenClassSupervision {
public:
    void loadrules(const char* path);
    void load(const char* path);
    void run();
    void freeze();
    void savefreeze(const char* new_directory);
    void expectstatus(const char* expected);
    void save(const char* new_directory);
    void clear();
    const cxvision::supervision::Conversion& result() const { return result_; }
    const std::string& status() const { return status_; }
    const nlohmann::json& frozen() const { return frozen_; }
private:
    nlohmann::json rules_, sample_, frozen_;
    cxvision::supervision::Conversion result_;
    std::string status_ = "NOT_RUN";
    void invalidate();
};
