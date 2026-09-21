#ifndef CXPARSER_EXT_PARSER_PIPELINE_H
#define CXPARSER_EXT_PARSER_PIPELINE_H

#include "../meta/parser_binding_spec.h"
#include "../meta/parser_execution_guard.h"
#include "../meta/parser_evidence.h"
#include "../meta/parser_validation_types.h"
#include "../validation/parser_validation_engine.h"
#include "parser_runtime_facade.h"
#include "parser_task_types.h"

namespace cxparser_ext
{
class ParserPipeline
{
public:
  ParserPipeline();

  void Reset();
  void SetGuardProfile(ExecutionGuardProfile profile);
  void SetGuardLimits(const ExecutionGuardLimits &limits);
  ExecutionGuardLimits GetGuardLimits() const;
  bool PrepareTask(const ExecutionTarget &target);
  bool MergeBindingSpec(const ParserBindingSpec &spec);
  bool MergeEvidence(const ParserEvidenceBundle &bundle);
  bool Run(ExecutionResult &result);
  bool Validate(ParserValidationReport &report);
  void *GetClassObject(const std::string &class_name, const std::string &object_name);
  const ParserEvidenceBundle &GetEvidence() const;

private:
  void AppendTrace(const std::string &stage,
                   const std::string &action,
                   const std::string &status,
                   const std::string &detail);
  void AppendLog(const std::string &level,
                 const std::string &stage,
                 const std::string &code,
                 const std::string &message);

  ExecutionTarget target_;
  ParserBindingSpec binding_spec_;
  ParserEvidenceBundle evidence_;
  ExecutionResult last_result_;
  ParserValidationReport last_report_;
  ExecutionGuardContext guard_;
  ParserRuntimeFacade runtime_;
  ParserValidationEngine validation_;
};
}

#endif
