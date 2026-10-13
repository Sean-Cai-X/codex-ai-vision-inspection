#include "OfflineFeedbackValidation.h"
#include "CxGeoSO2ReferenceAsset.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
namespace f = vision_ai::offline_feedback;
std::string Read(const char* name) {
    std::ifstream file(name, std::ios::binary);
    if (!file) throw std::runtime_error("FEEDBACK_READ_FAILED");
    std::string bytes(1024 * 1024 + 1, '\0');
    file.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    bytes.resize(static_cast<std::size_t>(file.gcount()));
    if (bytes.size() > 1024 * 1024) throw std::runtime_error("FEEDBACK_FILE_LIMIT");
    return bytes;
}
int main(int argc, char** argv) {
    try {
        if (argc < 3 || argc > 5) throw std::runtime_error("usage: offline_feedback_validate contract issue [disposition [repair-binding]]");
        const auto contract = f::ParseStrict(Read(argv[1]));
        const auto issueBytes = Read(argv[2]);
        const auto issue = f::ParseStrict(issueBytes);
        f::ValidateRecord(issue, contract, "issue");
        const auto issueSha = "sha256:" + cxgeom::so2::Sha256Bytes(issueBytes);
        if (argc >= 4) {
            const auto disposition = f::ParseStrict(Read(argv[3]));
            f::ValidateRecord(disposition, contract, "disposition");
            f::Json repair;
            if (argc == 5) {
                repair = f::ParseStrict(Read(argv[4]));
                f::ValidateRecord(repair, contract, "repair");
            }
            f::ValidateLinks(issue, issueSha, disposition, argc == 5 ? &repair : nullptr);
        }
        std::cout << f::Json{{"status","PASS"},{"scope","CONTRACT_ONLY"},
            {"issue_sha256",issueSha},{"production_allowed",false},
            {"model_quality_accepted",false},{"business_issue_closed",false}}.dump() << '\n';
        return 0;
    } catch (const f::Json::exception&) {
        std::cerr << "FEEDBACK_VALIDATION_FAILED: FEEDBACK_JSON_INVALID\n";
        return 1;
    } catch (const std::exception& e) {
        // Never echo raw input, local paths or parser excerpts into feedback logs.
        std::cerr << "FEEDBACK_VALIDATION_FAILED: " << e.what() << '\n';
        return 1;
    }
}
