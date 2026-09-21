#include "CxFastMatchRuntimeCapture.h"

#include "FastMatch.h"
#include "Image.h"
#include "ParserDebugBridge.h"

#include <cmath>
#include <limits>
#include <sstream>

namespace
{

bool QueryGlobalInt(
    ParserDebugBridge& bridge,
    const std::string& name,
    int& value)
{
    double numeric = 0.0;

    if (!bridge.QueryDouble(name, numeric))
        return false;

    if (!std::isfinite(numeric))
        return false;

    const double rounded = std::round(numeric);

    if (std::abs(numeric - rounded) > 1e-9)
        return false;

    if (rounded < static_cast<double>(std::numeric_limits<int>::min()) ||
        rounded > static_cast<double>(std::numeric_limits<int>::max()))
        return false;

    value = static_cast<int>(rounded);
    return true;
}

}

bool CaptureFastMatchRuntime(
    ParserDebugBridge& bridge,
    const std::string& object_name,
    CxFastMatchRuntimeCapture& capture)
{
    capture = {};

    void* object =
        bridge.QueryClassObject("FastMatch", object_name);

    if (object == nullptr)
    {
        capture.failure_stage = "fastmatch_object_lookup";
        capture.reason = "runtime object not found: " + object_name;
        return false;
    }

    capture.object_found = true;
    capture.object_unique = true;
    capture.object_address = reinterpret_cast<std::uintptr_t>(object);
    capture.object_aliases = "FastMatch";

    auto* matcher =
        static_cast<FastMatch*>(object);

    capture.object_model_point_count =
        matcher->getmodelpointcount();
    capture.object_learn_a_count =
        matcher->getlearnacount();
    capture.object_learn_b_count =
        matcher->getlearnbcount();
    capture.object_learn_a2_count =
        matcher->getlearna2count();
    capture.object_learn_b2_count =
        matcher->getlearnb2count();
    capture.object_learn_status_code =
        matcher->getlearnstatuscode();
    capture.object_normal_trace_candidate_count =
        matcher->getnormaltracecandidatecount();
    capture.object_normal_trace_deduplicated_count =
        matcher->getnormaltracededuplicatedcount();
    capture.object_normal_trace_point_count =
        matcher->getnormaltracepointcount();
    capture.object_normal_trace_pair_count =
        matcher->getnormaltracepaircount();
    capture.last_learn_argument =
        matcher->debuglastlearnargument();

    capture.global_model_found =
        QueryGlobalInt(
            bridge,
            "global_model_point_count",
            capture.global_model_point_count);

    capture.global_learn_a_found =
        QueryGlobalInt(
            bridge,
            "global_learn_a_count",
            capture.global_learn_a_count);

    capture.global_learn_b_found =
        QueryGlobalInt(
            bridge,
            "global_learn_b_count",
            capture.global_learn_b_count);

    capture.global_learn_status_found =
        QueryGlobalInt(
            bridge,
            "global_learn_status_code",
            capture.global_learn_status_code);

    auto* local_image = static_cast<Image*>(
        bridge.QueryClassObject("Image", "m_image"));

    auto* global_image = bridge.QueryImage("global_matInput");

    capture.local_image_address =
        reinterpret_cast<std::uintptr_t>(local_image);
    capture.global_image_address =
        reinterpret_cast<std::uintptr_t>(global_image);

    if (local_image != nullptr)
    {
        const cv::Mat& mat = local_image->getmat();

        capture.image_empty = mat.empty();
        capture.image_rows = mat.rows;
        capture.image_cols = mat.cols;
        capture.image_type = mat.empty() ? -1 : mat.type();
    }

    return true;
}
