#ifndef CXPARSER_EXT_DEBUG_CXSCRIPT_DEBUG_EVENT_H
#define CXPARSER_EXT_DEBUG_CXSCRIPT_DEBUG_EVENT_H

namespace cxparser_ext {
namespace debug {

enum class EmbeddedDebugEventKind
{
  LineView,
  StatementView,
  ObjectAssignment,
  MethodCall,
  ReturnObject,
  RuntimeSnapshot,
  VariableSnapshot
};

}  // namespace debug
}  // namespace cxparser_ext

#endif
