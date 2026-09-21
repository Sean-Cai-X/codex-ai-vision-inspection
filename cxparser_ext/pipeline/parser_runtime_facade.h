#ifndef CXPARSER_EXT_PARSER_RUNTIME_FACADE_H
#define CXPARSER_EXT_PARSER_RUNTIME_FACADE_H

#include <memory>
#include <string>

#include "../meta/parser_binding_spec.h"
#include "../meta/parser_evidence.h"
#include "parser_task_types.h"

namespace mu
{
class Parser;
}

namespace cxparser_ext
{
class ParserRuntimeFacade
{
public:
  ParserRuntimeFacade();
  ~ParserRuntimeFacade();
  ParserRuntimeFacade(const ParserRuntimeFacade &) = delete;
  ParserRuntimeFacade &operator=(const ParserRuntimeFacade &) = delete;
  ParserRuntimeFacade(ParserRuntimeFacade &&) noexcept;
  ParserRuntimeFacade &operator=(ParserRuntimeFacade &&) noexcept;

  void Reset();
  bool LoadBindingSpec(const ParserBindingSpec &spec);
  bool LoadScript(const ExecutionTarget &target);
  bool Execute(ExecutionResult &result);
  bool CollectRuntimeEvidence(ParserEvidenceBundle &bundle);
  void *GetClassObject(const std::string &class_name, const std::string &object_name);

private:
  ParserBindingSpec binding_spec_;
  ExecutionTarget target_;
  std::string script_text_;
  std::unique_ptr<mu::Parser> parser_;
};
}

#endif
