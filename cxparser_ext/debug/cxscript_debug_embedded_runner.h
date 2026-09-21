#ifndef CXPARSER_EXT_DEBUG_CXSCRIPT_DEBUG_EMBEDDED_RUNNER_H
#define CXPARSER_EXT_DEBUG_CXSCRIPT_DEBUG_EMBEDDED_RUNNER_H

#include "cxscript_debug_result.h"

namespace cxparser_ext {
namespace debug {

EmbeddedDebugRunResult RunCxScriptDebugEmbedded(
  const EmbeddedDebugRunRequest& request);

}  // namespace debug
}  // namespace cxparser_ext

#endif
