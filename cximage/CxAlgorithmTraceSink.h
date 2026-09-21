#pragma once

#include <functional>
#include <string>

struct CxAlgorithmTraceEvent
{
    std::string tool;
    std::string phase;
    std::string status;
    std::string message;

    int scan_index = 0;
    int sample_count = 0;
    int valid_points = 0;
    int elapsed_ms = 0;
};

using CxAlgorithmTraceCallback =
    std::function<void(const CxAlgorithmTraceEvent&)>;

class CxAlgorithmTraceScope
{
public:
    static void SetCallback(CxAlgorithmTraceCallback cb);
    static void Emit(const CxAlgorithmTraceEvent& e);
    static void Clear();

private:
    static CxAlgorithmTraceCallback callback_;
};