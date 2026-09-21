#ifndef CXPARSER_EXT_DEBUG_CXSCRIPT_DEBUG_LOG_BUILDER_H
#define CXPARSER_EXT_DEBUG_CXSCRIPT_DEBUG_LOG_BUILDER_H

#include "cxscript_debug_result.h"

#include <string>

namespace cxparser_ext {
namespace debug {

std::string BuildCxScriptDebugRawLog(const EmbeddedDebugRunResult& result);

}  // namespace debug
}  // namespace cxparser_ext

#endif
