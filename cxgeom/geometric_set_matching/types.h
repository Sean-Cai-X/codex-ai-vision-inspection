#pragma once
#include <cstddef>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace cxgeom::gsm {
struct Point2 { double x=0, y=0; };
struct LineSegment { Point2 start, end; }; // Endpoint order is local, not set order.
struct ArcSegment {
    Point2 center;
    double radius=0, start_deg=0, sweep_deg=0; // Signed sweep; no forced closing edge.
};
using Geometry = std::variant<Point2, LineSegment, ArcSegment>;
struct Element {
    std::string stable_id;
    std::string source_ref; // Local provenance reference, never image bytes.
    double quality=1;
    Geometry geometry;
};
struct Rect { double x=0, y=0, width=0, height=0; };
struct GeometricSet {
    std::string set_id;
    std::string source_ref;
    Rect roi;
    std::vector<Element> elements; // Unordered across elements.
};
struct Similarity2d {
    Point2 translation;
    double angle_deg=0, scale=1; // x' = scale * R(angle) * x + translation.
};
struct Parameters {
    Rect search_roi;
    double angle_min_deg=-180, angle_max_deg=180;
    double scale_min=0.8, scale_max=1.2;
    double max_residual_px=1;
    double min_weighted_coverage=0.5;
    double min_spatial_span_ratio=0.25;
    double min_candidate_score_gap=0.05;
    std::size_t min_matched_elements=3;
    std::size_t max_elements_per_set=4096;
    std::size_t max_hypotheses=256;
    std::size_t max_pair_checks=1000000;
    unsigned max_elapsed_ms=100;
    unsigned seed=0;
};
struct Request {
    std::string request_id;
    GeometricSet reference, target;
    Parameters parameters;
};
enum class ExecutionStatus { NotRun, InvalidInput, BudgetExhausted, NotImplemented, Completed };
enum class Solvability { FullySolvable, PartiallySolvable, Ambiguous, Insufficient };
struct Correspondence {
    std::string reference_id, target_id;
    double score=0, residual_px=0;
};
struct Candidate {
    std::optional<Similarity2d> pose; // No identity-pose placeholder.
    std::vector<Correspondence> correspondences;
    std::vector<std::string> unmatched_reference_ids, unmatched_target_ids;
    double weighted_coverage=0, spatial_span_ratio=0, residual_px=0;
    bool translation_observable=false, rotation_observable=false, scale_observable=false;
};
struct Result {
    std::string request_id;
    ExecutionStatus execution_status=ExecutionStatus::NotRun;
    std::optional<Solvability> solvability; // Absent unless actually assessed.
    std::vector<Candidate> candidates;
    std::vector<std::string> reasons, evidence_refs;
    std::size_t pair_checks=0;
    double elapsed_ms=0;
    bool search_complete=false;
    bool production_eligible=false;
};
struct Validation {
    bool accepted=false;
    ExecutionStatus execution_status=ExecutionStatus::InvalidInput;
    std::vector<std::string> reasons;
};
Validation Validate(const Request& request);
// P0 contract boundary ONLY. Valid input returns NotImplemented, never a match.
Result Match(const Request& request);
const char* Name(ExecutionStatus status);
const char* Name(Solvability status);
}
