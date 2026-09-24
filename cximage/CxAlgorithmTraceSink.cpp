#include "CxAlgorithmTraceSink.h"
#include "CxStartupTrace.h"

CxStartupTraceMark g_algorithm_begin("[startup] algorithm-trace: begin\n");
CxAlgorithmTraceCallback CxAlgorithmTraceScope::callback_;
CxStartupTraceMark g_algorithm_end("[startup] algorithm-trace: ready\n");

void CxAlgorithmTraceScope::SetCallback(CxAlgorithmTraceCallback cb)
{
    callback_ = std::move(cb);
}

void CxAlgorithmTraceScope::Emit(const CxAlgorithmTraceEvent& e)
{
    if (callback_)
    {
        callback_(e);
    }
}

void CxAlgorithmTraceScope::Clear()
{
    callback_ = nullptr;
}