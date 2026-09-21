#ifndef CXPARSER_EXT_PARSER_VALIDATION_ENGINE_H
#define CXPARSER_EXT_PARSER_VALIDATION_ENGINE_H

#include "../adapters/clang/clang_types.h"
#include "../meta/parser_binding_spec.h"
#include "../meta/parser_evidence.h"
#include "../meta/parser_validation_types.h"
#include "../pipeline/parser_task_types.h"

namespace cxparser_ext
{
class ParserValidationEngine
{
public:
  bool CompareExecutionAndEvidence(const ExecutionResult &result,
                                   const ParserEvidenceBundle &bundle,
                                   ParserValidationReport &report);

  bool CompareSchemaAndBinding(const ApiSchema &schema,
                               const ParserBindingSpec &spec,
                               ParserValidationReport &report);
};
}

#endif
