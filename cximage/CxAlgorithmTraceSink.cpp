#include "CxAlgorithmTraceSink.h"

CxAlgorithmTraceCallback CxAlgorithmTraceScope::callback_;

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