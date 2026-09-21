#ifndef CXIMAGE_CXSCRIPT_RUNTIME_CAPTURE_SMOKE_H
#define CXIMAGE_CXSCRIPT_RUNTIME_CAPTURE_SMOKE_H

#include <string>

namespace mu
{
    class CxParserRuntime;
}

struct CxScriptExecutionCapture;

bool ValidateCxScriptRuntimeCaptureSmoke(
    mu::CxParserRuntime& runtime,
    CxScriptExecutionCapture& capture,
    std::string& reason);

#endif