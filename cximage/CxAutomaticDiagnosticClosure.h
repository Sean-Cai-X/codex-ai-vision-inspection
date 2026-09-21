#pragma once

#include <filesystem>
#include <string>

struct CxAutomaticDiagnosticClosureOptions
{
    std::filesystem::path matrix_path;
    std::filesystem::path output_dir;
};

struct CxAutomaticDiagnosticClosureResult
{
    bool executed = false;
    bool complete = false;
    std::string status = "NOT_RUN";
    std::string reason;
    int discovered_rows = 0;
    int bound_rows = 0;
    int executed_cases = 0;
    int completed_cases = 0;
    int rejected_cases = 0;
    std::filesystem::path preflight_ref;
    std::filesystem::path process_status_ref;
    std::filesystem::path aggregate_ref;
    std::filesystem::path stability_ref;
    std::filesystem::path promotion_gate_ref;
};

bool RunCxAutomaticDiagnosticClosure(
    const CxAutomaticDiagnosticClosureOptions& options,
    CxAutomaticDiagnosticClosureResult& result,
    std::string& reason);
